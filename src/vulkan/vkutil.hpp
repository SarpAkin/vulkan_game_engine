#pragma once

#include "vulkan/vulkan.h"

#include <fmt/format.h>

#define VK_CHECK(x)                                             \
    {                                                           \
        VkResult result = x;                                    \
        if (result != VK_SUCCESS)                               \
        {                                                       \
            fmt::print(stderr, "[Vulkan Error]: {}\n", result); \
            assert(0);                                          \
        }                                                       \
    }
