add_library(doggo_gpu_deko STATIC
        deko_GraphicsContext.cpp
        deko_Presenter.cpp
)

add_library(doggo::gpu::deko ALIAS doggo_gpu_deko)

target_include_directories(doggo_gpu_deko
        PUBLIC
        $<BUILD_INTERFACE:${DOGGO_ENGINE_INCLUDE_DIR}>
)

target_compile_features(doggo_gpu_deko
        PUBLIC
        cxx_std_20
)

target_link_libraries(doggo_gpu_deko
        PUBLIC
        $<IF:$<CONFIG:Debug>,deko3dd,deko3d>
        PRIVATE
        doggo::log
)
