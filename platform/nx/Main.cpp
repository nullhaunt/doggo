#include "NxApplication.hpp"
#include "NxInput.hpp"

int main()
{
    doggo::platform::nx::NxApplication host;
    doggo::platform::nx::NxInput       input;
    return doggo::run( host, input );
}
