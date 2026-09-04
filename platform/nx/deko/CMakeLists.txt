nx_add_shader_program(
        doggo_triangle_vert
        ${PROJECT_SOURCE_DIR}/shaders/triangle.vert.glsl
        vert
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/triangle.vert.dksh"
)

nx_add_shader_program(
        doggo_triangle_frag
        ${PROJECT_SOURCE_DIR}/shaders/triangle.frag.glsl
        frag
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/triangle.frag.dksh"
)

dkp_add_asset_target(
        doggo_nx_romfs
        "${CMAKE_CURRENT_BINARY_DIR}/romfs"
)

dkp_install_assets(
        doggo_nx_romfs
        DESTINATION shaders
        TARGETS
        doggo_triangle_vert
        doggo_triangle_frag
)

add_library(
        doggo_nx_deko
        STATIC
        DekoBackend.cpp
        DekoBackend.hpp
)

add_library(doggo::nx_deko ALIAS doggo_nx_deko)

target_include_directories(
        doggo_nx_deko
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_nx_deko
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_nx_deko
        PUBLIC
        doggo::render_backend
        PRIVATE
        doggo::core
        doggo::warnings
        deko3dd
)
