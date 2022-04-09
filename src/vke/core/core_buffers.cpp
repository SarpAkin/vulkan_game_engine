#include "core.hpp"

#include "../vkutil.hpp"

#include <vk_mem_alloc.h>

namespace vke
{

void Core::init_allocator()
{
    VmaVulkanFunctions vulkan_functions    = {};
    vulkan_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.vulkanApiVersion       = VK_API_VERSION_1_2;
    allocator_create_info.physicalDevice         = m_chosen_gpu;
    allocator_create_info.device                 = device();
    allocator_create_info.instance               = instance();
    allocator_create_info.pVulkanFunctions       = &vulkan_functions;

    VK_CHECK(vmaCreateAllocator(&allocator_create_info, &m_allocator));
}

void Core::cleanup_allocator()
{
    vmaDestroyAllocator(m_allocator);
}

Buffer Core::allocate_buffer(VkBufferUsageFlagBits usage, uint32_t buffer_size, bool host_visible)
{
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = buffer_size,
        .usage = usage,
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = host_visible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0u,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Buffer buffer      = {};
    buffer.m_allocator = m_allocator;
    buffer.m_buffer_size = buffer_size;

    VK_CHECK(vmaCreateBuffer(m_allocator, &buffer_info, &alloc_info, &buffer.m_buffer, &buffer.m_allocation, nullptr));

    if (host_visible)
    {
        VK_CHECK(vmaMapMemory(m_allocator, buffer.m_allocation, &buffer.m_mapped_data));
    }

    return buffer;
}

Image Core::allocate_image(VkFormat format, VkImageUsageFlags usageFlags, uint32_t width, uint32_t height, bool host_visible)
{
    VkImageCreateInfo ic_info = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .extent      = {width, height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = host_visible ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
        .usage       = usageFlags,
    };

    VkImage image;

    VmaAllocationCreateInfo dimg_allocinfo = {
        .flags = host_visible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0u,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocation allocation;

    VK_CHECK(vmaCreateImage(m_allocator, &ic_info, &dimg_allocinfo, &image, &allocation, nullptr));

    VkImageViewCreateInfo ivc_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = {
            .aspectMask     = is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    VkImageView view;

    VK_CHECK(vkCreateImageView(device(), &ivc_info, nullptr, &view));

    Image vke_image = {};

    vke_image.image        = image;
    vke_image.view         = view;
    vke_image.m_allocation = allocation;
    vke_image.m_core       = this;

    if (host_visible)
    {
        VK_CHECK(vmaMapMemory(m_allocator, allocation, &vke_image.m_mapped_data));
    }
    else
    {
        vke_image.m_mapped_data = nullptr;
    }

    return vke_image;
}

void Buffer::clean_up()
{
    if (m_mapped_data) vmaUnmapMemory(m_allocator, m_allocation);
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

void Image::clean_up()
{
    if (m_mapped_data) vmaUnmapMemory(m_core->allocator(), m_allocation);

    vkDestroyImageView(m_core->device(), view, nullptr);
    vmaDestroyImage(m_core->allocator(), image, m_allocation);
}

} // namespace vke