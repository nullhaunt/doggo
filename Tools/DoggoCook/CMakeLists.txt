cmake_minimum_required(VERSION 3.20)

project(DoggoCook
        VERSION 0.1.0
        DESCRIPTION "DOGGO deterministic asset cooker"
        LANGUAGES CXX
)

option(DOGGO_COOK_BUILD_TESTS "Build DoggoCook host tests" ON)

add_library(doggo-cook-library STATIC
        src/ShaderCooker.cpp
)

target_include_directories(doggo-cook-library
        PUBLIC
        src
)

target_compile_features(doggo-cook-library
        PUBLIC
        cxx_std_20
)

set_target_properties(doggo-cook-library PROPERTIES
        CXX_EXTENSIONS OFF
)

add_executable(doggo-cook
        src/Main.cpp
)

target_link_libraries(doggo-cook
        PRIVATE
        doggo-cook-library
)

function(doggo_cook_enable_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif ()
endfunction()

doggo_cook_enable_warnings(doggo-cook-library)
doggo_cook_enable_warnings(doggo-cook)

if (DOGGO_COOK_BUILD_TESTS)
    enable_testing()

    add_executable(doggo-cook-tests
            tests/ShaderCookerTests.cpp
    )

    target_link_libraries(doggo-cook-tests
            PRIVATE
            doggo-cook-library
    )

    doggo_cook_enable_warnings(doggo-cook-tests)
    add_test(NAME doggo-cook-tests COMMAND doggo-cook-tests)
endif ()
