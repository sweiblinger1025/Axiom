/*
 * main.cpp — Axiom Headless Runner
 *
 * TASK-001: World lifecycle + tick loop + minimal observation.
 * TASK-002: Spatial grid channels (terrain + occupancy snapshots).
 *
 * Validates:
 *   - ABI compatibility (retained from TASK-000)
 *   - World creation and destruction
 *   - Cell count query
 *   - Tick advancement
 *   - Snapshot read (caller-allocated buffer pattern)
 *   - Terrain and occupancy snapshot channels
 *   - Null-safety of all API functions
 *
 * See TASK-001.md and TASK-002.md for acceptance criteria.
 */

#include "ax_api.h"

#include <cstdint>   // uint8_t
#include <cstdio>
#include <cstdlib>   // EXIT_SUCCESS, EXIT_FAILURE
#include <cstring>   // memset
#include <vector>    // test buffers

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
    std::printf("=== Axiom Headless -- TASK-002 Validation ===\n\n");

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

    /* Cell count (TASK-002) */
    check(ax_world_get_cell_count(world) == 64 * 48,
          "cell count == 3072");

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

    /* ── 4. Snapshot: World Meta (TASK-001) ─────────────────────── */

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

    /* ── 5. Snapshot: Terrain (TASK-002) ───────────────────────── */

    std::printf("--- Snapshot (Terrain) ---\n");

    const uint32_t cellCount = ax_world_get_cell_count(world);

    /* Size query */
    uint32_t terrainRequired = ax_world_read_snapshot(
        world, AX_SNAP_TERRAIN, nullptr, 0);

    check(terrainRequired == cellCount,
          "terrain size query == cellCount");

    /* Full read */
    std::vector<uint8_t> terrainBuf(cellCount, 0xFF);

    uint32_t terrainWritten = ax_world_read_snapshot(
        world, AX_SNAP_TERRAIN, terrainBuf.data(), cellCount);

    check(terrainWritten == cellCount,
          "terrain read returns cellCount bytes");

    /* Verify all zeros (default initialization) */
    bool terrainAllZero = true;
    for (uint32_t i = 0; i < cellCount; ++i) {
        if (terrainBuf[i] != 0) {
            terrainAllZero = false;
            break;
        }
    }
    check(terrainAllZero, "terrain data all zeros");

    /* Undersized buffer returns required size */
    std::vector<uint8_t> terrainSmall(4, 0xFF);

    uint32_t terrainUndersized = ax_world_read_snapshot(
        world, AX_SNAP_TERRAIN, terrainSmall.data(), 4);

    check(terrainUndersized == cellCount,
          "terrain undersized buffer returns required size");

    /* Verify undersized buffer was not modified */
    bool terrainSmallOk = true;
    for (size_t i = 0; i < terrainSmall.size(); ++i) {
        if (terrainSmall[i] != 0xFF) {
            terrainSmallOk = false;
            break;
        }
    }
    check(terrainSmallOk, "terrain undersized buffer not modified");

    std::printf("\n");

    /* ── 6. Snapshot: Occupancy (TASK-002) ─────────────────────── */

    std::printf("--- Snapshot (Occupancy) ---\n");

    /* Size query */
    uint32_t occRequired = ax_world_read_snapshot(
        world, AX_SNAP_OCCUPANCY, nullptr, 0);

    check(occRequired == cellCount,
          "occupancy size query == cellCount");

    /* Full read */
    std::vector<uint8_t> occBuf(cellCount, 0xFF);

    uint32_t occWritten = ax_world_read_snapshot(
        world, AX_SNAP_OCCUPANCY, occBuf.data(), cellCount);

    check(occWritten == cellCount,
          "occupancy read returns cellCount bytes");

    /* Verify all zeros (default initialization) */
    bool occAllZero = true;
    for (uint32_t i = 0; i < cellCount; ++i) {
        if (occBuf[i] != 0) {
            occAllZero = false;
            break;
        }
    }
    check(occAllZero, "occupancy data all zeros");

    /* Undersized buffer returns required size */
    std::vector<uint8_t> occSmall(4, 0xFF);

    uint32_t occUndersized = ax_world_read_snapshot(
        world, AX_SNAP_OCCUPANCY, occSmall.data(), 4);

    check(occUndersized == cellCount,
          "occupancy undersized buffer returns required size");

    /* Verify undersized buffer was not modified */
    bool occSmallOk = true;
    for (size_t i = 0; i < occSmall.size(); ++i) {
        if (occSmall[i] != 0xFF) {
            occSmallOk = false;
            break;
        }
    }
    check(occSmallOk, "occupancy undersized buffer not modified");

    std::printf("\n");

    /* ── 7. Null-safety ────────────────────────────────────────── */

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

    /* Overflow rejection (TASK-002):
     * 65536 * 65536 = 4294967296 which exceeds UINT32_MAX */
    bad_desc.width = 65536; bad_desc.height = 65536;
    check(ax_world_create(&bad_desc) == nullptr,
          "ax_world_create(65536x65536) overflow -> NULL");

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

    /* Cell count null safety (TASK-002) */
    check(ax_world_get_cell_count(nullptr) == 0,
          "ax_world_get_cell_count(NULL) -> 0");

    /* get_size with null handle and null out-pointers */
    ax_world_get_size(nullptr, &w, &h);
    check(true, "ax_world_get_size(NULL, ...) no crash");

    ax_world_get_size(world, nullptr, nullptr);
    check(true, "ax_world_get_size(world, NULL, NULL) no crash");

    /* Snapshot on null world */
    check(ax_world_read_snapshot(nullptr, AX_SNAP_WORLD_META, nullptr, 0) == 0,
          "ax_world_read_snapshot(NULL) -> 0");

    /* Snapshot on null world for new channels (TASK-002) */
    check(ax_world_read_snapshot(nullptr, AX_SNAP_TERRAIN, nullptr, 0) == 0,
          "ax_world_read_snapshot(NULL, TERRAIN) -> 0");

    check(ax_world_read_snapshot(nullptr, AX_SNAP_OCCUPANCY, nullptr, 0) == 0,
          "ax_world_read_snapshot(NULL, OCCUPANCY) -> 0");

    /* Unknown channel */
    check(ax_world_read_snapshot(world, (ax_snapshot_channel)999, nullptr, 0) == 0,
          "ax_world_read_snapshot(unknown channel) -> 0");

    std::printf("\n");

    /* ── 8. Cleanup ────────────────────────────────────────────── */

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