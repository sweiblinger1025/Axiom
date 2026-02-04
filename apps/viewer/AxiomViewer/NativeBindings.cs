/*
 * NativeBindings.cs - P/Invoke declarations for AxiomCore C API
 * 
 * This file mirros ax_api.h on the C# sside.
 * Every exported function in ax_api.h should have a corresponding
 * declaration here.
 * 
 * Conventions:
 *   - Library name: "AxiomCore" (.NET resolves platform-specific names)
 *   - Calling convention: Cdecl (matches AX_CALL)
 *   - const char* returns: IntPtr + manual marshal (avoids free-on-static-memory)
 *   - size_t: nuint
 *   - void*: IntPtr
 */

using System;
using System.Runtime.InteropServices;

namespace Axiom
{
    /// <summary>
    /// P/Invoke bindings for the AxiomCore native library.
    /// Maps directly to the C API declared in ax_api.h.
    /// </summary>
    internal static class NativeBindings
    {
        private const string LibName = "AxiomCore";

        // -- ABI version constant -----------------------------------------
        // Must match AX_ABIVERSION in ax_api.h.
        // Used for compile-time vs runtime version checking.
        internal const uint AX_ABI_VERSION = 1;

        // -- Versioning ---------------------------------------------------

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

        // -- Memory -------------------------------------------------------

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr ax_alloc(nuint size);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ax_free(IntPtr ptr);

    }
}
