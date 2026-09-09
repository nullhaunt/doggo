add_library(doggo_platform_nx STATIC
        nx_AppletLifecycle.cpp
        nx_Application.cpp
        nx_Memory.cpp
        nx_MonotonicClock.cpp
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
        PRIVATE
        nx
)
