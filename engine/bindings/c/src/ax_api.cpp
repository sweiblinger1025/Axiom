/*
 * ax_api.cpp — Axiom C API implementation
 *
 * Implements the public C ABI surface declared in ax_api.h.
 *
 * For TASK-000 these are all self-contained. As core systems
 * are added, binding functions will forward into internal C++
 * interfaces — but the C boundary stays here.
 */

#include "ax_api.h"

#include <cstdlib>  // malloc, free
#include <cstdio>   // snprintf

/* ── Build configuration ─────────────────────────────────────────
 *
 * AX_BUILD_TYPE is defined by CMake (e.g., "Debug", "Release").
 * Falls back to "Unknown" if not set, so the code compiles
 * even without CMake (e.g., during IDE indexing).
 */

#ifndef AX_BUILD_TYPE
  #define AX_BUILD_TYPE "Unknown"
#endif

#ifndef AX_COMPILER_ID
  #define AX_COMPILER_ID "Unknown"
#endif

#ifndef AX_PLATFORM
  #define AX_PLATFORM "Unknown"
#endif

/* ── Versioning ──────────────────────────────────────────────── */

uint32_t ax_get_abi_version(void)
{
    return AX_ABI_VERSION;
}

uint32_t ax_get_version_packed(void)
{
    return AX_VERSION_PACKED;
}

const char* ax_get_build_info(void)
{
    /*
     * Static buffer, built once on first call.
     * Lifetime = library lifetime.
     * Thread safety: benign race — worst case is redundant init
     * with identical content (all inputs are compile-time constants).
     */
    static char buffer[256] = {0};
    static bool initialized = false;

    if (!initialized) {
        snprintf(buffer, sizeof(buffer),
                 "Axiom %d.%d.%d (ABI %u) [%s] compiler=%s platform=%s built %s %s",
                 AX_VERSION_MAJOR, AX_VERSION_MINOR, AX_VERSION_PATCH,
                 AX_ABI_VERSION,
                 AX_BUILD_TYPE,
                 AX_COMPILER_ID,
                 AX_PLATFORM,
                 __DATE__, __TIME__);
        initialized = true;
    }

    return buffer;
}

/* ── Memory ──────────────────────────────────────────────────── */

void* ax_alloc(size_t size)
{
    if (size == 0) {
        return nullptr;
    }
    return std::malloc(size);
}

void ax_free(void* ptr)
{
    std::free(ptr);  /* free(nullptr) is safe per the C standard */
}
