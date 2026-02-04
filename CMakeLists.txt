# CMakeLists.txt — Axiom project root
#
# Orchestrates the build for all Axiom components.
# Invoke with: cmake -S . -B build
#
# Components:
#   engine/       → AxiomCore shared library
#   apps/headless → Headless runner (C++ ABI validation, tests, benchmarks)
#   apps/viewer   → C# viewer (built separately via dotnet CLI — not managed by CMake)

cmake_minimum_required(VERSION 3.24)

project(Axiom
        VERSION   0.0.1
        LANGUAGES C CXX
)

# ── Subdirectories ───────────────────────────────────────────────
# Order matters: engine defines the AxiomCore target that apps consume.

add_subdirectory(engine)
add_subdirectory(apps/headless)