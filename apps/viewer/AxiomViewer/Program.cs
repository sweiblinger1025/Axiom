/*
 * Program.cs — Axiom Viewer entry point
 *
 * TASK-001: World lifecycle + tick loop + minimal observation.
 * TASK-002: Spatial grid channels (terrain + occupancy snapshots).
 *
 * Validates the same checks as the headless runner, but from
 * managed code via P/Invoke. Proves that:
 *   - World creation and destruction work across the ABI
 *   - Cell count query works
 *   - Tick advancement is deterministic
 *   - Snapshot reads work with caller-allocated buffers
 *   - Terrain and occupancy channels round-trip correctly
 *   - All null-safety guarantees hold from C#
 *
 * See TASK-001.md and TASK-002.md for acceptance criteria.
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
            Console.WriteLine("=== Axiom Viewer -- TASK-002 Validation (C#) ===");
            Console.WriteLine();

            // ── 1. ABI Checks (from TASK-000) ─────────────────────

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

            // ── 2. World Creation ─────────────────────────────────

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

            // Cell count (TASK-002)
            Check(NativeBindings.ax_world_get_cell_count(world) == 64u * 48u,
                  "cell count == 3072");

            // Initial tick
            Check(NativeBindings.ax_world_get_tick(world) == 0, "initial tick == 0");

            Console.WriteLine();

            // ── 3. Tick Advancement ───────────────────────────────

            Console.WriteLine("--- Tick Advancement ---");

            NativeBindings.ax_world_step(world, 1);
            Check(NativeBindings.ax_world_get_tick(world) == 1, "step(1) -> tick == 1");

            NativeBindings.ax_world_step(world, 9);
            Check(NativeBindings.ax_world_get_tick(world) == 10, "step(9) -> tick == 10");

            Console.WriteLine();

            // ── 4. Snapshot: World Meta (TASK-001) ────────────────

            Console.WriteLine("--- Snapshot (World Meta) ---");

            int structSize = Marshal.SizeOf<AxWorldMetaSnapshotV1>();

            // Phase 1: query required size (null buffer)
            uint required = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.WorldMeta, IntPtr.Zero, 0);

            Check(required == (uint)structSize, "size query returns correct size");

            // Phase 2: read into buffer
            IntPtr buffer = Marshal.AllocHGlobal(structSize);
            try
            {
                // Zero the buffer before reading
                for (int i = 0; i < structSize; i++)
                    Marshal.WriteByte(buffer, i, 0);

                uint written = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.WorldMeta, buffer, (uint)structSize);

                Check(written == (uint)structSize, "read returns correct size");

                var snap = Marshal.PtrToStructure<AxWorldMetaSnapshotV1>(buffer);

                Check(snap.version == NativeBindings.AX_WORLD_META_SNAPSHOT_VERSION,
                      "snapshot version correct");
                Check(snap.sizeBytes == (uint)structSize,
                      "snapshot sizeBytes correct");
                Check(snap.tick == 10,
                      "snapshot tick == 10");
                Check(snap.width == 64,
                      "snapshot width == 64");
                Check(snap.height == 48,
                      "snapshot height == 48");
                Check(snap.reserved == 0,
                      "snapshot reserved == 0");
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            // Undersized buffer: should return required size without writing
            IntPtr undersizedBuf = Marshal.AllocHGlobal(structSize);
            try
            {
                // Fill with 0xFF sentinel pattern
                for (int i = 0; i < structSize; i++)
                    Marshal.WriteByte(undersizedBuf, i, 0xFF);

                uint undersized = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.WorldMeta, undersizedBuf, 1);

                Check(undersized == (uint)structSize,
                      "undersized buffer returns required size");

                // Verify the buffer was not written to
                bool untouched = true;
                for (int i = 0; i < structSize; i++)
                {
                    if (Marshal.ReadByte(undersizedBuf, i) != 0xFF)
                    {
                        untouched = false;
                        break;
                    }
                }
                Check(untouched, "undersized buffer not modified");
            }
            finally
            {
                Marshal.FreeHGlobal(undersizedBuf);
            }

            Console.WriteLine();

            // ── 5. Snapshot: Terrain (TASK-002) ───────────────────

            Console.WriteLine("--- Snapshot (Terrain) ---");

            uint cellCount = NativeBindings.ax_world_get_cell_count(world);

            // Size query
            uint terrainRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Terrain, IntPtr.Zero, 0);

            Check(terrainRequired == cellCount,
                  "terrain size query == cellCount");

            // Full read
            IntPtr terrainBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                // Fill with sentinel
                for (int i = 0; i < (int)cellCount; i++)
                    Marshal.WriteByte(terrainBuf, i, 0xFF);

                uint terrainWritten = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainBuf, cellCount);

                Check(terrainWritten == cellCount,
                      "terrain read returns cellCount bytes");

                // Verify all zeros
                bool terrainAllZero = true;
                for (int i = 0; i < (int)cellCount; i++)
                {
                    if (Marshal.ReadByte(terrainBuf, i) != 0)
                    {
                        terrainAllZero = false;
                        break;
                    }
                }
                Check(terrainAllZero, "terrain data all zeros");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainBuf);
            }

            // Undersized buffer
            IntPtr terrainSmall = Marshal.AllocHGlobal(4);
            try
            {
                for (int i = 0; i < 4; i++)
                    Marshal.WriteByte(terrainSmall, i, 0xFF);

                uint terrainUndersized = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Terrain, terrainSmall, 4);

                Check(terrainUndersized == cellCount,
                      "terrain undersized buffer returns required size");

                bool terrainSmallOk = true;
                for (int i = 0; i < 4; i++)
                {
                    if (Marshal.ReadByte(terrainSmall, i) != 0xFF)
                    {
                        terrainSmallOk = false;
                        break;
                    }
                }
                Check(terrainSmallOk, "terrain undersized buffer not modified");
            }
            finally
            {
                Marshal.FreeHGlobal(terrainSmall);
            }

            Console.WriteLine();

            // ── 6. Snapshot: Occupancy (TASK-002) ─────────────────

            Console.WriteLine("--- Snapshot (Occupancy) ---");

            // Size query
            uint occRequired = NativeBindings.ax_world_read_snapshot(
                world, AxSnapshotChannel.Occupancy, IntPtr.Zero, 0);

            Check(occRequired == cellCount,
                  "occupancy size query == cellCount");

            // Full read
            IntPtr occBuf = Marshal.AllocHGlobal((int)cellCount);
            try
            {
                for (int i = 0; i < (int)cellCount; i++)
                    Marshal.WriteByte(occBuf, i, 0xFF);

                uint occWritten = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Occupancy, occBuf, cellCount);

                Check(occWritten == cellCount,
                      "occupancy read returns cellCount bytes");

                bool occAllZero = true;
                for (int i = 0; i < (int)cellCount; i++)
                {
                    if (Marshal.ReadByte(occBuf, i) != 0)
                    {
                        occAllZero = false;
                        break;
                    }
                }
                Check(occAllZero, "occupancy data all zeros");
            }
            finally
            {
                Marshal.FreeHGlobal(occBuf);
            }

            // Undersized buffer
            IntPtr occSmall = Marshal.AllocHGlobal(4);
            try
            {
                for (int i = 0; i < 4; i++)
                    Marshal.WriteByte(occSmall, i, 0xFF);

                uint occUndersized = NativeBindings.ax_world_read_snapshot(
                    world, AxSnapshotChannel.Occupancy, occSmall, 4);

                Check(occUndersized == cellCount,
                      "occupancy undersized buffer returns required size");

                bool occSmallOk = true;
                for (int i = 0; i < 4; i++)
                {
                    if (Marshal.ReadByte(occSmall, i) != 0xFF)
                    {
                        occSmallOk = false;
                        break;
                    }
                }
                Check(occSmallOk, "occupancy undersized buffer not modified");
            }
            finally
            {
                Marshal.FreeHGlobal(occSmall);
            }

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

            // Overflow rejection (TASK-002)
            badDesc = new AxWorldDesc { width = 65536, height = 65536, reserved = 0 };
            Check(NativeBindings.ax_world_create(ref badDesc) == IntPtr.Zero,
                  "ax_world_create(65536x65536) overflow -> NULL");

            // Operations on null handle (must not crash)
            NativeBindings.ax_world_destroy(IntPtr.Zero);
            Check(true, "ax_world_destroy(NULL) no crash");

            NativeBindings.ax_world_step(IntPtr.Zero, 10);
            Check(true, "ax_world_step(NULL, 10) no crash");

            // Step with ticks == 0 (must not advance)
            ulong tickBefore = NativeBindings.ax_world_get_tick(world);
            NativeBindings.ax_world_step(world, 0);
            Check(NativeBindings.ax_world_get_tick(world) == tickBefore,
                  "ax_world_step(world, 0) no-op");

            Check(NativeBindings.ax_world_get_tick(IntPtr.Zero) == 0,
                  "ax_world_get_tick(NULL) -> 0");

            // Cell count null safety (TASK-002)
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
                  "ax_world_read_snapshot(NULL, META) -> 0");

            // Snapshot on null world for new channels (TASK-002)
            Check(NativeBindings.ax_world_read_snapshot(
                      IntPtr.Zero, AxSnapshotChannel.Terrain, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(NULL, TERRAIN) -> 0");

            Check(NativeBindings.ax_world_read_snapshot(
                      IntPtr.Zero, AxSnapshotChannel.Occupancy, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(NULL, OCCUPANCY) -> 0");

            // Unknown channel
            Check(NativeBindings.ax_world_read_snapshot(
                      world, (AxSnapshotChannel)999, IntPtr.Zero, 0) == 0,
                  "ax_world_read_snapshot(unknown channel) -> 0");

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