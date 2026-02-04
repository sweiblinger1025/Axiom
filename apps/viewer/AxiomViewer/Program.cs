/*
 * Program.cs — Axiom Viewer entry point
 *
 * TASK-001: World lifecycle + tick loop + minimal observation.
 *
 * Validates the same 31 checks as the headless runner, but from
 * managed code via P/Invoke. Proves that:
 *   - World creation and destruction work across the ABI
 *   - Tick advancement is deterministic
 *   - Snapshot reads work with caller-allocated buffers
 *   - All null-safety guarantees hold from C#
 *
 * See TASK-001.md for acceptance criteria.
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
            Console.WriteLine("=== Axiom Viewer -- TASK-001 Validation (C#) ===");
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

            // ── 4. Snapshot Read ──────────────────────────────────

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

            // ── 5. Null Safety ────────────────────────────────────

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

            // Step with ticks == 0 (must not advance)
            ulong tickBefore = NativeBindings.ax_world_get_tick(world);
            NativeBindings.ax_world_step(world, 0);
            Check(NativeBindings.ax_world_get_tick(world) == tickBefore,
                  "ax_world_step(world, 0) no-op");

            Check(NativeBindings.ax_world_get_tick(IntPtr.Zero) == 0,
                  "ax_world_get_tick(NULL) -> 0");

            // get_size with null handle (out params receive uninitialized but no crash)
            NativeBindings.ax_world_get_size(IntPtr.Zero, out _, out _);
            Check(true, "ax_world_get_size(NULL, ...) no crash");

            // get_size with null out-pointers (uses IntPtr overload)
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

            Console.WriteLine();

            // ── 6. Cleanup ────────────────────────────────────────

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