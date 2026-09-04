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
