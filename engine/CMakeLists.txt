# engine/CMakeLists.txt — AxiomCore shared library
#
# Builds the deterministic simulation core as a shared library
# exporting a C ABI surface.
#
# Consumers:
#   - apps/headless (C++, links directly)
#   - apps/viewer   (C#, loads via P/Invoke)

# ── Sources ──────────────────────────────────────────────────────

add_library(AxiomCore SHARED
        core/world.cpp
        bindings/c/src/ax_api.cpp
)

# ── Include paths ────────────────────────────────────────────────
# PUBLIC:  consumers linking against AxiomCore get ax_api.h
# PRIVATE: internal engine headers (core simulation, future subsystems)

target_include_directories(AxiomCore
        PUBLIC  bindings/c/include
        PRIVATE core
        PRIVATE src
)

# ── Compile definitions ─────────────────────────────────────────
# AX_BUILDING_DLL:  flips AX_API from dllimport to dllexport (Windows)
#                   and sets visibility("default") (Unix). Consumers
#                   must NOT define this.
# AX_BUILD_TYPE:    baked into ax_get_build_info() output
# AX_COMPILER_ID:   identifies the compiler (MSVC, Clang, GNU, etc.)
# AX_PLATFORM:      identifies the target OS

target_compile_definitions(AxiomCore
        PRIVATE
        AX_BUILDING_DLL
        AX_BUILD_TYPE="$<CONFIG>"
        AX_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
        AX_PLATFORM="${CMAKE_SYSTEM_NAME}"
)

# ── Language standard ────────────────────────────────────────────

target_compile_features(AxiomCore PRIVATE cxx_std_20)

# ── Symbol visibility (non-Windows) ─────────────────────────────
# Hide everything by default; only AX_API-marked symbols are exported.
# On Windows, dllexport/dllimport handles this automatically.

set_target_properties(AxiomCore PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# ── Output directories ──────────────────────────────────────────
# Place the built library alongside executables so they can find
# each other at runtime (especially important on Windows where
# the DLL must be next to the EXE or on PATH).

set_target_properties(AxiomCore PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ── Platform fixups ──────────────────────────────────────────────
# MinGW prefixes shared libraries with "lib" (Unix convention).
# Strip it on Windows so the output is AxiomCore.dll, which is what
# both the headless linker and C# P/Invoke expect.

if(WIN32)
    set_target_properties(AxiomCore PROPERTIES PREFIX "")
endif()