if (NINTENDO_SWITCH)
    add_executable(
            doggo
            NxMain.cpp
    )

    target_link_libraries(
            doggo
            PRIVATE
            doggo::nx_platform
            doggo::nx_deko
            doggo::warnings
    )

    target_compile_features(
            doggo
            PRIVATE
            cxx_std_20
    )

    nx_create_nro(
            doggo
            ROMFS doggo_nx_romfs
    )

elseif (WIN32)
    add_executable(
            doggo
            WindowsMain.cpp
    )

    target_link_libraries(
            doggo
            PRIVATE
            doggo::windows_platform
            doggo::platform_input
            doggo::render_backend
            doggo::warnings
    )

    target_compile_features(
            doggo
            PRIVATE
            cxx_std_20
    )

else ()
    message(
            FATAL_ERROR
            "DOGGO mesh sample does not support platform: ${CMAKE_SYSTEM_NAME}"
    )
endif ()
