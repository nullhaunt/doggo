cmake_minimum_required(VERSION 3.28)

project(DOGGO
        VERSION 0.1.0
        DESCRIPTION "Data-Oriented Geometry & Gameplay Orchestrator"
        LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DOGGO_BUILD_GAME "Build the DOGGO game" ON)

add_subdirectory(Engine)

if (DOGGO_BUILD_GAME)
    add_subdirectory(Game)
endif ()

# Convenience target for explicitly building everything that belongs to the
# workspace. The game NRO already depends on the game executable, which in turn
# depends on the engine, but keeping a named aggregate target makes presets and
# IDE usage unambiguous.
if (TARGET doggo_game_nro)
    add_custom_target(doggo_all
            DEPENDS
            doggo_platform_nx
            doggo_game_nro
    )
else ()
    add_custom_target(doggo_all
            DEPENDS
            doggo_platform_nx
    )
endif ()
