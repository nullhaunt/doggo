cmake_minimum_required(VERSION 3.28)

project(DOGGO_Game
        VERSION 0.1.0
        DESCRIPTION "Game built on the DOGGO engine"
        LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# When Game is built through the workspace, the engine target already exists.
# When Game is configured by itself, locate an engine checkout/submodule and
# add it to this build. DOGGO_ENGINE_ROOT can always be supplied explicitly.
if (NOT TARGET doggo::platform::nx)
    set(DOGGO_ENGINE_ROOT "" CACHE PATH "Path to the DOGGO Engine source directory")

    if (NOT DOGGO_ENGINE_ROOT)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/CMakeLists.txt")
            # Common layout when Engine is a submodule of the Game repository.
            set(DOGGO_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Engine")
        elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Engine/CMakeLists.txt")
            # Convenient sibling-repository/workspace layout.
            set(DOGGO_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Engine")
        endif ()
    endif ()

    if (NOT EXISTS "${DOGGO_ENGINE_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
                "DOGGO Engine was not found. Set DOGGO_ENGINE_ROOT to the Engine "
                "repository, or place Engine at ./Engine (submodule) or ../Engine (sibling repo)."
        )
    endif ()

    add_subdirectory(
            "${DOGGO_ENGINE_ROOT}"
            "${CMAKE_CURRENT_BINARY_DIR}/doggo-engine"
    )
endif ()

set(APP_TARGET doggo_game)
set(APP_ROMFS_TARGET ${APP_TARGET}_romfs)
set(APP_TITLE "DOGGO Game")
set(APP_AUTHOR "Nullhaunt")
set(APP_VERSION "${PROJECT_VERSION}")

add_executable(${APP_TARGET}
        src/Main.cpp
)

target_compile_features(${APP_TARGET}
        PRIVATE
        cxx_std_20
)

target_link_libraries(${APP_TARGET}
        PRIVATE
        doggo::platform::nx
)

set(GATE0_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Content/shaders/gate0")

nx_add_shader_program(doggo_gate0_triangle_vsh
        "${GATE0_SHADER_SOURCE_DIR}/gate0_triangle_vsh.glsl"
        vert
)

nx_add_shader_program(doggo_gate0_triangle_fsh
        "${GATE0_SHADER_SOURCE_DIR}/gate0_triangle_fsh.glsl"
        frag
)

dkp_add_asset_target(${APP_ROMFS_TARGET}
        "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
)

dkp_track_assets(${APP_ROMFS_TARGET}
        FOLDER gate0
        FILES read_fixture.bin
)

dkp_install_assets(${APP_ROMFS_TARGET}
        DESTINATION shaders/gate0
        TARGETS
        doggo_gate0_triangle_vsh
        doggo_gate0_triangle_fsh
)

nx_generate_nacp(${APP_TARGET}.nacp
        NAME "${APP_TITLE}"
        AUTHOR "${APP_AUTHOR}"
        VERSION "${APP_VERSION}"
)

nx_create_nro(${APP_TARGET}
        NACP ${APP_TARGET}.nacp
        ROMFS ${APP_ROMFS_TARGET}
)
