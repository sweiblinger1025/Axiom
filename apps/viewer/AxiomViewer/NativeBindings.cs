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
        Terrain   = 2,
        Occupancy = 3,
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

        // ── Cell count (TASK-002) ──────────────────────────────

        /// <summary>
        /// Returns total cell count (width * height).
        /// Returns 0 if world is NULL.
        /// </summary>
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
    }
}