# apps/headless/CMakeLists.txt — Headless runner
#
# Minimal C++ executable that links against AxiomCore.
# Used for ABI validation (TASK-000), and later for
# testing, benchmarking, and server hosting.
#
# No rendering, no UI, no platform dependencies beyond
# the C++ standard library and AxiomCore.

add_executable(AxiomHeadless
        main.cpp
)

# ── Link against AxiomCore ───────────────────────────────────────
# This transitively provides:
#   - include path to ax_api.h (PUBLIC on AxiomCore)
#   - link dependency on the shared library

target_link_libraries(AxiomHeadless
        PRIVATE AxiomCore
)

# ── Language standard ────────────────────────────────────────────

target_compile_features(AxiomHeadless PRIVATE cxx_std_20)

# ── Output directory ─────────────────────────────────────────────
# Same bin/ directory as AxiomCore so the executable can find
# the shared library at runtime without PATH manipulation.

set_target_properties(AxiomHeadless PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── Platform fixups ──────────────────────────────────────────────
# MinGW defaults can pick the Windows GUI subsystem (expecting WinMain).
# Explicitly request the console subsystem so it links against main().

if(MINGW)
    target_link_options(AxiomHeadless PRIVATE -mconsole)
endif()