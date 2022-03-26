#pragma once

#include "vulkan/vulkan.h"

#include <cassert>

#include <fmt/format.h>

namespace vke
{
const char* vk_result_string(VkResult res);

#define VK_CHECK(x)                                                               \
    {                                                                             \
        VkResult result = x;                                                      \
        if (result != VK_SUCCESS)                                                 \
        {                                                                         \
            fmt::print(stderr, "[Vulkan Error]: {}\n", vk_result_string(result)); \
            assert(0);                                                            \
        }                                                                         \
    }

bool is_depth_format(VkFormat format);

} // namespace vke
