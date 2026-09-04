add_library(doggo_core STATIC)

add_library(doggo::core ALIAS doggo_core)

target_sources(
        doggo_core
        PRIVATE
        Doggo.cpp
        Doggo.hpp
)

target_include_directories(
        doggo_core
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
        doggo_core
        PUBLIC
        cxx_std_20
)

target_link_libraries(
        doggo_core
        PRIVATE
        doggo::warnings
)

add_subdirectory(logging)
add_subdirectory(time)
add_subdirectory(diagnostics)
add_subdirectory(memory)
add_subdirectory(profiling)
