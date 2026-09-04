if (NINTENDO_SWITCH)
    add_subdirectory(nx)
elseif (WIN32)
    add_subdirectory(windows)
else ()
    message(
            FATAL_ERROR
            "DOGGO does not currently support platform: ${CMAKE_SYSTEM_NAME}"
    )
endif ()