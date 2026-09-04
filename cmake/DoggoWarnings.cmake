add_library(doggo_warnings INTERFACE)

add_library(doggo::warnings ALIAS doggo_warnings)

if (MSVC)
    target_compile_options(
            doggo_warnings
            INTERFACE
            /W4
            /permissive-
            /Zc:__cplusplus
    )

    if (DOGGO_WARNINGS_AS_ERRORS)
        target_compile_options(
                doggo_warnings
                INTERFACE
                /WX
        )
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
            doggo_warnings
            INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wundef
            -Wformat=2
            -Wcast-align
            -Wnon-virtual-dtor
            -Woverloaded-virtual
    )

    if (DOGGO_WARNINGS_AS_ERRORS)
        target_compile_options(
                doggo_warnings
                INTERFACE
                -Werror
        )
    endif ()
else ()
    message(
            FATAL_ERROR
            "DOGGO does not currently define a warning policy for compiler: "
            "${CMAKE_CXX_COMPILER_ID}"
    )
endif ()