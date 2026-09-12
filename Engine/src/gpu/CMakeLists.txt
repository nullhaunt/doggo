add_library(doggo_gpu STATIC
        gpu_ArenaAllocator.cpp
)

add_library(doggo::gpu ALIAS doggo_gpu)

target_include_directories(doggo_gpu
        PUBLIC
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_gpu
        PUBLIC
        cxx_std_20
)

add_subdirectory(deko)
