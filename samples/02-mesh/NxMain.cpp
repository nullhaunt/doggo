#include "DekoBackend.hpp"
#include "NxApplication.hpp"
#include "NxInput.hpp"

int main()
{
    doggo::platform::nx::NxApplication     host;
    doggo::platform::nx::NxInput           input;
    doggo::platform::nx::deko::DekoBackend renderer;

    return doggo::run( host, input, renderer );
}
