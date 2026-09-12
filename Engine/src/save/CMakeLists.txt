add_library(doggo_save INTERFACE)

add_library(doggo::save ALIAS doggo_save)

target_include_directories(doggo_save
        INTERFACE
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_save
        INTERFACE
        cxx_std_20
)
