cmake_minimum_required(VERSION 3.20)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DoggoHostTests
            VERSION 0.1.0
            DESCRIPTION "DOGGO platform-independent host tests"
            LANGUAGES CXX
    )
    enable_testing()
endif ()

get_filename_component(DOGGO_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(doggo-host-tests
        "${DOGGO_WORKSPACE_ROOT}/Engine/src/gpu/gpu_ArenaAllocator.cpp"
        tests/GpuArenaAllocatorTests.cpp
)

target_include_directories(doggo-host-tests
        PRIVATE
        "${DOGGO_WORKSPACE_ROOT}/Engine/include"
)

target_compile_features(doggo-host-tests
        PRIVATE
        cxx_std_20
)

set_target_properties(doggo-host-tests PROPERTIES
        CXX_EXTENSIONS OFF
)

if (MSVC)
    target_compile_options(doggo-host-tests PRIVATE /W4 /WX /permissive-)
else ()
    target_compile_options(doggo-host-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

add_test(NAME doggo-host-tests COMMAND doggo-host-tests)
