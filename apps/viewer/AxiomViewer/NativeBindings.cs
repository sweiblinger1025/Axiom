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
        Terrain = 2,    // TASK-002
        Occupancy = 3     // TASK-002
    }

    // ── Command types ──────────────────────────────────────────
    // Mirrors ax_command_type in ax_api.h.
    // Wire format is uint32_t; enum backing matches.

    internal enum AxCommandType : uint
    {
        None = 0,
        DebugSetCellU8 = 1000
    }

    // ── Command reject reasons ─────────────────────────────────
    // Mirrors ax_command_reject_reason in ax_api.h.
    // Wire format is uint8_t; enum backing is byte.

    internal enum AxCommandRejectReason : byte
    {
        None = 0,
        InvalidCoords = 1,
        InvalidChannel = 2
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
    /// World meta snapshot (version 1). Mirrors ax_world_meta_snapshot_v1.
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
    /// Single-cell set payload for uint8 grids.
    /// Mirrors ax_cmd_set_cell_u8_v1 in ax_api.h.
    /// Expected size: 12 bytes.
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
    /// Uses Explicit layout to model the C union: all payload variants
    /// overlap at offset 8. Size forced to 40 bytes (8-byte header +
    /// 32-byte payload union).
    /// Expected size: 40 bytes.
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Size = 40)]
    internal struct AxCommandV1
    {
        [FieldOffset(0)] public uint version;
        [FieldOffset(4)] public uint type;
        [FieldOffset(8)] public AxCmdSetCellU8V1 setCellU8;
        // Future command payloads: add [FieldOffset(8)] fields here.
        // The _raw[32] union member is implicit from Size = 40.
    }

    /// <summary>
    /// Command result (version 1). Mirrors ax_command_result_v1.
    /// Expected size: 32 bytes.
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

        // ── Versioning ─────────────────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_get_abi_version();

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_get_version_packed();

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
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl,
                   EntryPoint = "ax_world_get_size")]
        internal static extern void ax_world_get_size_ptr(
            IntPtr world, IntPtr outWidth, IntPtr outHeight);

        // ── Cell count (TASK-002) ──────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_get_cell_count(IntPtr world);

        // ── Snapshots ──────────────────────────────────────────

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_read_snapshot(
            IntPtr world,
            AxSnapshotChannel channel,
            IntPtr outBuffer,
            uint outBufferSizeBytes);

        // ── Commands (TASK-003) ────────────────────────────────

        /// <summary>
        /// Submit a command for processing at the next tick boundary.
        /// Returns engine-assigned command ID (monotonic, non-zero).
        /// Returns 0 for structural failures (not queued).
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong ax_world_submit_command(
            IntPtr world, ref AxCommandV1 cmd);

        /// <summary>
        /// Raw-pointer overload for null-safety testing (null cmd).
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl,
                   EntryPoint = "ax_world_submit_command")]
        internal static extern ulong ax_world_submit_command_ptr(
            IntPtr world, IntPtr cmd);

        /// <summary>
        /// Read command results from the last tick.
        /// Follows the caller-allocated buffer pattern:
        ///   - NULL/undersized buffer → returns required size
        ///   - Sufficient buffer → writes results, returns bytes written
        ///   - 0 on error (null world)
        /// </summary>
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ax_world_read_command_results(
            IntPtr world, IntPtr outBuffer, uint outBufferSizeBytes);
    }
}