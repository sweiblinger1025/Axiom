/*
 * main.cpp — Axiom Headless Runner
 *
 * TASK-001: World lifecycle + tick loop + minimal observation.
 *
 * Validates:
 *   - ABI compatibility (retained from TASK-000)
 *   - World creation and destruction
 *   - Tick advancement
 *   - Snapshot read (caller-allocated buffer pattern)
 *   - Null-safety of all API functions
 *
 * See TASK-001.md for acceptance criteria.
 */

#include "ax_api.h"

#include <cstdio>
#include <cstdlib>   // EXIT_SUCCESS, EXIT_FAILURE
#include <cstring>   // memset

/* ── Test helper ─────────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* label)
{
    if (condition) {
        std::printf("  [PASS] %s\n", label);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", label);
        ++g_fail;
    }
}

/* ── Main ────────────────────────────────────────────────────── */

int main()
{
    std::printf("=== Axiom Headless -- TASK-001 Validation ===\n\n");

    /* ── 1. ABI compatibility (from TASK-000) ──────────────────── */

    std::printf("--- ABI Checks ---\n");

    check(ax_get_abi_version() == AX_ABI_VERSION,
          "ABI version matches");

    check(ax_get_version_packed() == AX_VERSION_PACKED,
          "Packed version matches");

    check(ax_get_build_info() != nullptr,
          "Build info non-null");

    std::printf("\n");

    /* ── 2. World creation ─────────────────────────────────────── */

    std::printf("--- World Creation ---\n");

    ax_world_desc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.width  = 64;
    desc.height = 48;

    ax_world_handle world = ax_world_create(&desc);
    check(world != nullptr, "ax_world_create(64x48) succeeds");

    /* Query dimensions */
    uint32_t w = 0, h = 0;
    ax_world_get_size(world, &w, &h);
    check(w == 64, "width == 64");
    check(h == 48, "height == 48");

    /* Initial tick */
    check(ax_world_get_tick(world) == 0, "initial tick == 0");

    std::printf("\n");

    /* ── 3. Tick advancement ───────────────────────────────────── */

    std::printf("--- Tick Advancement ---\n");

    ax_world_step(world, 1);
    check(ax_world_get_tick(world) == 1, "step(1) -> tick == 1");

    ax_world_step(world, 9);
    check(ax_world_get_tick(world) == 10, "step(9) -> tick == 10");

    std::printf("\n");

    /* ── 4. Snapshot read ──────────────────────────────────────── */

    std::printf("--- Snapshot (World Meta) ---\n");

    /* Phase 1: query required size */
    uint32_t required = ax_world_read_snapshot(
        world, AX_SNAP_WORLD_META, nullptr, 0);

    check(required == sizeof(ax_world_meta_snapshot_v1),
          "size query returns correct size");

    /* Phase 2: read into buffer */
    ax_world_meta_snapshot_v1 snap;
    std::memset(&snap, 0, sizeof(snap));

    uint32_t written = ax_world_read_snapshot(
        world, AX_SNAP_WORLD_META, &snap, sizeof(snap));

    check(written == sizeof(ax_world_meta_snapshot_v1),
          "read returns correct size");

    check(snap.version == AX_WORLD_META_SNAPSHOT_VERSION,
          "snapshot version correct");

    check(snap.sizeBytes == sizeof(ax_world_meta_snapshot_v1),
          "snapshot sizeBytes correct");

    check(snap.tick == 10,
          "snapshot tick == 10");

    check(snap.width == 64,
          "snapshot width == 64");

    check(snap.height == 48,
          "snapshot height == 48");

    check(snap.reserved == 0,
          "snapshot reserved == 0");

    /* Undersized buffer returns required size without writing */
    ax_world_meta_snapshot_v1 untouched;
    std::memset(&untouched, 0xFF, sizeof(untouched));

    uint32_t undersized = ax_world_read_snapshot(
        world, AX_SNAP_WORLD_META, &untouched, 1);

    check(undersized == sizeof(ax_world_meta_snapshot_v1),
          "undersized buffer returns required size");

    /* Verify the buffer was not written to */
    unsigned char expected_byte = 0xFF;
    bool untouched_ok = true;
    auto* bytes = reinterpret_cast<unsigned char*>(&untouched);
    for (size_t i = 0; i < sizeof(untouched); ++i) {
        if (bytes[i] != expected_byte) {
            untouched_ok = false;
            break;
        }
    }
    check(untouched_ok, "undersized buffer not modified");

    std::printf("\n");

    /* ── 5. Null-safety ────────────────────────────────────────── */

    std::printf("--- Null Safety ---\n");

    /* Create with null desc */
    check(ax_world_create(nullptr) == nullptr,
          "ax_world_create(NULL) -> NULL");

    /* Create with zero dimensions */
    ax_world_desc bad_desc;
    std::memset(&bad_desc, 0, sizeof(bad_desc));

    bad_desc.width = 0; bad_desc.height = 48;
    check(ax_world_create(&bad_desc) == nullptr,
          "ax_world_create(0x48) -> NULL");

    bad_desc.width = 64; bad_desc.height = 0;
    check(ax_world_create(&bad_desc) == nullptr,
          "ax_world_create(64x0) -> NULL");

    /* Operations on null handle (must not crash) */
    ax_world_destroy(nullptr);
    check(true, "ax_world_destroy(NULL) no crash");

    ax_world_step(nullptr, 10);
    check(true, "ax_world_step(NULL, 10) no crash");

    /* step with ticks == 0 (must not crash, must not advance) */
    uint64_t tick_before = ax_world_get_tick(world);
    ax_world_step(world, 0);
    check(ax_world_get_tick(world) == tick_before,
          "ax_world_step(world, 0) no-op");

    check(ax_world_get_tick(nullptr) == 0,
          "ax_world_get_tick(NULL) -> 0");

    /* get_size with null handle and null out-pointers */
    ax_world_get_size(nullptr, &w, &h);
    check(true, "ax_world_get_size(NULL, ...) no crash");

    ax_world_get_size(world, nullptr, nullptr);
    check(true, "ax_world_get_size(world, NULL, NULL) no crash");

    /* Snapshot on null world */
    check(ax_world_read_snapshot(nullptr, AX_SNAP_WORLD_META, nullptr, 0) == 0,
          "ax_world_read_snapshot(NULL) -> 0");

    /* Unknown channel */
    check(ax_world_read_snapshot(world, (ax_snapshot_channel)999, nullptr, 0) == 0,
          "ax_world_read_snapshot(unknown channel) -> 0");

    std::printf("\n");

    /* ── 6. Cleanup ────────────────────────────────────────────── */

    std::printf("--- Cleanup ---\n");

    ax_world_destroy(world);
    world = nullptr;
    check(true, "ax_world_destroy completed");

    std::printf("\n");

    /* ── Summary ─────────────────────────────────────────────── */

    std::printf("=== Results: %d passed, %d failed ===\n",
                g_pass, g_fail);

    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}