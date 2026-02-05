/*
 * ax_api.cpp — Axiom C API implementation
 *
 * Implements the public C ABI surface declared in ax_api.h.
 * Binding functions forward into internal C++ interfaces;
 * the C boundary and all input validation live here.
 *
 * See TRUTH_VS_PRESENTATION.md: "The C API is an active
 * contract layer, not a passive passthrough."
 */

#include "ax_api.h"

/* Engine core */
#include "world.h"

/* Standard library */
#include <cstdlib>   // malloc, free
#include <cstdio>    // snprintf
#include <cstring>   // memcpy, memset
#include <new>       // nothrow

/* ── Static Layout Guarantees (TASK-003) ─────────────────────────
 *
 * Byte-level size agreement between C/C++ and C# is mandatory.
 * If any of these fire, a struct has unexpected padding.
 */

static_assert(sizeof(ax_cmd_set_cell_u8_v1) == 12,
              "ax_cmd_set_cell_u8_v1 size mismatch");
static_assert(sizeof(ax_command_v1) == 40,
              "ax_command_v1 size mismatch");
static_assert(sizeof(ax_command_result_v1) == 32,
              "ax_command_result_v1 size mismatch");

/* ── Build configuration ─────────────────────────────────────────
 *
 * AX_BUILD_TYPE is defined by CMake (e.g., "Debug", "Release").
 * Falls back to "Unknown" if not set, so the code compiles
 * even without CMake (e.g., during IDE indexing).
 */

#ifndef AX_BUILD_TYPE
  #define AX_BUILD_TYPE "Unknown"
#endif

#ifndef AX_COMPILER_ID
  #define AX_COMPILER_ID "Unknown"
#endif

#ifndef AX_PLATFORM
  #define AX_PLATFORM "Unknown"
#endif

/* ── Versioning ──────────────────────────────────────────────── */

uint32_t ax_get_abi_version(void)
{
    return AX_ABI_VERSION;
}

uint32_t ax_get_version_packed(void)
{
    return AX_VERSION_PACKED;
}

const char* ax_get_build_info(void)
{
    static char buffer[256] = {0};
    static bool initialized = false;

    if (!initialized) {
        snprintf(buffer, sizeof(buffer),
                 "Axiom %d.%d.%d (ABI %u) [%s] compiler=%s platform=%s built %s %s",
                 AX_VERSION_MAJOR, AX_VERSION_MINOR, AX_VERSION_PATCH,
                 AX_ABI_VERSION,
                 AX_BUILD_TYPE,
                 AX_COMPILER_ID,
                 AX_PLATFORM,
                 __DATE__, __TIME__);
        initialized = true;
    }

    return buffer;
}

/* ── Memory ──────────────────────────────────────────────────── */

void* ax_alloc(size_t size)
{
    if (size == 0) {
        return nullptr;
    }
    return std::malloc(size);
}

void ax_free(void* ptr)
{
    std::free(ptr);
}

/* ── Handle Casting (file-local) ─────────────────────────────── */

static axiom::World* to_world(ax_world_handle h)
{
    return reinterpret_cast<axiom::World*>(h);
}

static ax_world_handle to_handle(axiom::World* w)
{
    return reinterpret_cast<ax_world_handle>(w);
}

/* ── World: Lifecycle ────────────────────────────────────────── */

ax_world_handle ax_world_create(const ax_world_desc* desc)
{
    if (!desc)              return nullptr;
    if (desc->width == 0)   return nullptr;
    if (desc->height == 0)  return nullptr;

    auto* world = new (std::nothrow) axiom::World(desc->width, desc->height);
    return to_handle(world);
}

void ax_world_destroy(ax_world_handle world)
{
    delete to_world(world);
}

void ax_world_step(ax_world_handle world, uint32_t ticks)
{
    if (!world) return;
    if (ticks == 0) return;

    to_world(world)->step(ticks);
}

uint64_t ax_world_get_tick(ax_world_handle world)
{
    if (!world) return 0;

    return to_world(world)->tick();
}

void ax_world_get_size(ax_world_handle world,
                       uint32_t* outWidth, uint32_t* outHeight)
{
    if (!world) return;

    if (outWidth)  *outWidth  = to_world(world)->width();
    if (outHeight) *outHeight = to_world(world)->height();
}

uint32_t ax_world_get_cell_count(ax_world_handle world)
{
    if (!world) return 0;

    return to_world(world)->cellCount();
}

/* ── Snapshots ───────────────────────────────────────────────── */

