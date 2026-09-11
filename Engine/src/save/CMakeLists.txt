add_library(doggo_save STATIC
        save_Gate0Fixture.cpp
)

add_library(doggo::save ALIAS doggo_save)

target_include_directories(doggo_save
        PUBLIC
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_save
        PUBLIC
        cxx_std_20
)
