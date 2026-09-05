add_subdirectory(deko)

add_library(
        doggo_nx_platform
        STATIC
        NxApplication.cpp
        NxApplication.hpp
        NxInput.cpp
        NxInput.hpp
)

add_library(doggo::nx_platform ALIAS doggo_nx_platform)

target_include_directories(
        doggo_nx_platform
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_nx_platform
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_nx_platform
        PUBLIC
        doggo::platform_application
        doggo::platform_input
        PRIVATE
        doggo::warnings
)
