/*
 * Program.cs — Axiom Viewer entry point
 *
 * TASK-000: ABI validation from managed code.
 * Proves that a C# application can load AxiomCore via P/Invoke,
 * call through the C API, and get correct results.
 *
 * This will later become the viewer/UI application.
 * For now it validates the managed-to-native boundary.
 */

using System;
using System.Runtime.InteropServices;

namespace Axiom
{
    internal class Program
    {
        private const int EXIT_SUCCESS = 0;
        private const int EXIT_FAILURE = 1;

        static int Main(string[] args)
        {
            Console.WriteLine("=== Axiom Viewer — ABI Validation (C#) ===");
            Console.WriteLine();

            // ── ABI version check ───────────────────────────────
            // Compare the C# compile-time constant (from NativeBindings)
            // with the runtime value from the loaded native library.

            uint runtimeAbi = NativeBindings.ax_get_abi_version();
            uint compiledAbi = NativeBindings.AX_ABI_VERSION;

            Console.WriteLine($"ABI version (compiled): {compiledAbi}");
            Console.WriteLine($"ABI version (runtime):  {runtimeAbi}");

            if (runtimeAbi != compiledAbi)
            {
                Console.WriteLine();
                Console.WriteLine("** FAIL: ABI version mismatch! **");
                return EXIT_FAILURE;
            }
            Console.WriteLine("ABI version match: OK");
            Console.WriteLine();

            // ── Packed version ───────────────────────────────────
            // Decode the packed version to prove uint32 returns work
            // across the P/Invoke boundary.

            uint version = NativeBindings.ax_get_version_packed();
            uint major = (version >> 16) & 0xFF;
            uint minor = (version >> 8) & 0xFF;
            uint patch = version & 0xFF;

            Console.WriteLine($"Engine version: {major}.{minor}.{patch} (packed: 0x{version:X8})");

            // Verify against the same packing formula used in ax_api.h
            uint expectedPacked = (0u << 16) | (0u << 8) | 1u;  // 0.0.1
            if (version != expectedPacked)
            {
                Console.WriteLine();
                Console.WriteLine("** FAIL: packed version mismatch! **");
                return EXIT_FAILURE;
            }
            Console.WriteLine("Packed version match: OK");
            Console.WriteLine();

            // ── Build info string ────────────────────────────────
            // Proves const char* → IntPtr marshaling works.
            // The pointer is to static storage inside the native library,
            // so we must NOT free it — just read from it.

            IntPtr infoPtr = NativeBindings.ax_get_build_info();

            if (infoPtr == IntPtr.Zero)
            {
                Console.WriteLine("** FAIL: ax_get_build_info() returned null! **");
                return EXIT_FAILURE;
            }

            string? info = Marshal.PtrToStringUTF8(infoPtr);

            if (string.IsNullOrEmpty(info))
            {
                Console.WriteLine("** FAIL: build info string is null or empty! **");
                return EXIT_FAILURE;
            }
            Console.WriteLine($"Build info: {info}");
            Console.WriteLine("Build info return: OK");
            Console.WriteLine();

            // ── Allocator round-trip ─────────────────────────────
            // Proves ax_alloc / ax_free work across the managed/native
            // boundary. Uses Marshal.WriteByte / ReadByte to access
            // the native memory from C#.

            nuint testSize = 256;
            IntPtr block = NativeBindings.ax_alloc(testSize);

            if (block == IntPtr.Zero)
            {
                Console.WriteLine($"** FAIL: ax_alloc({testSize}) returned null! **");
                return EXIT_FAILURE;
            }
            Console.WriteLine($"ax_alloc({testSize}): 0x{block:X}");

            // Write a pattern into native memory
            for (int i = 0; i < (int)testSize; i++)
            {
                Marshal.WriteByte(block, i, (byte)(i & 0xFF));
            }

            // Read it back and verify
            bool patternOk = true;
            for (int i = 0; i < (int)testSize; i++)
            {
                byte actual = Marshal.ReadByte(block, i);
                byte expected = (byte)(i & 0xFF);
                if (actual != expected)
                {
                    Console.WriteLine($"** FAIL: byte [{i}] expected {expected}, got {actual} **");
                    patternOk = false;
                    break;
                }
            }

            NativeBindings.ax_free(block);

            if (!patternOk)
            {
                Console.WriteLine("** FAIL: memory pattern verification failed! **");
                return EXIT_FAILURE;
            }
            Console.WriteLine("Allocator round-trip: OK");
            Console.WriteLine();

            // ── Zero-size allocation ─────────────────────────────
            // ax_alloc(0) must return IntPtr.Zero (nullptr on native side).

            IntPtr zeroBlock = NativeBindings.ax_alloc(0);
            if (zeroBlock != IntPtr.Zero)
            {
                Console.WriteLine("** FAIL: ax_alloc(0) should return null! **");
                NativeBindings.ax_free(zeroBlock);
                return EXIT_FAILURE;
            }
            Console.WriteLine("ax_alloc(0) → null: OK");
            Console.WriteLine();

            // ── ax_free(null) ────────────────────────────────────
            // Must be a safe no-op.

            NativeBindings.ax_free(IntPtr.Zero);
            Console.WriteLine("ax_free(null): OK (no crash)");
            Console.WriteLine();

            // ── Summary ──────────────────────────────────────────
            Console.WriteLine("=== All ABI validation checks passed (C#) ===");
            return EXIT_SUCCESS;
        }
    }
}