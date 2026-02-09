cmake_minimum_required(VERSION 3.20)

project(Axiom LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AXIOM_BUILD_APPS "Build example app shells" ON)

add_library(axiom_core STATIC
    engine/src/ax_core.cpp
    engine/src/ax_last_error.cpp
)
target_include_directories(axiom_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)
target_compile_definitions(axiom_core PRIVATE AXIOM_CORE_BUILD=1)

if (AXIOM_BUILD_APPS)
    add_executable(axiom_headless
        apps/headless/main.cpp
    )
    target_link_libraries(axiom_headless PRIVATE axiom_core)
    set_target_properties(axiom_headless PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/headless
    )
endif()