uint32_t ax_world_read_snapshot(ax_world_handle world,
                                ax_snapshot_channel channel,
                                void* outBuffer,
                                uint32_t outBufferSizeBytes)
{
    if (!world) return 0;

    axiom::World* w = to_world(world);

    switch (channel)
    {
    case AX_SNAP_WORLD_META:
    {
        const uint32_t required = (uint32_t)sizeof(ax_world_meta_snapshot_v1);

        if (!outBuffer || outBufferSizeBytes < required)
            return required;

        ax_world_meta_snapshot_v1 snap;
        std::memset(&snap, 0, sizeof(snap));

        snap.version   = AX_WORLD_META_SNAPSHOT_VERSION;
        snap.sizeBytes = required;
        snap.tick      = w->tick();
        snap.width     = w->width();
        snap.height    = w->height();

        std::memcpy(outBuffer, &snap, required);
        return required;
    }

    case AX_SNAP_TERRAIN:
    {
        const uint32_t required = w->cellCount();
        if (required == 0) return 0;

        if (!outBuffer || outBufferSizeBytes < required)
            return required;

        std::memcpy(outBuffer, w->terrain(), required);
        return required;
    }

    case AX_SNAP_OCCUPANCY:
    {
        const uint32_t required = w->cellCount();
        if (required == 0) return 0;

        if (!outBuffer || outBufferSizeBytes < required)
            return required;

        std::memcpy(outBuffer, w->occupancy(), required);
        return required;
    }

    default:
        return 0;
    }
}

/* ── Commands (TASK-003) ─────────────────────────────────────────
 *
 * Two-phase validation per COMMAND_MODEL.md:
 *
 *   Phase 1 (here, at submission):
 *     Structural checks — null pointers, version, known type.
 *     Failures return 0 and produce NO result record.
 *
 *   Phase 2 (inside World::step, at tick boundary):
 *     State-dependent checks — bounds, channel validity.
 *     Failures produce a result record with accepted=0.
 */

uint64_t ax_world_submit_command(ax_world_handle world,
                                 const ax_command_v1* cmd)
{
    /* ── Structural validation (not queued, no result) ─────── */

    if (!world) return 0;
    if (!cmd)   return 0;

    if (cmd->version != 1) return 0;

    /* Validate known command type.
     * Unknown types are structural failures — the engine
     * doesn't know what payload to expect. */
    switch (cmd->type)
    {
    case AX_CMD_DEBUG_SET_CELL_U8:
        break;  /* known — fall through to queueing */

    default:
        return 0;  /* unknown type — structural failure */
    }

    /* ── Extract payload and forward to core ──────────────── */

    axiom::PendingCommand pcmd{};
    pcmd.type = cmd->type;

    switch (cmd->type)
    {
    case AX_CMD_DEBUG_SET_CELL_U8:
    {
        const ax_cmd_set_cell_u8_v1& p = cmd->payload.setCellU8;
        pcmd.x       = p.x;
        pcmd.y       = p.y;
        pcmd.channel = p.channel;
        pcmd.value   = p.value;
        break;
    }
    }

    return to_world(world)->submitCommand(std::move(pcmd));
}

/* ── Command Results ─────────────────────────────────────────────
 *
 * Caller-allocated buffer pattern (same as snapshots):
 *   - NULL buffer or undersized → return required size
 *   - Sufficient buffer → write results, return bytes written
 *   - 0 on error (null world)
 *
 * Results are an array of ax_command_result_v1 structs.
 * Results remain valid until the next step() call.
 */

uint32_t ax_world_read_command_results(ax_world_handle world,
                                       void* outBuffer,
                                       uint32_t outBufferSizeBytes)
{
    if (!world) return 0;

    const axiom::World* w = to_world(world);
    const auto& results = w->results();

    const uint32_t count = (uint32_t)results.size();
    const uint32_t required = count * (uint32_t)sizeof(ax_command_result_v1);

    /* Size query or insufficient buffer */
    if (!outBuffer || outBufferSizeBytes < required)
        return required;

    /* Translate internal CommandResult → C API ax_command_result_v1 */
    auto* out = static_cast<ax_command_result_v1*>(outBuffer);

    for (uint32_t i = 0; i < count; ++i)
    {
        std::memset(&out[i], 0, sizeof(ax_command_result_v1));

        out[i].sizeBytes   = (uint32_t)sizeof(ax_command_result_v1);
        out[i].version     = 1;
        out[i].commandId   = results[i].commandId;
        out[i].tickApplied = results[i].tickApplied;
        out[i].type        = results[i].type;
        out[i].accepted    = results[i].accepted ? 1 : 0;
        out[i].reason      = results[i].reason;
    }

    return required;
}