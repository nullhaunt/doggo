add_executable(
        doggo
        Main.cpp
        NxApplication.cpp
        NxApplication.hpp
        NxInput.cpp
        NxInput.hpp
)

target_link_libraries(
        doggo
        PRIVATE
        doggo::platform_application
        doggo::platform_input
        doggo::warnings
)

target_compile_features(
        doggo
        PRIVATE
        cxx_std_20
)

nx_create_nro(doggo)