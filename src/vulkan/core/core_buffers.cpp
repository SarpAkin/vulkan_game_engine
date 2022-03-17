#include "core.hpp"

#include "../vkutil.hpp"

#define VMA_IMPLEMENTATION
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

    vmaCreateAllocator(&allocator_create_info, &m_allocator);
}

void Core::cleanup_allocator()
{
    vmaDestroyAllocator(m_allocator);
}

Buffer Core::allocate_buffer(VkBufferUsageFlagBits usage, uint32_t buffer_size, bool host_visible)
{
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size               = buffer_size;
    buffer_info.usage              = usage;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage                   = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags                   = host_visible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0;

    Buffer buffer      = {};
    buffer.m_allocator = m_allocator;

    VK_CHECK(vmaCreateBuffer(m_allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.m_allocation, nullptr));

    if (host_visible)
    {
        vmaMapMemory(m_allocator, buffer.m_allocation, &buffer.m_mapped_data);
    }

    return buffer;
}

void Buffer::clean_up()
{
    vmaDestroyBuffer(m_allocator, buffer, m_allocation);
}

} // namespace vke