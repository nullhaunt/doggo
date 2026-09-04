add_library(
        doggo_platform_input
        STATIC
        Input.cpp
        Input.hpp
)

add_library(
        doggo::platform_input
        ALIAS
        doggo_platform_input
)

target_include_directories(
        doggo_platform_input
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_platform_input
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_platform_input
        PRIVATE
        doggo::warnings
)