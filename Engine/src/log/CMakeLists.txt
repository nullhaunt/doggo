add_library(doggo_log STATIC
        log_Log.cpp
)

add_library(doggo::log ALIAS doggo_log)

target_include_directories(doggo_log
        PUBLIC
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_log
        PUBLIC
        cxx_std_20
)
