/*
 * NativeBindings.cs — P/Invoke declarations for AxiomCore C API
 *
 * This file mirrors ax_api.h on the C# side.
 * Every exported function in ax_api.h should have a corresponding
 * declaration here.
 *
 * Conventions:
 *   - Library name: "AxiomCore" (.NET resolves platform-specific names)
 *   - Calling convention: Cdecl (matches AX_CALL)
 *   - const char* returns: IntPtr + manual marshal (avoids free-on-static-memory)
 *   - size_t: nuint
 *   - void*: IntPtr
 *   - ax_world_handle: IntPtr (opaque handle)
 */

using System;
using System.Runtime.InteropServices;

namespace Axiom
{
    // ── Snapshot channel identifiers ────────────────────────────
    // Mirrors ax_snapshot_channel in ax_api.h.

    internal enum AxSnapshotChannel : int
    {
        WorldMeta = 1,
        Terrain = 2,
        Occupancy = 3
    }

    // ── Command types ───────────────────────────────────────────
    // Mirrors ax_command_type in ax_api.h.

    internal enum AxCommandType : uint
    {
        None = 0,
        DebugSetCellU8 = 1000
    }

    // ── Command reject reasons ──────────────────────────────────
    // Mirrors ax_command_reject_reason in ax_api.h.

    internal static class AxCommandRejectReason
    {
        internal const byte None = 0;
        internal const byte InvalidCoords = 1;
        internal const byte InvalidChannel = 2;
    }

    // ── Interop structs ────────────────────────────────────────

    /// <summary>
    /// World creation descriptor. Mirrors ax_world_desc in ax_api.h.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct AxWorldDesc
    {
        public uint width;
        public uint height;
        public uint reserved;
    }

    /// <summary>
    /// World meta snapshot (version 1). Mirrors ax_world_meta_snapshot_v1 in ax_api.h.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct AxWorldMetaSnapshotV1
    {
        public uint version;
        public uint sizeBytes;
        public ulong tick;
        public uint width;
        public uint height;
        public uint reserved;
    }

    /// <summary>
    /// Single-cell set command payload. Mirrors ax_cmd_set_cell_u8_v1 in ax_api.h.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct AxCmdSetCellU8V1
    {
        public uint x;
        public uint y;
        public byte channel;
        public byte value;
        public ushort _pad;
    }

    /// <summary>
    /// Command descriptor (version 1). Mirrors ax_command_v1 in ax_api.h.
    /// Uses explicit layout to model the C union.
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 40)]
    internal struct AxCommandV1
    {
        [FieldOffset(0)] public uint version;
        [FieldOffset(4)] public uint type;

        // Union: payload starts at offset 8
        [FieldOffset(8)] public AxCmdSetCellU8V1 setCellU8;

        // Raw payload access (32 bytes)
        // Note: Can't overlay a fixed buffer on the struct directly in safe code,
        // but we don't need it for current usage.
    }

    /// <summary>
    /// Command result (version 1). Mirrors ax_command_result_v1 in ax_api.h.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct AxCommandResultV1
    {
        public uint sizeBytes;
        public uint version;
        public ulong commandId;
        public ulong tickApplied;
        public uint type;
        public byte accepted;
        public byte reason;
        public ushort _pad;
    }

    /// <summary>
    /// P/Invoke bindings for the AxiomCore native library.
    /// Maps directly to the C API declared in ax_api.h.
    /// </summary>
    internal static class NativeBindings
    {
        private const string LibName = "AxiomCore";

        // ── Constants ──────────────────────────────────────────
        // Must match corresponding #defines in ax_api.h.
        // Manual sync point — runtime ABI check catches mismatches.

        internal const uint AX_ABI_VERSION = 1;
        internal const uint AX_WORLD_META_SNAPSHOT_VERSION = 1;
        internal const uint AX_MATH_VERSION = 1;

        // ── Expected struct sizes (for validation) ─────────────

        internal const int ExpectedCmdSetCellU8V1Size = 12;
        internal const int ExpectedCommandV1Size = 40;
        internal const int ExpectedCommandResultV1Size = 32;

        // ── Versioning ─────────────────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_get_abi_version();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_get_version_packed();

        /// <summary>
        /// Returns a pointer to a static string inside the native library.
        /// Caller must NOT free this pointer.
        /// Use Marshal.PtrToStringUTF8() to read it.
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr ax_get_build_info();

        // ── Memory ─────────────────────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr ax_alloc(nuint size);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ax_free(IntPtr ptr);

        // ── World lifecycle ────────────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr ax_world_create(ref AxWorldDesc desc);

        /// <summary>
        /// Raw-pointer overload for null-safety testing.
        /// Pass IntPtr.Zero to test ax_world_create(NULL).
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl,
                   EntryPoint = "ax_world_create")]
        internal static extern IntPtr ax_world_create_ptr(IntPtr desc);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ax_world_destroy(IntPtr world);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ax_world_step(IntPtr world, uint ticks);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong ax_world_get_tick(IntPtr world);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ax_world_get_size(
            IntPtr world, out uint outWidth, out uint outHeight);

        /// <summary>
        /// Raw-pointer overload for null-safety testing.
        /// Allows passing IntPtr.Zero for outWidth/outHeight
        /// (impossible with the 'out uint' signature).
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl,
                   EntryPoint = "ax_world_get_size")]
        internal static extern void ax_world_get_size_ptr(
            IntPtr world, IntPtr outWidth, IntPtr outHeight);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_get_cell_count(IntPtr world);

        // ── Snapshots ──────────────────────────────────────────

        /// <summary>
        /// Read a snapshot channel. Pass IntPtr.Zero / size 0 to query
        /// required buffer size. Returns bytes written, or required size,
        /// or 0 on error.
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_read_snapshot(
            IntPtr world,
            AxSnapshotChannel channel,
            IntPtr outBuffer,
            uint outBufferSizeBytes);

        // ── Commands ───────────────────────────────────────────

        /// <summary>
        /// Submit a command for processing at the next tick boundary.
        /// Returns engine-assigned command ID, or 0 on structural failure.
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong ax_world_submit_command(
            IntPtr world,
            ref AxCommandV1 cmd);

        /// <summary>
        /// Raw-pointer overload for null-safety testing.
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl,
                   EntryPoint = "ax_world_submit_command")]
        internal static extern ulong ax_world_submit_command_ptr(
            IntPtr world,
            IntPtr cmd);

        /// <summary>
        /// Read command results from the last tick.
        /// Pass IntPtr.Zero / size 0 to query required buffer size.
        /// Returns bytes written, or required size, or 0 on error.
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_read_command_results(
            IntPtr world,
            IntPtr outBuffer,
            uint outBufferSizeBytes);

        // ── Math Utilities (TASK-004) ──────────────────────────

        /// <summary>
        /// Returns the math subsystem version.
        /// Consumers should verify: ax_math_get_version() == AX_MATH_VERSION
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_math_get_version();

        /// <summary>
        /// Runs a deterministic battery of arithmetic operations and returns
        /// a 64-bit checksum. Used to prove cross-language determinism.
        /// 
        /// Same seed → same checksum, always.
        /// C++ checksum must match C# checksum exactly (within same build config).
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong ax_math_selftest_checksum(uint seed);
    }
}