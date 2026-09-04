#include "Doggo.hpp"

namespace doggo
{
    std::string_view getName() noexcept
    {
        return "DOGGO";
    }

    std::string_view getDescription() noexcept
    {
        return "Data-Oriented Geometry & Gameplay Orchestrator";
    }
} // namespace doggo
