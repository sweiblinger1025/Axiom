# Axiom Core — static library (v1)
#
# Builds the engine core as a static library.
# Public headers in include/ are exposed to consumers.

add_library(axiom_core STATIC
        src/ax_core.cpp
)

target_include_directories(axiom_core
        PUBLIC  include    # ax_abi.h — visible to anything linking axiom_core
)

# Strict warnings
target_compile_options(axiom_core PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)