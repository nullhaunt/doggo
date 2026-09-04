#include <format>
#include <iostream>

#include "Doggo.hpp"

int main()
{
    std::cout << std::format( "{}\n"
                              "{}\n\n"
                              "Platform: Windows\n\n"
                              "DOGGO bootstrap OK.",
                              doggo::getName(), doggo::getDescription() );

    return 0;
}