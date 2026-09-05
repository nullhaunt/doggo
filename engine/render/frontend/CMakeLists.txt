add_library(
        doggo_render_frontend
        STATIC
        Renderer.cpp
        Renderer.hpp
        RenderSource.hpp
)

add_library(doggo::render_frontend ALIAS doggo_render_frontend)

target_include_directories(
        doggo_render_frontend
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_render_frontend
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_render_frontend
        PUBLIC
        doggo::render_backend
        PRIVATE
        doggo::warnings
)
