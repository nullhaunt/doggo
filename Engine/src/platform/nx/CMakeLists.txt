add_library(doggo_platform_nx STATIC
        nx_AppletLifecycle.cpp
        nx_Input.cpp
        nx_LogSinks.cpp
        nx_Memory.cpp
        nx_MonotonicClock.cpp
        nx_RomFs.cpp
        nx_SdCardSaveStorage.cpp
)

add_library(doggo::platform::nx ALIAS doggo_platform_nx)

target_include_directories(doggo_platform_nx
        PUBLIC
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_platform_nx
        PUBLIC
        cxx_std_20
)

target_link_libraries(doggo_platform_nx
        PUBLIC
        doggo::log
        doggo::save
        nx
)
