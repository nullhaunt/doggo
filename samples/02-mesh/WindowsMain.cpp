#include "MeshSample.hpp"
#include "RenderBackend.hpp"
#include "Renderer.hpp"
#include "WindowsApplication.hpp"

int main()
{
    doggo::platform::windows::WindowsApplication host;
    doggo::input::NullBackend                    input;

    doggo::sample::mesh::MeshSample sample;
    doggo::render::NullBackend      backend;
    doggo::render::Renderer         renderer{ backend, sample };

    return doggo::run( host, input, renderer );
}
