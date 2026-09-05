add_library(
        doggo_windows_platform
        STATIC
        WindowsApplication.cpp
        WindowsApplication.hpp
)

add_library(doggo::windows_platform ALIAS doggo_windows_platform)

target_include_directories(
        doggo_windows_platform
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_windows_platform
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_windows_platform
        PUBLIC
        doggo::platform_application
        PRIVATE
        doggo::warnings
)
