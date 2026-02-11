# Axiom Headless Shell — A1 test harness
#
# First-class app shell (ARCHITECTURE.md) for determinism tests,
# replay validation, and CI acceptance checks.

add_executable(axiom_headless
        main.cpp
)

target_link_libraries(axiom_headless
        PRIVATE axiom_core
)

# Strict warnings
target_compile_options(axiom_headless PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
