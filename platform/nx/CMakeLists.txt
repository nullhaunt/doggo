add_executable(
        doggo
        Main.cpp
)

target_link_libraries(
        doggo
        PRIVATE
        doggo::core
        doggo::warnings
)

target_compile_features(
        doggo
        PRIVATE
        cxx_std_20
)

nx_create_nro(doggo)