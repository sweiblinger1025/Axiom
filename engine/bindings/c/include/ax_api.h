/*
 * ax_api.h — Axiom Engine C API
 *
 * This is the public C ABI boundary for AxiomCore.
 * All external consumers (headless runner, C# viewer, future tools)
 * interact with the engine exclusively through this header.
 *
 * Rules (from TRUTH_VS_PRESENTATION.md):
 *   - C++ owns truth; this API is the only way to observe or change it
 *   - Commands in, snapshots/events out
 *   - No C++ types cross this boundary
 *
 * P/Invoke notes (C# interop):
 *   uint32_t    → uint
 *   size_t      → nuint (or UIntPtr)
 *   void*       → IntPtr
 *   const char* → IntPtr + Marshal.PtrToStringUTF8()
 *                 (or [return: MarshalAs(UnmanagedType.LPUTF8Str)] → string)
 */

#ifndef AX_API_H
#define AX_API_H

#include <stdint.h>
#include <stddef.h>

/* ── Export / Import ─────────────────────────────────────────────
 *
 * AX_API marks symbols for shared-library export (when building
 * AxiomCore) or import (when consuming it). The build system
 * defines AX_BUILDING_DLL only when compiling the engine library.
 * Consumers must not define it.
 *
 * AX_CALL sets the calling convention. __cdecl is the Windows
 * default and the safest choice for P/Invoke interop.
 */

#if defined(_WIN32)
  #if defined(AX_BUILDING_DLL)
    #define AX_API __declspec(dllexport)
  #else
    #define AX_API __declspec(dllimport)
  #endif
  #define AX_CALL __cdecl
#else
  #if defined(AX_BUILDING_DLL)
    #define AX_API __attribute__((visibility("default")))
  #else
    #define AX_API
  #endif
  #define AX_CALL
#endif

/* ── ABI Version ─────────────────────────────────────────────────
 *
 * Bumped on any breaking change to function signatures, struct
 * layouts, or behavioral contracts in this header.
 *
 * Consumers should check ax_get_abi_version() at load time
 * and refuse to proceed on mismatch.
 */

#define AX_ABI_VERSION 1u

/* ── Version Packing ─────────────────────────────────────────────
 *
 * Engine version encoded as 0x00MMmmpp:
 *   major = (v >> 16) & 0xFF
 *   minor = (v >>  8) & 0xFF
 *   patch =  v        & 0xFF
 */

#define AX_VERSION_MAJOR 0
#define AX_VERSION_MINOR 0
#define AX_VERSION_PATCH 1

#define AX_VERSION_PACKED                  \
    ((uint32_t)(AX_VERSION_MAJOR) << 16 |  \
     (uint32_t)(AX_VERSION_MINOR) <<  8 |  \
     (uint32_t)(AX_VERSION_PATCH))

#ifdef __cplusplus
extern "C" {
#endif

/* ── Versioning ──────────────────────────────────────────────── */

/*
 * Returns the ABI version of the loaded library.
 * Consumers must check: ax_get_abi_version() == AX_ABI_VERSION
 *
 * C#: [DllImport] → uint
 */
AX_API uint32_t AX_CALL ax_get_abi_version(void);

/*
 * Returns the packed engine version (0x00MMmmpp).
 *
 * C#: [DllImport] → uint
 */
AX_API uint32_t AX_CALL ax_get_version_packed(void);

/*
 * Returns a human-readable build info string.
 * The returned pointer is to a static string inside the library.
 * Valid for the lifetime of the loaded library. Do NOT free it.
 *
 * C#: [DllImport] → IntPtr, then Marshal.PtrToStringUTF8()
 */
AX_API const char* AX_CALL ax_get_build_info(void);

/* ── Memory ──────────────────────────────────────────────────── */

/*
 * Allocate memory through the engine's allocator.
 * Used when the engine returns engine-allocated buffers that
 * the caller must eventually free via ax_free().
 *
 * Returns NULL on failure.
 *
 * C#: [DllImport] size → nuint, returns IntPtr
 */
AX_API void* AX_CALL ax_alloc(size_t size);

/*
 * Free memory previously allocated by ax_alloc().
 * Passing NULL is a safe no-op.
 *
 * C#: [DllImport] IntPtr → void
 */
AX_API void AX_CALL ax_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* AX_API_H */
