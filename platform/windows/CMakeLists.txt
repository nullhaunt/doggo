add_executable(
        doggo
        Main.cpp
        WindowsApplication.cpp
        WindowsApplication.hpp
)

target_link_libraries(
        doggo
        PRIVATE
        doggo::platform_application
        doggo::warnings
)

target_compile_features(
        doggo
        PRIVATE
        cxx_std_20
)