#include "WindowsApplication.hpp"

int main()
{
    doggo::platform::windows::WindowsApplication host;
    doggo::input::NullBackend                    input;
    return doggo::run( host, input );
}
