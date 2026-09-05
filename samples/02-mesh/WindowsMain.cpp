#include "RenderBackend.hpp"
#include "WindowsApplication.hpp"

int main()
{
    doggo::platform::windows::WindowsApplication host;
    doggo::input::NullBackend                    input;
    doggo::render::NullBackend                   renderer;

    return doggo::run( host, input, renderer );
}
