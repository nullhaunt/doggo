cmake_minimum_required(VERSION 3.28)

project(DOGGOEngine
        VERSION 0.1.0
        DESCRIPTION "DOGGO game engine"
        LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Stable engine-relative paths. Do not use PROJECT_SOURCE_DIR from deeply nested
# CMakeLists files for public headers: this project can be either the top-level
# project or a subproject of the game/workspace.
set(DOGGO_ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(src)
