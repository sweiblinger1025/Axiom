/*
 * Program.cs — Axiom Viewer entry point
 *
 * TASK-003: Debug Command Pipeline validation (C# side).
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

using System;
using System.Runtime.InteropServices;

namespace Axiom
{
    internal class Program
    {
        private const int EXIT_SUCCESS = 0;
        private const int EXIT_FAILURE = 1;

        private static int _pass = 0;
        private static int _fail = 0;

        private static void Check(bool condition, string label)
        {
            if (condition)
            {
                Console.WriteLine($"  [PASS] {label}");
                _pass++;
            }
            else
            {
                Console.WriteLine($"  [FAIL] {label}");
                _fail++;
            }
        }

        static int Main(string[] args)
        {
            Console.WriteLine("=== Axiom Viewer -- TASK-003 Validation (C#) ===");
            Console.WriteLine();

            // ── 0. Struct Size Checks (compile-time equivalent) ───
            //
            // C++ uses static_assert; C# validates at runtime.
            // These must match the expected sizes from TASK-003.md
            // and the static_asserts in ax_api.cpp.

            Console.WriteLine("--- Struct Size Checks ---");

            Check(Marshal.SizeOf<AxCmdSetCellU8V1>() == 12,
                  "AxCmdSetCellU8V1 size == 12");
            Check(Marshal.SizeOf<AxCommandV1>() == 40,
                  "AxCommandV1 size == 40");
            Check(Marshal.SizeOf<AxCommandResultV1>() == 32,
                  "AxCommandResultV1 size == 32");

            Console.WriteLine();

            // ── 1. ABI Checks (TASK-000) ──────────────────────────

            Console.WriteLine("--- ABI Checks ---");

            Check(NativeBindings.ax_get_abi_version() == NativeBindings.AX_ABI_VERSION,
                  "ABI version matches");

            uint expectedPacked = (0u << 16) | (0u << 8) | 1u;
            Check(NativeBindings.ax_get_version_packed() == expectedPacked,
                  "Packed version matches");

            IntPtr infoPtr = NativeBindings.ax_get_build_info();
            Check(infoPtr != IntPtr.Zero, "Build info non-null");

            Console.WriteLine();

            // ── 2. World Creation (TASK-001) ──────────────────────

            Console.WriteLine("--- World Creation ---");

            var desc = new AxWorldDesc
            {
                width = 64,
                height = 48,
                reserved = 0
            };

            IntPtr world = NativeBindings.ax_world_create(ref desc);
            Check(world != IntPtr.Zero, "ax_world_create(64x48) succeeds");

            NativeBindings.ax_world_get_size(world, out uint w, out uint h);
            Check(w == 64, "width == 64");
            Check(h == 48, "height == 48");

            Check(NativeBindings.ax_world_get_tick(world) == 0, "initial tick == 0");

            Console.WriteLine();

            // ── 3. Cell Count (TASK-002) ──────────────────────────

            Console.WriteLine("--- Cell Count ---");

            Check(NativeBindings.ax_world_get_cell_count(world) == 64u * 48u,
                  "cell count == 64 * 48");

            Console.WriteLine();

            // ── 4. Tick Advancement (TASK-001) ────────────────────

            Console.WriteLine("--- Tick Advancement ---");

            NativeBindings.ax_world_step(world, 1);
            Check(NativeBindings.ax_world_get_tick(world) == 1, "step(1) -> tick == 1");

            NativeBindings.ax_world_step(world, 9);
            Check(NativeBindings.ax_world_get_tick(world) == 10, "step(9) -> tick == 10");

            Console.WriteLine();

            // ── 5. Snapshot: World Meta (TASK-001) ────────────────

            Console.WriteLine("--- Snapshot (World Meta) ---");

            int metaSize = Marshal.SizeOf<AxWorldMetaSnapshotV1>();

            uint required = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.WorldMeta, IntPtr.Zero, 0);
            Check(required == (uint)metaSize, "size query returns correct size");

            IntPtr metaBuf = Marshal.AllocHGlobal(metaSize);
            try
            {
                for (int i = 0; i < metaSize; i++)
                    Marshal.WriteByte(metaBuf, i, 0);

                uint written = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.WorldMeta, metaBuf, (uint)metaSize);

                Check(written == (uint)metaSize, "read returns correct size");

                var metaSnap = Marshal.PtrToStructure<AxWorldMetaSnapshotV1>(metaBuf);
                Check(metaSnap.version == NativeBindings.AX_WORLD_META_SNAPSHOT_VERSION,
                      "snapshot version correct");
                Check(metaSnap.sizeBytes == (uint)metaSize,
                      "snapshot sizeBytes correct");
                Check(metaSnap.tick == 10, "snapshot tick == 10");
                Check(metaSnap.width == 64, "snapshot width == 64");
                Check(metaSnap.height == 48, "snapshot height == 48");
                Check(metaSnap.reserved == 0, "snapshot reserved == 0");
            }
            finally
            {
                Marshal.FreeHGlobal(metaBuf);
            }

            // Undersized buffer
            IntPtr undersizedBuf = Marshal.AllocHGlobal(metaSize);
            try
            {
                for (int i = 0; i < metaSize; i++)
                    Marshal.WriteByte(undersizedBuf, i, 0xFF);

                uint undersized = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.WorldMeta, undersizedBuf, 1);
                Check(undersized == (uint)metaSize, "undersized buffer returns required size");

                bool untouched = true;
                for (int i = 0; i < metaSize; i++)
                {
                    if (Marshal.ReadByte(undersizedBuf, i) != 0xFF)
                    { untouched = false; break; }
                }
                Check(untouched, "undersized buffer not modified");
            }
            finally
            {
                Marshal.FreeHGlobal(undersizedBuf);
            }

            Console.WriteLine();

            // ── 6. Snapshot: Terrain & Occupancy (TASK-002) ───────

            Console.WriteLine("--- Snapshot (Terrain & Occupancy) ---");

            uint cellCount = NativeBindings.ax_world_get_cell_count(world);

            // Terrain
            uint terrainRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Terrain, IntPtr.Zero, 0);
            Check(terrainRequired == cellCount, "terrain size query == cellCount");

            IntPtr terrainBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                byte[] terrainArr = new byte[cellCount];

                uint terrainWritten = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainBuf, cellCount);
                Check(terrainWritten == cellCount, "terrain read succeeds");

                Marshal.Copy(terrainBuf, terrainArr, 0, (int)cellCount);

                bool allZero = true;
                for (uint i = 0; i < cellCount; i++)
                {
                    if (terrainArr[i] != 0) { allZero = false; break; }
                }
                Check(allZero, "terrain all zeros initially");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainBuf);
            }

            // Occupancy
            uint occRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Occupancy, IntPtr.Zero, 0);
            Check(occRequired == cellCount, "occupancy size query == cellCount");

            IntPtr occBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                byte[] occArr = new byte[cellCount];

                uint occWritten = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Occupancy, occBuf, cellCount);
                Check(occWritten == cellCount, "occupancy read succeeds");

                Marshal.Copy(occBuf, occArr, 0, (int)cellCount);

                bool allZero = true;
                for (uint i = 0; i < cellCount; i++)
                {
                    if (occArr[i] != 0) { allZero = false; break; }
                }
                Check(allZero, "occupancy all zeros initially");
            }
            finally
            {
                Marshal.FreeHGlobal(occBuf);
            }

            Console.WriteLine();

            // ── 7. Command Pipeline (TASK-003) ────────────────────

            Console.WriteLine("--- Command Pipeline ---");

            // -- 7a. Valid set-cell command (terrain) ---------------
            //
            // World is at tick 10. Submit command to set terrain at
            // (3, 2) to value 42. Step once. Verify result accepted,
            // tickApplied==10, and snapshot reflects the change.

            var cmd = default(AxCommandV1);
            cmd.version = 1;
            cmd.type = (uint)AxCommandType.DebugSetCellU8;
            cmd.setCellU8.x = 3;
            cmd.setCellU8.y = 2;
            cmd.setCellU8.channel = (byte)AxSnapshotChannel.Terrain;
            cmd.setCellU8.value = 42;

            ulong id1 = NativeBindings.ax_world_submit_command(world, ref cmd);
            Check(id1 != 0, "valid command -> non-zero ID");

            NativeBindings.ax_world_step(world, 1);
            // tick was 10, commands processed at 10, tick now 11

            Check(NativeBindings.ax_world_get_tick(world) == 11,
                  "tick == 11 after step");

            // Read command result
            int resultStructSize = Marshal.SizeOf<AxCommandResultV1>();

            uint resultRequired = NativeBindings.ax_world_read_command_results(
                world, IntPtr.Zero, 0);
            Check(resultRequired == (uint)resultStructSize,
                  "one result -> correct required size");

            IntPtr resultBuf = Marshal.AllocHGlobal(resultStructSize);
            try
            {
                for (int i = 0; i < resultStructSize; i++)
                    Marshal.WriteByte(resultBuf, i, 0);

                uint resultWritten = NativeBindings.ax_world_read_command_results(
                    world, resultBuf, (uint)resultStructSize);
                Check(resultWritten == (uint)resultStructSize,
                      "result read succeeds");

                var result = Marshal.PtrToStructure<AxCommandResultV1>(resultBuf);
                Check(result.commandId == id1,
                      "result commandId matches submitted ID");
                Check(result.tickApplied == 10,
                      "tickApplied == 10");
                Check(result.type == (uint)AxCommandType.DebugSetCellU8,
                      "result type correct");
                Check(result.accepted == 1,
                      "command accepted");
                Check(result.reason == (byte)AxCommandRejectReason.None,
                      "no reject reason");
            }
            finally
            {
                Marshal.FreeHGlobal(resultBuf);
            }

            // Verify snapshot reflects the mutation
            uint targetIdx = 2 * 64 + 3;  // y * width + x (SPATIAL_MODEL.md)

            terrainBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainBuf, cellCount);

                byte[] terrainArr = new byte[cellCount];
                Marshal.Copy(terrainBuf, terrainArr, 0, (int)cellCount);

                Check(terrainArr[targetIdx] == 42,
                      "terrain(3,2) == 42 after command");

                bool othersUnchanged = true;
                for (uint i = 0; i < cellCount; i++)
                {
                    if (i != targetIdx && terrainArr[i] != 0)
                    { othersUnchanged = false; break; }
                }
                Check(othersUnchanged, "other terrain cells still zero");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainBuf);
            }

            // -- 7b. Results are idempotent within a tick -----------

            resultBuf = Marshal.AllocHGlobal(resultStructSize);
            try
            {
                uint reread = NativeBindings.ax_world_read_command_results(
                    world, resultBuf, (uint)resultStructSize);
                Check(reread == (uint)resultStructSize,
                      "results re-read returns same size");

                var result = Marshal.PtrToStructure<AxCommandResultV1>(resultBuf);
                Check(result.commandId == id1,
                      "re-read commandId still matches");
            }
            finally
            {
                Marshal.FreeHGlobal(resultBuf);
            }

            // -- 7c. Results cleared on next step ------------------

            NativeBindings.ax_world_step(world, 1);
            // tick now 12, no commands queued, results cleared

            uint afterClear = NativeBindings.ax_world_read_command_results(
                world, IntPtr.Zero, 0);
            Check(afterClear == 0,
                  "results cleared after next step");

            Console.WriteLine();

            // ── 8. Command Rejections (TASK-003) ──────────────────

            Console.WriteLine("--- Command Rejections ---");

            // -- 8a. Out-of-bounds coordinates ---------------------

            cmd = default(AxCommandV1);
            cmd.version = 1;
            cmd.type = (uint)AxCommandType.DebugSetCellU8;
            cmd.setCellU8.x = 999;   // out of bounds
            cmd.setCellU8.y = 2;
            cmd.setCellU8.channel = (byte)AxSnapshotChannel.Terrain;
            cmd.setCellU8.value = 7;

            ulong id2 = NativeBindings.ax_world_submit_command(world, ref cmd);
            Check(id2 != 0, "OOB command -> non-zero ID (queued)");
            Check(id2 > id1, "command IDs monotonic (id2 > id1)");

            NativeBindings.ax_world_step(world, 1);
            // tick was 12, processed at 12, tick now 13

            resultBuf = Marshal.AllocHGlobal(resultStructSize);
            try
            {
                NativeBindings.ax_world_read_command_results(
                    world, resultBuf, (uint)resultStructSize);

                var result = Marshal.PtrToStructure<AxCommandResultV1>(resultBuf);
                Check(result.accepted == 0,
                      "OOB command rejected");
                Check(result.reason == (byte)AxCommandRejectReason.InvalidCoords,
                      "reject reason: INVALID_COORDS");
            }
            finally
            {
                Marshal.FreeHGlobal(resultBuf);
            }

            // Verify no mutation
            terrainBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainBuf, cellCount);

                byte[] terrainArr = new byte[cellCount];
                Marshal.Copy(terrainBuf, terrainArr, 0, (int)cellCount);

                Check(terrainArr[targetIdx] == 42,
                      "terrain(3,2) unchanged after OOB reject");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainBuf);
            }

            // -- 8b. Invalid channel -------------------------------

            cmd = default(AxCommandV1);
            cmd.version = 1;
            cmd.type = (uint)AxCommandType.DebugSetCellU8;
            cmd.setCellU8.x = 0;
            cmd.setCellU8.y = 0;
            cmd.setCellU8.channel = 99;    // not terrain or occupancy
            cmd.setCellU8.value = 7;

            ulong id3 = NativeBindings.ax_world_submit_command(world, ref cmd);
            Check(id3 != 0, "invalid-channel command -> non-zero ID");
            Check(id3 > id2, "command IDs monotonic (id3 > id2)");

            NativeBindings.ax_world_step(world, 1);
            // tick was 13, processed at 13, tick now 14

            resultBuf = Marshal.AllocHGlobal(resultStructSize);
            try
            {
                NativeBindings.ax_world_read_command_results(
                    world, resultBuf, (uint)resultStructSize);

                var result = Marshal.PtrToStructure<AxCommandResultV1>(resultBuf);
                Check(result.accepted == 0,
                      "invalid-channel rejected");
                Check(result.reason == (byte)AxCommandRejectReason.InvalidChannel,
                      "reject reason: INVALID_CHANNEL");
            }
            finally
            {
                Marshal.FreeHGlobal(resultBuf);
            }

            Console.WriteLine();

            // ── 9. Structural Failures (TASK-003) ─────────────────

            Console.WriteLine("--- Structural Failures ---");

            // Unknown command type
            cmd = default(AxCommandV1);
            cmd.version = 1;
            cmd.type = 9999;

            Check(NativeBindings.ax_world_submit_command(world, ref cmd) == 0,
                  "unknown type -> submission returns 0");

            // Bad version
            cmd = default(AxCommandV1);
            cmd.version = 99;
            cmd.type = (uint)AxCommandType.DebugSetCellU8;

            Check(NativeBindings.ax_world_submit_command(world, ref cmd) == 0,
                  "bad version -> submission returns 0");

            // Verify no results generated from structural failures
            NativeBindings.ax_world_step(world, 1);
            // tick now 15, no valid commands were queued

            Check(NativeBindings.ax_world_read_command_results(
                      world, IntPtr.Zero, 0) == 0,
                  "no results from structural failures");

            Console.WriteLine();

            // ── 10. Null Safety (cumulative) ──────────────────────

            Console.WriteLine("--- Null Safety ---");

            // -- World creation (TASK-001) --

            Check(NativeBindings.ax_world_create_ptr(IntPtr.Zero) == IntPtr.Zero,
                  "ax_world_create(NULL) -> NULL");

            var badDesc = new AxWorldDesc { width = 0, height = 48, reserved = 0 };
            Check(NativeBindings.ax_world_create(ref badDesc) == IntPtr.Zero,
                  "ax_world_create(0x48) -> NULL");

            badDesc = new AxWorldDesc { width = 64, height = 0, reserved = 0 };
            Check(NativeBindings.ax_world_create(ref badDesc) == IntPtr.Zero,
                  "ax_world_create(64x0) -> NULL");

            // -- Lifecycle (TASK-001) --

            NativeBindings.ax_world_destroy(IntPtr.Zero);
            Check(true, "ax_world_destroy(NULL) no crash");

            NativeBindings.ax_world_step(IntPtr.Zero, 10);
            Check(true, "ax_world_step(NULL, 10) no crash");

            ulong tickBefore = NativeBindings.ax_world_get_tick(world);
            NativeBindings.ax_world_step(world, 0);
            Check(NativeBindings.ax_world_get_tick(world) == tickBefore,
                  "ax_world_step(world, 0) no-op");

            Check(NativeBindings.ax_world_get_tick(IntPtr.Zero) == 0,
                  "ax_world_get_tick(NULL) -> 0");

            NativeBindings.ax_world_get_size(IntPtr.Zero, out _, out _);
            Check(true, "ax_world_get_size(NULL, ...) no crash");

            NativeBindings.ax_world_get_size_ptr(world, IntPtr.Zero, IntPtr.Zero);
            Check(true, "ax_world_get_size(world, NULL, NULL) no crash");

            // -- Cell count (TASK-002) --

            Check(NativeBindings.ax_world_get_cell_count(IntPtr.Zero) == 0,
                  "ax_world_get_cell_count(NULL) -> 0");

            // -- Snapshots (TASK-001) --

            Check(NativeBindings.ax_world_read_snapshot(
                      IntPtr.Zero, AxSnapshotChannel.WorldMeta, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(NULL) -> 0");

            Check(NativeBindings.ax_world_read_snapshot(
                      world, (AxSnapshotChannel)999, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(unknown channel) -> 0");

            // -- Commands (TASK-003) --

            cmd = default(AxCommandV1);
            cmd.version = 1;
            cmd.type = (uint)AxCommandType.DebugSetCellU8;

            Check(NativeBindings.ax_world_submit_command_ptr(IntPtr.Zero, IntPtr.Zero) == 0,
                  "ax_world_submit_command(NULL world) -> 0");

            Check(NativeBindings.ax_world_submit_command_ptr(world, IntPtr.Zero) == 0,
                  "ax_world_submit_command(world, NULL cmd) -> 0");

            Check(NativeBindings.ax_world_read_command_results(
                      IntPtr.Zero, IntPtr.Zero, 0) == 0,
                  "ax_world_read_command_results(NULL world) -> 0");

            Console.WriteLine();

            // ── 11. Cleanup ──────────────────────────────────────

            Console.WriteLine("--- Cleanup ---");

            NativeBindings.ax_world_destroy(world);
            world = IntPtr.Zero;
            Check(true, "ax_world_destroy completed");

            Console.WriteLine();

            // ── Summary ──────────────────────────────────────────

            Console.WriteLine($"=== Results: {_pass} passed, {_fail} failed ===");

            return (_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
}