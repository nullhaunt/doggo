add_library(
        doggo_platform_application
        STATIC
        Application.cpp
        Application.hpp
)

add_library(
        doggo::platform_application
        ALIAS
        doggo_platform_application
)

target_include_directories(
        doggo_platform_application
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_platform_application
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_platform_application
        PUBLIC
        doggo::core
        PRIVATE
        doggo::warnings
)