#include "DekoBackend.hpp"
#include "MeshSample.hpp"
#include "NxApplication.hpp"
#include "NxInput.hpp"

int main()
{
    doggo::platform::nx::NxApplication host;
    doggo::platform::nx::NxInput       input;

    doggo::sample::mesh::MeshSample        sample;
    doggo::platform::nx::deko::DekoBackend backend;
    doggo::render::Renderer                renderer{ backend, sample };

    return doggo::run( host, input, renderer );
}
