#include <fmt/format.h>

#include <iostream>

#include "vulkan/core/core.hpp"

int main()
{
    fmt::print("hello world\n");

    auto core = vke::Core(500,500,"app");
    
    std::cin.get();

    return 0;
}