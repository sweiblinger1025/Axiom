cmake_minimum_required(VERSION 3.20)
project(Axiom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine core library
add_subdirectory(engine)

# App shells
add_subdirectory(apps/headless)