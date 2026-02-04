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

/* ── World: Opaque Handle ────────────────────────────────────── */

/*
 * Opaque world handle. Consumers hold a pointer to an
 * incomplete type — the actual layout is internal to the engine.
 *
 * See ARCHITECTURE_OVERVIEW.md: "Handle-based (opaque world pointers)"
 *
 * C#: IntPtr
 */
typedef struct ax_world_t ax_world_t;
typedef ax_world_t* ax_world_handle;

/* ── World: Creation Descriptor ──────────────────────────────── */

/*
 * Describes initial world parameters for ax_world_create().
 *
 * reserved: must be set to 0. Available for future expansion
 *           without changing struct size or ABI.
 *
 * C#: [StructLayout(LayoutKind.Sequential)]
 *     uint width, uint height, uint reserved
 */
typedef struct ax_world_desc
{
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} ax_world_desc;

/* ── World: Lifecycle ────────────────────────────────────────── */

/*
 * Create a new world from a descriptor.
 *
 * Returns NULL if:
 *   - desc is NULL
 *   - desc->width == 0
 *   - desc->height == 0
 *
 * The returned handle must be destroyed with ax_world_destroy().
 *
 * C#: [DllImport] ref ax_world_desc → IntPtr
 */
AX_API ax_world_handle AX_CALL ax_world_create(const ax_world_desc* desc);

/*
 * Destroy a world and free all associated resources.
 * Passing NULL is a safe no-op.
 *
 * C#: [DllImport] IntPtr → void
 */
AX_API void AX_CALL ax_world_destroy(ax_world_handle world);

/*
 * Advance the simulation by the given number of ticks.
 * Passing NULL or ticks == 0 is a safe no-op.
 *
 * See DECISIONS.md D001: "10 Hz fixed simulation tick."
 *
 * C#: [DllImport] IntPtr, uint → void
 */
AX_API void AX_CALL ax_world_step(ax_world_handle world, uint32_t ticks);

/*
 * Returns the current simulation tick.
 * A newly created world is at tick 0.
 *
 * Returns 0 if world is NULL.
 *
 * C#: [DllImport] IntPtr → ulong
 */
AX_API uint64_t AX_CALL ax_world_get_tick(ax_world_handle world);

/*
 * Writes the world dimensions into the provided output pointers.
 * Either output pointer may be NULL (that dimension is skipped).
 *
 * No-op if world is NULL.
 *
 * C#: [DllImport] IntPtr, out uint, out uint → void
 */
AX_API void AX_CALL ax_world_get_size(
    ax_world_handle world,
    uint32_t* outWidth,
    uint32_t* outHeight);

/* ── Snapshots ───────────────────────────────────────────────── */

/*
 * Snapshot channels identify which data stream to read.
 * See SNAPSHOT_EVENT_FORMATS.md: "Channels are the fundamental
 * unit of snapshot access."
 *
 * C#: enum (int or uint)
 */
typedef enum ax_snapshot_channel
{
    AX_SNAP_WORLD_META = 1
} ax_snapshot_channel;

/*
 * World meta snapshot — version 1.
 *
 * Contains top-level world information: tick, dimensions.
 * Versioned so consumers can detect layout changes.
 *
 * See SNAPSHOT_EVENT_FORMATS.md: "Each channel has an
 * independent version."
 *
 * C#: [StructLayout(LayoutKind.Sequential)]
 *     uint version, uint sizeBytes, ulong tick,
 *     uint width, uint height, uint reserved
 */
#define AX_WORLD_META_SNAPSHOT_VERSION 1u

typedef struct ax_world_meta_snapshot_v1
{
    uint32_t version;       /* AX_WORLD_META_SNAPSHOT_VERSION        */
    uint32_t sizeBytes;     /* sizeof(ax_world_meta_snapshot_v1)     */
    uint64_t tick;          /* current simulation tick                */
    uint32_t width;         /* world width in cells                  */
    uint32_t height;        /* world height in cells                 */
    uint32_t reserved;      /* must be 0                             */
} ax_world_meta_snapshot_v1;

/*
 * Read a snapshot channel into a caller-allocated buffer.
 *
 * Caller-allocated buffer pattern (from SNAPSHOT_EVENT_FORMATS.md):
 *   1. Call with outBuffer == NULL (or outBufferSizeBytes too small)
 *      → returns the required buffer size in bytes
 *   2. Call with a sufficiently sized buffer
 *      → writes the snapshot data, returns bytes written
 *
 * Returns 0 if:
 *   - world is NULL
 *   - channel is unknown
 *
 * The snapshot reflects the current world state at the time of the call.
 *
 * C#: [DllImport] IntPtr, int, IntPtr, uint → uint
 *     (or byte[] with marshaling)
 */
AX_API uint32_t AX_CALL ax_world_read_snapshot(
    ax_world_handle world,
    ax_snapshot_channel channel,
    void* outBuffer,
    uint32_t outBufferSizeBytes);

#ifdef __cplusplus
}
#endif

#endif /* AX_API_H */