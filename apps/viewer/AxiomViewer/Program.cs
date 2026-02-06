/*
 * Program.cs — Axiom Viewer entry point
 *
 * Validates the C API from managed code via P/Invoke.
 *
 * TASK-001: World lifecycle + tick loop + minimal observation
 * TASK-002: Spatial grid allocation + terrain/occupancy snapshots
 * TASK-003: Debug command pipeline
 * TASK-004: Math utilities + cross-language determinism
 *
 * See task specs for acceptance criteria.
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
            Console.WriteLine("=== Axiom Viewer -- Validation (C#) ===");
            Console.WriteLine();

            // ── 1. ABI Checks ─────────────────────────────────────

            Console.WriteLine("--- ABI Checks ---");

            Check(NativeBindings.ax_get_abi_version() == NativeBindings.AX_ABI_VERSION,
                  "ABI version matches");

            // Packed version: 0.0.1 → 0x00000001
            uint expectedPacked = (0u << 16) | (0u << 8) | 1u;
            Check(NativeBindings.ax_get_version_packed() == expectedPacked,
                  "Packed version matches");

            IntPtr infoPtr = NativeBindings.ax_get_build_info();
            Check(infoPtr != IntPtr.Zero, "Build info non-null");

            Console.WriteLine();

            // ── 2. Struct Size Validation ─────────────────────────

            Console.WriteLine("--- Struct Size Validation ---");

            Check(Marshal.SizeOf<AxCmdSetCellU8V1>() == NativeBindings.ExpectedCmdSetCellU8V1Size,
                  $"AxCmdSetCellU8V1 size == {NativeBindings.ExpectedCmdSetCellU8V1Size}");

            Check(Marshal.SizeOf<AxCommandV1>() == NativeBindings.ExpectedCommandV1Size,
                  $"AxCommandV1 size == {NativeBindings.ExpectedCommandV1Size}");

            Check(Marshal.SizeOf<AxCommandResultV1>() == NativeBindings.ExpectedCommandResultV1Size,
                  $"AxCommandResultV1 size == {NativeBindings.ExpectedCommandResultV1Size}");

            Console.WriteLine();

            // ── 3. World Creation ─────────────────────────────────

            Console.WriteLine("--- World Creation ---");

            var desc = new AxWorldDesc
            {
                width = 64,
                height = 48,
                reserved = 0
            };

            IntPtr world = NativeBindings.ax_world_create(ref desc);
            Check(world != IntPtr.Zero, "ax_world_create(64x48) succeeds");

            // Query dimensions
            NativeBindings.ax_world_get_size(world, out uint w, out uint h);
            Check(w == 64, "width == 64");
            Check(h == 48, "height == 48");

            // Cell count
            Check(NativeBindings.ax_world_get_cell_count(world) == 64 * 48,
                  "cell count == 3072");

            // Initial tick
            Check(NativeBindings.ax_world_get_tick(world) == 0, "initial tick == 0");

            Console.WriteLine();

            // ── 4. Snapshot Reads ─────────────────────────────────

            Console.WriteLine("--- Snapshot Reads ---");

            // World meta snapshot
            int metaSize = Marshal.SizeOf<AxWorldMetaSnapshotV1>();
            uint metaRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.WorldMeta, IntPtr.Zero, 0);
            Check(metaRequired == (uint)metaSize, "world meta size query correct");

            // Terrain snapshot
            uint terrainRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Terrain, IntPtr.Zero, 0);
            Check(terrainRequired == 64 * 48, "terrain size == cell count");

            // Occupancy snapshot
            uint occupancyRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Occupancy, IntPtr.Zero, 0);
            Check(occupancyRequired == 64 * 48, "occupancy size == cell count");

            // Read terrain and verify all zeros
            IntPtr terrainBuf = Marshal.AllocHGlobal((int)terrainRequired);
            try
            {
                uint terrainWritten = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainBuf, terrainRequired);
                Check(terrainWritten == terrainRequired, "terrain read succeeds");

                bool allZero = true;
                for (int i = 0; i < (int)terrainRequired; i++)
                {
                    if (Marshal.ReadByte(terrainBuf, i) != 0)
                    {
                        allZero = false;
                        break;
                    }
                }
                Check(allZero, "terrain initially all zeros");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainBuf);
            }

            Console.WriteLine();

            // ── 5. Command Pipeline ───────────────────────────────

            Console.WriteLine("--- Command Pipeline ---");

            // Submit valid command
            var cmd = new AxCommandV1
            {
                version = 1,
                type = (uint)AxCommandType.DebugSetCellU8
            };
            cmd.setCellU8.x = 10;
            cmd.setCellU8.y = 20;
            cmd.setCellU8.channel = (byte)AxSnapshotChannel.Terrain;
            cmd.setCellU8.value = 42;

            ulong cmdId1 = NativeBindings.ax_world_submit_command(world, ref cmd);
            Check(cmdId1 != 0, "submit valid command returns non-zero ID");

            // Step to process
            NativeBindings.ax_world_step(world, 1);

            // Read results
            int resultSize = Marshal.SizeOf<AxCommandResultV1>();
            uint resultsRequired = NativeBindings.ax_world_read_command_results(
                world, IntPtr.Zero, 0);
            Check(resultsRequired == (uint)resultSize, "one result available");

            IntPtr resultBuf = Marshal.AllocHGlobal(resultSize);
            try
            {
                uint resultsWritten = NativeBindings.ax_world_read_command_results(
                    world, resultBuf, (uint)resultSize);
                Check(resultsWritten == (uint)resultSize, "result read succeeds");

                var result = Marshal.PtrToStructure<AxCommandResultV1>(resultBuf);
                Check(result.commandId == cmdId1, "result commandId matches");
                Check(result.accepted == 1, "command accepted");
                Check(result.tickApplied == 0, "tickApplied == 0");
            }
            finally
            {
                Marshal.FreeHGlobal(resultBuf);
            }

            // Verify mutation via snapshot
            IntPtr verifyBuf = Marshal.AllocHGlobal((int)terrainRequired);
            try
            {
                NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, verifyBuf, terrainRequired);

                // Index = y * width + x = 20 * 64 + 10 = 1290
                int idx = 20 * 64 + 10;
                byte val = Marshal.ReadByte(verifyBuf, idx);
                Check(val == 42, "terrain[10,20] == 42 after command");
            }
            finally
            {
                Marshal.FreeHGlobal(verifyBuf);
            }

            // Test rejection: out of bounds
            cmd.setCellU8.x = 9999;
            cmd.setCellU8.y = 9999;
            ulong cmdId2 = NativeBindings.ax_world_submit_command(world, ref cmd);
            Check(cmdId2 != 0, "out-of-bounds command gets ID");
            Check(cmdId2 > cmdId1, "command IDs monotonic");

            NativeBindings.ax_world_step(world, 1);

            IntPtr rejectBuf = Marshal.AllocHGlobal(resultSize);
            try
            {
                NativeBindings.ax_world_read_command_results(
                    world, rejectBuf, (uint)resultSize);
                var rejectResult = Marshal.PtrToStructure<AxCommandResultV1>(rejectBuf);
                Check(rejectResult.accepted == 0, "out-of-bounds rejected");
                Check(rejectResult.reason == AxCommandRejectReason.InvalidCoords,
                      "reject reason == InvalidCoords");
            }
            finally
            {
                Marshal.FreeHGlobal(rejectBuf);
            }

            // Test structural failure: unknown type
            var badCmd = new AxCommandV1
            {
                version = 1,
                type = 99999
            };
            ulong badCmdId = NativeBindings.ax_world_submit_command(world, ref badCmd);
            Check(badCmdId == 0, "unknown command type returns 0");

            Console.WriteLine();

            // ── 6. Math Utilities (TASK-004) ──────────────────────

            Console.WriteLine("--- Math Utilities (TASK-004) ---");

            // Version check
            Check(NativeBindings.ax_math_get_version() == NativeBindings.AX_MATH_VERSION,
                  "ax_math_get_version() == AX_MATH_VERSION");

            // Canonical selftest
            ulong checksum0 = NativeBindings.ax_math_selftest_checksum(0);
            Check(checksum0 != 0, "selftest checksum(0) non-zero");

            // Print checksum for cross-language comparison
            Console.WriteLine($"       checksum(0) = 0x{checksum0:X16}");

            // Determinism: same seed → same result
            ulong checksum0Again = NativeBindings.ax_math_selftest_checksum(0);
            Check(checksum0 == checksum0Again, "selftest checksum(0) deterministic");

            // Different seeds → different results
            ulong checksum1 = NativeBindings.ax_math_selftest_checksum(1);
            ulong checksum2 = NativeBindings.ax_math_selftest_checksum(2);
            ulong checksum42 = NativeBindings.ax_math_selftest_checksum(42);
            ulong checksum1000 = NativeBindings.ax_math_selftest_checksum(1000);

            Check(checksum1 != checksum0, "checksum(1) != checksum(0)");
            Check(checksum2 != checksum0, "checksum(2) != checksum(0)");
            Check(checksum2 != checksum1, "checksum(2) != checksum(1)");
            Check(checksum42 != checksum0, "checksum(42) != checksum(0)");
            Check(checksum1000 != checksum0, "checksum(1000) != checksum(0)");

            // Print all for C++ comparison
            Console.WriteLine($"       checksum(1)    = 0x{checksum1:X16}");
            Console.WriteLine($"       checksum(2)    = 0x{checksum2:X16}");
            Console.WriteLine($"       checksum(42)   = 0x{checksum42:X16}");
            Console.WriteLine($"       checksum(1000) = 0x{checksum1000:X16}");

            Console.WriteLine();

            // ── 7. Null Safety ────────────────────────────────────

            Console.WriteLine("--- Null Safety ---");

            // Create with null desc (uses IntPtr overload)
            Check(NativeBindings.ax_world_create_ptr(IntPtr.Zero) == IntPtr.Zero,
                  "ax_world_create(NULL) -> NULL");

            // Create with zero dimensions
            var badDesc = new AxWorldDesc { width = 0, height = 48, reserved = 0 };
            Check(NativeBindings.ax_world_create(ref badDesc) == IntPtr.Zero,
                  "ax_world_create(0x48) -> NULL");

            badDesc = new AxWorldDesc { width = 64, height = 0, reserved = 0 };
            Check(NativeBindings.ax_world_create(ref badDesc) == IntPtr.Zero,
                  "ax_world_create(64x0) -> NULL");

            // Operations on null handle (must not crash)
            NativeBindings.ax_world_destroy(IntPtr.Zero);
            Check(true, "ax_world_destroy(NULL) no crash");

            NativeBindings.ax_world_step(IntPtr.Zero, 10);
            Check(true, "ax_world_step(NULL, 10) no crash");

            Check(NativeBindings.ax_world_get_tick(IntPtr.Zero) == 0,
                  "ax_world_get_tick(NULL) -> 0");

            Check(NativeBindings.ax_world_get_cell_count(IntPtr.Zero) == 0,
                  "ax_world_get_cell_count(NULL) -> 0");

            // get_size with null handle
            NativeBindings.ax_world_get_size(IntPtr.Zero, out _, out _);
            Check(true, "ax_world_get_size(NULL, ...) no crash");

            // get_size with null out-pointers
            NativeBindings.ax_world_get_size_ptr(world, IntPtr.Zero, IntPtr.Zero);
            Check(true, "ax_world_get_size(world, NULL, NULL) no crash");

            // Snapshot on null world
            Check(NativeBindings.ax_world_read_snapshot(
                      IntPtr.Zero, AxSnapshotChannel.WorldMeta, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(NULL) -> 0");

            // Unknown channel
            Check(NativeBindings.ax_world_read_snapshot(
                      world, (AxSnapshotChannel)999, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(unknown channel) -> 0");

            // Command submission null safety
            Check(NativeBindings.ax_world_submit_command_ptr(IntPtr.Zero, IntPtr.Zero) == 0,
                  "ax_world_submit_command(NULL, NULL) -> 0");

            Check(NativeBindings.ax_world_submit_command_ptr(world, IntPtr.Zero) == 0,
                  "ax_world_submit_command(world, NULL) -> 0");

            // Results on null world
            Check(NativeBindings.ax_world_read_command_results(IntPtr.Zero, IntPtr.Zero, 0) == 0,
                  "ax_world_read_command_results(NULL) -> 0");

            Console.WriteLine();

            // ── 8. Cleanup ────────────────────────────────────────

            Console.WriteLine("--- Cleanup ---");

            NativeBindings.ax_world_destroy(world);
            world = IntPtr.Zero;
            Check(true, "ax_world_destroy completed");

            Console.WriteLine();

            // ── Summary ───────────────────────────────────────────

            Console.WriteLine($"=== Results: {_pass} passed, {_fail} failed ===");

            return (_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
}