cmake_minimum_required(VERSION 3.28)

project(
        DOGGO
        VERSION 0.2.0
        DESCRIPTION "Data-Oriented Geometry & Gameplay Orchestrator"
        LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/DoggoOptions.cmake)
include(cmake/DoggoWarnings.cmake)

add_subdirectory(engine)
add_subdirectory(platform)
add_subdirectory(samples/02-mesh)
