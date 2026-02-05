/*
 * main.cpp — Axiom Headless Runner
 *
 * TASK-003: Debug Command Pipeline validation.
 *
 * Validates (cumulative — includes all prior task checks):
 *   TASK-000: ABI compatibility
 *   TASK-001: World lifecycle, tick advancement, snapshots
 *   TASK-002: Cell count, terrain/occupancy grid snapshots
 *   TASK-003: Command submission, tick-boundary processing,
 *             results, rejections, structural failures
 *
 * See TASK-003.md for acceptance criteria.
 */

#include "ax_api.h"

#include <cstdio>
#include <cstdlib>   // EXIT_SUCCESS, EXIT_FAILURE, malloc, free
#include <cstring>   // memset

/* ── Static Layout Guarantees (TASK-003) ─────────────────────────
 *
 * Consumer-side size validation. These must agree with the
 * static_asserts in ax_api.cpp. If either side fires, struct
 * padding differs between compilation units.
 */

static_assert(sizeof(ax_cmd_set_cell_u8_v1) == 12,
              "ax_cmd_set_cell_u8_v1 size mismatch");
static_assert(sizeof(ax_command_v1) == 40,
              "ax_command_v1 size mismatch");
static_assert(sizeof(ax_command_result_v1) == 32,
              "ax_command_result_v1 size mismatch");

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
    std::printf("=== Axiom Headless -- TASK-003 Validation ===\n\n");

    /* ── 1. ABI compatibility (TASK-000) ───────────────────────── */

    std::printf("--- ABI Checks ---\n");

    check(ax_get_abi_version() == AX_ABI_VERSION,
          "ABI version matches");

    check(ax_get_version_packed() == AX_VERSION_PACKED,
          "Packed version matches");

    check(ax_get_build_info() != nullptr,
          "Build info non-null");

    std::printf("\n");

    /* ── 2. World creation (TASK-001) ──────────────────────────── */

    std::printf("--- World Creation ---\n");

    ax_world_desc desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.width  = 64;
    desc.height = 48;

    ax_world_handle world = ax_world_create(&desc);
    check(world != nullptr, "ax_world_create(64x48) succeeds");

    uint32_t w = 0, h = 0;
    ax_world_get_size(world, &w, &h);
    check(w == 64, "width == 64");
    check(h == 48, "height == 48");

    check(ax_world_get_tick(world) == 0, "initial tick == 0");

    std::printf("\n");

    /* ── 3. Cell count (TASK-002) ──────────────────────────────── */

    std::printf("--- Cell Count ---\n");

    check(ax_world_get_cell_count(world) == 64 * 48,
          "cell count == 64 * 48");

    std::printf("\n");

    /* ── 4. Tick advancement (TASK-001) ────────────────────────── */

    std::printf("--- Tick Advancement ---\n");

    ax_world_step(world, 1);
    check(ax_world_get_tick(world) == 1, "step(1) -> tick == 1");

    ax_world_step(world, 9);
    check(ax_world_get_tick(world) == 10, "step(9) -> tick == 10");

    std::printf("\n");

    /* ── 5. Snapshot: World Meta (TASK-001) ────────────────────── */

    std::printf("--- Snapshot (World Meta) ---\n");

    uint32_t required = ax_world_read_snapshot(
        world, AX_SNAP_WORLD_META, nullptr, 0);

    check(required == sizeof(ax_world_meta_snapshot_v1),
          "size query returns correct size");

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
    check(snap.tick == 10, "snapshot tick == 10");
    check(snap.width == 64, "snapshot width == 64");
    check(snap.height == 48, "snapshot height == 48");
    check(snap.reserved == 0, "snapshot reserved == 0");

    /* Undersized buffer */
    ax_world_meta_snapshot_v1 untouched;
    std::memset(&untouched, 0xFF, sizeof(untouched));

    uint32_t undersized = ax_world_read_snapshot(
        world, AX_SNAP_WORLD_META, &untouched, 1);

    check(undersized == sizeof(ax_world_meta_snapshot_v1),
          "undersized buffer returns required size");

    bool untouched_ok = true;
    auto* bytes = reinterpret_cast<unsigned char*>(&untouched);
    for (size_t i = 0; i < sizeof(untouched); ++i) {
        if (bytes[i] != 0xFF) { untouched_ok = false; break; }
    }
    check(untouched_ok, "undersized buffer not modified");

    std::printf("\n");

    /* ── 6. Snapshot: Terrain & Occupancy (TASK-002) ───────────── */

    std::printf("--- Snapshot (Terrain & Occupancy) ---\n");

    const uint32_t cellCount = ax_world_get_cell_count(world);

    /* Terrain */
    uint32_t terrainRequired = ax_world_read_snapshot(
        world, AX_SNAP_TERRAIN, nullptr, 0);
    check(terrainRequired == cellCount,
          "terrain size query == cellCount");

    auto* terrain = static_cast<uint8_t*>(std::malloc(cellCount));
    std::memset(terrain, 0xFF, cellCount);

    written = ax_world_read_snapshot(
        world, AX_SNAP_TERRAIN, terrain, cellCount);
    check(written == cellCount, "terrain read succeeds");

    bool allZero = true;
    for (uint32_t i = 0; i < cellCount; ++i) {
        if (terrain[i] != 0) { allZero = false; break; }
    }
    check(allZero, "terrain all zeros initially");
    std::free(terrain);

    /* Occupancy */
    uint32_t occRequired = ax_world_read_snapshot(
        world, AX_SNAP_OCCUPANCY, nullptr, 0);
    check(occRequired == cellCount,
          "occupancy size query == cellCount");

    auto* occupancy = static_cast<uint8_t*>(std::malloc(cellCount));
    std::memset(occupancy, 0xFF, cellCount);

    written = ax_world_read_snapshot(
        world, AX_SNAP_OCCUPANCY, occupancy, cellCount);
    check(written == cellCount, "occupancy read succeeds");

    allZero = true;
    for (uint32_t i = 0; i < cellCount; ++i) {
        if (occupancy[i] != 0) { allZero = false; break; }
    }
    check(allZero, "occupancy all zeros initially");
    std::free(occupancy);

    std::printf("\n");

    /* ── 7. Command Pipeline (TASK-003) ────────────────────────── */

    std::printf("--- Command Pipeline ---\n");

    /* -- 7a. Valid set-cell command (terrain) ------------------- */
    /*
     * World is at tick 10. Submit a command to set terrain at
     * (3, 2) to value 42. Step once. Verify result accepted,
     * tickApplied==10, and snapshot reflects the change.
     *
     * Per COMMAND_MODEL.md: commands are processed sequentially
     * at tick boundaries. tickApplied = m_tick before increment.
     */

    ax_command_v1 cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    cmd.type    = AX_CMD_DEBUG_SET_CELL_U8;
    cmd.payload.setCellU8.x       = 3;
    cmd.payload.setCellU8.y       = 2;
    cmd.payload.setCellU8.channel = AX_SNAP_TERRAIN;
    cmd.payload.setCellU8.value   = 42;

    uint64_t id1 = ax_world_submit_command(world, &cmd);
    check(id1 != 0, "valid command -> non-zero ID");

    ax_world_step(world, 1);
    /* tick was 10, commands processed at 10, tick now 11 */

    check(ax_world_get_tick(world) == 11, "tick == 11 after step");

    /* Read command result */
    uint32_t resultRequired = ax_world_read_command_results(
        world, nullptr, 0);
    check(resultRequired == sizeof(ax_command_result_v1),
          "one result -> correct required size");

    ax_command_result_v1 result;
    std::memset(&result, 0, sizeof(result));

    uint32_t resultWritten = ax_world_read_command_results(
        world, &result, sizeof(result));
    check(resultWritten == sizeof(ax_command_result_v1),
          "result read succeeds");
    check(result.commandId == id1,
          "result commandId matches submitted ID");
    check(result.tickApplied == 10,
          "tickApplied == 10");
    check(result.type == AX_CMD_DEBUG_SET_CELL_U8,
          "result type correct");
    check(result.accepted == 1,
          "command accepted");
    check(result.reason == AX_CMD_REJECT_NONE,
          "no reject reason");

    /* Verify snapshot reflects the mutation */
    const uint32_t targetIdx = 2 * 64 + 3;  /* y * width + x */

    terrain = static_cast<uint8_t*>(std::malloc(cellCount));
    ax_world_read_snapshot(world, AX_SNAP_TERRAIN, terrain, cellCount);

    check(terrain[targetIdx] == 42,
          "terrain(3,2) == 42 after command");

    bool othersUnchanged = true;
    for (uint32_t i = 0; i < cellCount; ++i) {
        if (i != targetIdx && terrain[i] != 0) {
            othersUnchanged = false;
            break;
        }
    }
    check(othersUnchanged, "other terrain cells still zero");
    std::free(terrain);

    /* -- 7b. Results are idempotent within a tick --------------- */

    std::memset(&result, 0, sizeof(result));
    uint32_t reread = ax_world_read_command_results(
        world, &result, sizeof(result));
    check(reread == sizeof(ax_command_result_v1),
          "results re-read returns same size");
    check(result.commandId == id1,
          "re-read commandId still matches");

    /* -- 7c. Results cleared on next step ---------------------- */

    ax_world_step(world, 1);
    /* tick now 12, no commands queued, results cleared */

    uint32_t afterClear = ax_world_read_command_results(
        world, nullptr, 0);
    check(afterClear == 0,
          "results cleared after next step");

    std::printf("\n");

    /* ── 8. Command Rejections (TASK-003) ──────────────────────── */

    std::printf("--- Command Rejections ---\n");

    /* -- 8a. Out-of-bounds coordinates -------------------------- */

    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    cmd.type    = AX_CMD_DEBUG_SET_CELL_U8;
    cmd.payload.setCellU8.x       = 999;   /* out of bounds */
    cmd.payload.setCellU8.y       = 2;
    cmd.payload.setCellU8.channel = AX_SNAP_TERRAIN;
    cmd.payload.setCellU8.value   = 7;

    uint64_t id2 = ax_world_submit_command(world, &cmd);
    check(id2 != 0, "OOB command -> non-zero ID (queued)");
    check(id2 > id1, "command IDs monotonic (id2 > id1)");

    ax_world_step(world, 1);
    /* tick was 12, processed at 12, tick now 13 */

    ax_world_read_command_results(world, &result, sizeof(result));
    check(result.accepted == 0,
          "OOB command rejected");
    check(result.reason == AX_CMD_REJECT_INVALID_COORDS,
          "reject reason: INVALID_COORDS");

    /* Verify no mutation */
    terrain = static_cast<uint8_t*>(std::malloc(cellCount));
    ax_world_read_snapshot(world, AX_SNAP_TERRAIN, terrain, cellCount);
    check(terrain[targetIdx] == 42,
          "terrain(3,2) unchanged after OOB reject");
    std::free(terrain);

    /* -- 8b. Invalid channel ------------------------------------ */

    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    cmd.type    = AX_CMD_DEBUG_SET_CELL_U8;
    cmd.payload.setCellU8.x       = 0;
    cmd.payload.setCellU8.y       = 0;
    cmd.payload.setCellU8.channel = 99;    /* not terrain or occupancy */
    cmd.payload.setCellU8.value   = 7;

    uint64_t id3 = ax_world_submit_command(world, &cmd);
    check(id3 != 0, "invalid-channel command -> non-zero ID");
    check(id3 > id2, "command IDs monotonic (id3 > id2)");

    ax_world_step(world, 1);
    /* tick was 13, processed at 13, tick now 14 */

    ax_world_read_command_results(world, &result, sizeof(result));
    check(result.accepted == 0,
          "invalid-channel rejected");
    check(result.reason == AX_CMD_REJECT_INVALID_CHANNEL,
          "reject reason: INVALID_CHANNEL");

    std::printf("\n");

    /* ── 9. Structural Failures (TASK-003) ─────────────────────── */

    std::printf("--- Structural Failures ---\n");

    /* Unknown command type */
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    cmd.type    = 9999;

    check(ax_world_submit_command(world, &cmd) == 0,
          "unknown type -> submission returns 0");

    /* Bad version */
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 99;
    cmd.type    = AX_CMD_DEBUG_SET_CELL_U8;

    check(ax_world_submit_command(world, &cmd) == 0,
          "bad version -> submission returns 0");

    /* Verify no results generated from structural failures */
    ax_world_step(world, 1);
    /* tick now 15, no valid commands were queued */

    check(ax_world_read_command_results(world, nullptr, 0) == 0,
          "no results from structural failures");

    std::printf("\n");

    /* ── 10. Null Safety (cumulative) ──────────────────────────── */

    std::printf("--- Null Safety ---\n");

    /* -- World creation (TASK-001) -- */

    check(ax_world_create(nullptr) == nullptr,
          "ax_world_create(NULL) -> NULL");

    ax_world_desc bad_desc;
    std::memset(&bad_desc, 0, sizeof(bad_desc));

    bad_desc.width = 0; bad_desc.height = 48;
    check(ax_world_create(&bad_desc) == nullptr,
          "ax_world_create(0x48) -> NULL");

    bad_desc.width = 64; bad_desc.height = 0;
    check(ax_world_create(&bad_desc) == nullptr,
          "ax_world_create(64x0) -> NULL");

    /* -- Lifecycle (TASK-001) -- */

    ax_world_destroy(nullptr);
    check(true, "ax_world_destroy(NULL) no crash");

    ax_world_step(nullptr, 10);
    check(true, "ax_world_step(NULL, 10) no crash");

    uint64_t tick_before = ax_world_get_tick(world);
    ax_world_step(world, 0);
    check(ax_world_get_tick(world) == tick_before,
          "ax_world_step(world, 0) no-op");

    check(ax_world_get_tick(nullptr) == 0,
          "ax_world_get_tick(NULL) -> 0");

    ax_world_get_size(nullptr, &w, &h);
    check(true, "ax_world_get_size(NULL, ...) no crash");

    ax_world_get_size(world, nullptr, nullptr);
    check(true, "ax_world_get_size(world, NULL, NULL) no crash");

    /* -- Cell count (TASK-002) -- */

    check(ax_world_get_cell_count(nullptr) == 0,
          "ax_world_get_cell_count(NULL) -> 0");

    /* -- Snapshots (TASK-001) -- */

    check(ax_world_read_snapshot(
              nullptr, AX_SNAP_WORLD_META, nullptr, 0) == 0,
          "ax_world_read_snapshot(NULL) -> 0");

    check(ax_world_read_snapshot(
              world, (ax_snapshot_channel)999, nullptr, 0) == 0,
          "ax_world_read_snapshot(unknown channel) -> 0");

    /* -- Commands (TASK-003) -- */

    std::memset(&cmd, 0, sizeof(cmd));
    cmd.version = 1;
    cmd.type = AX_CMD_DEBUG_SET_CELL_U8;

    check(ax_world_submit_command(nullptr, &cmd) == 0,
          "ax_world_submit_command(NULL world) -> 0");

    check(ax_world_submit_command(world, nullptr) == 0,
          "ax_world_submit_command(world, NULL cmd) -> 0");

    check(ax_world_read_command_results(nullptr, nullptr, 0) == 0,
          "ax_world_read_command_results(NULL world) -> 0");

    std::printf("\n");

    /* ── 11. Cleanup ───────────────────────────────────────────── */

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