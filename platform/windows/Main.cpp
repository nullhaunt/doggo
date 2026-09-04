#include "WindowsApplication.hpp"
#include "RenderBackend.hpp"

int main()
{
    doggo::platform::windows::WindowsApplication host;
    doggo::input::NullBackend                    input;
    doggo::render::NullBackend                   render;

    return doggo::run( host, input, render );
}
