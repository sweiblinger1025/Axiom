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
    /*
     * Static buffer, built once on first call.
     * Lifetime = library lifetime.
     * Thread safety: benign race — worst case is redundant init
     * with identical content (all inputs are compile-time constants).
     */
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
    std::free(ptr);  /* free(nullptr) is safe per the C standard */
}

/* ── Handle Casting (file-local) ─────────────────────────────────
 *
 * ax_world_handle is an opaque pointer to an incomplete type.
 * These helpers translate between the C handle and the real C++
 * object. All C API functions go through these — no raw casts
 * elsewhere in this file.
 */

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
    return to_handle(world);  /* nullptr on allocation failure */
}

void ax_world_destroy(ax_world_handle world)
{
    delete to_world(world);  /* delete nullptr is safe per C++ standard */
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

/* ── Snapshots ───────────────────────────────────────────────────
 *
 * Caller-allocated buffer pattern:
 *
 *   1. Call with outBuffer == NULL or outBufferSizeBytes too small
 *      → returns the required size in bytes (no data written)
 *
 *   2. Call with a sufficiently sized buffer
 *      → writes the snapshot struct, returns bytes written
 *
 * Returns 0 on error (NULL world, unknown channel).
 *
 * See SNAPSHOT_EVENT_FORMATS.md for the full contract.
 */

uint32_t ax_world_read_snapshot(ax_world_handle world,
                                ax_snapshot_channel channel,
                                void* outBuffer,
                                uint32_t outBufferSizeBytes)
{
    if (!world) return 0;

    switch (channel)
    {
    case AX_SNAP_WORLD_META:
    {
        const uint32_t required = (uint32_t)sizeof(ax_world_meta_snapshot_v1);

        /* Size query or insufficient buffer → return required size */
        if (!outBuffer || outBufferSizeBytes < required)
            return required;

        /* Populate snapshot from current world state */
        axiom::World* w = to_world(world);

        ax_world_meta_snapshot_v1 snap;
        std::memset(&snap, 0, sizeof(snap));

        snap.version   = AX_WORLD_META_SNAPSHOT_VERSION;
        snap.sizeBytes = required;
        snap.tick      = w->tick();
        snap.width     = w->width();
        snap.height    = w->height();
        /* snap.reserved is already 0 from memset */

        std::memcpy(outBuffer, &snap, required);
        return required;
    }

    default:
        return 0;  /* Unknown channel */
    }
}