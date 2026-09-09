cmake_minimum_required(VERSION 3.20)

project(DoggoDev
        VERSION 0.1.0
        DESCRIPTION "DOGGO Nintendo Switch deploy-and-log tool"
        LANGUAGES CXX
)

add_executable(doggo-dev
        src/Main.cpp
)

target_compile_features(doggo-dev
        PRIVATE
        cxx_std_20
)

set_target_properties(doggo-dev PROPERTIES
        CXX_EXTENSIONS OFF
)

if (MSVC)
    target_compile_options(doggo-dev PRIVATE
            /W4 /permissive-)
else ()
    target_compile_options(doggo-dev PRIVATE
            -Wall -Wextra -Wpedantic)
endif ()
