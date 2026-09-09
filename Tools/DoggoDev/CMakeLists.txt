cmake_minimum_required(VERSION 3.20)

project(DoggoDev
        VERSION 0.1.0
        DESCRIPTION "DOGGO Nintendo Switch deploy-and-log tool"
        LANGUAGES CXX
)

find_package(ZLIB REQUIRED)

add_executable(doggo-dev
        src/Arguments.cpp
        src/Main.cpp
        src/NetloaderClient.cpp
        src/Network.cpp
)

target_include_directories(doggo-dev
        PRIVATE
        src
        ../../Engine/include
)

target_compile_features(doggo-dev
        PRIVATE
        cxx_std_20
)

set_target_properties(doggo-dev PROPERTIES
        CXX_EXTENSIONS OFF
)

target_link_libraries(doggo-dev
        PRIVATE
        ZLIB::ZLIB
)

if (WIN32)
    target_link_libraries(doggo-dev PRIVATE ws2_32)
endif ()

if (MSVC)
    target_compile_options(doggo-dev PRIVATE
            /W4 /permissive-)
else ()
    target_compile_options(doggo-dev PRIVATE
            -Wall -Wextra -Wpedantic)
endif ()
