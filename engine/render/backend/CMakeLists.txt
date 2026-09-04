add_library(
        doggo_render_backend
        STATIC
        RenderBackend.cpp
        RenderBackend.hpp
)

add_library(doggo::render_backend ALIAS doggo_render_backend)

target_include_directories(
        doggo_render_backend
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_render_backend
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_render_backend
        PRIVATE
        doggo::warnings
)