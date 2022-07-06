#include "core.hpp"

#include "../vkutil.hpp"

#include <vk_mem_alloc.h>

namespace vke
{

size_t Core::pad_buffer(size_t bsize) const
{
    size_t alignment = gpu_allignment();
    if (bsize > 0)
    {
        return (bsize + alignment - 1) & ~(alignment - 1);
    }
    return 0;
}

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

std::unique_ptr<Buffer> Core::allocate_buffer(VkBufferUsageFlagBits usage, uint32_t buffer_size, bool host_visible)
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

    auto buffer           = std::make_unique<Buffer>();
    buffer->m_allocator   = m_allocator;
    buffer->m_buffer_size = buffer_size;

    VK_CHECK(vmaCreateBuffer(m_allocator, &buffer_info, &alloc_info, &buffer->m_buffer, &buffer->m_allocation, nullptr));

    if (host_visible)
    {
        VK_CHECK(vmaMapMemory(m_allocator, buffer->m_allocation, &buffer->m_mapped_data));
    }

    return buffer;
}

Image::Image(Core* core, VkFormat format, VkImageUsageFlags usage_flags, uint32_t width, uint32_t height, uint32_t layers, bool host_visible)
{
    m_core = core;

    VkImageCreateInfo ic_info = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .extent      = {width, height, 1},
        .mipLevels   = 1,
        .arrayLayers = layers,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = host_visible ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
        .usage       = usage_flags,
    };

    VmaAllocationCreateInfo dimg_allocinfo = {
        .flags = host_visible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : 0u,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VK_CHECK(vmaCreateImage(core->allocator(), &ic_info, &dimg_allocinfo, &image, &m_allocation, nullptr));

    VkImageViewCreateInfo ivc_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image,
        .viewType         = layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format           = format,
        .subresourceRange = {
            .aspectMask     = is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = layers,
        },
    };

    VK_CHECK(vkCreateImageView(core->device(), &ivc_info, nullptr, &view));

    if (host_visible)
    {
        VK_CHECK(vmaMapMemory(core->allocator(), m_allocation, &m_mapped_data));
    }
    else
    {
        m_mapped_data = nullptr;
    }
}

ImageArray::ImageArray(Core* core, VkFormat format, VkImageUsageFlags usage_flags, uint32_t width, uint32_t height, uint32_t layers, bool host_visible)
    : Image(core, format, usage_flags, width, height, layers, host_visible)
{
    layered_views.resize(layers);

    for (uint32_t i = 0; i < layers; ++i)
    {
        VkImageViewCreateInfo ivc_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = format,
            .subresourceRange = {
                .aspectMask     = is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = i,
                .layerCount     = 1,
            },
        };

        VK_CHECK(vkCreateImageView(core->device(), &ivc_info, nullptr, &layered_views[i]));
    }
}

void ImageArray::clean_up()
{
    Image::clean_up();

    for (auto& layer : layered_views)
    {
        vkDestroyImageView(m_core->device(), layer, nullptr);
    }
}

ImageArray::~ImageArray()
{
}

std::unique_ptr<Image> Core::allocate_image(VkFormat format, VkImageUsageFlags usage_flags, uint32_t width, uint32_t height, bool host_visible)
{
    return std::make_unique<Image>(this, format, usage_flags, width, height, 1, host_visible);
}

std::unique_ptr<ImageArray> Core::allocate_image_array(VkFormat format, VkImageUsageFlags usage_flags, uint32_t width, uint32_t height, uint32_t layer_count, bool host_visible)
{
    return std::make_unique<ImageArray>(this, format, usage_flags, width, height, layer_count, host_visible);
}

void Buffer::clean_up()
{
    if (m_mapped_data) vmaUnmapMemory(m_allocator, m_allocation);
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);

    m_allocation = nullptr;
    m_buffer     = nullptr;
}

void Image::clean_up()
{
    if (m_mapped_data) vmaUnmapMemory(m_core->allocator(), m_allocation);

    vkDestroyImageView(m_core->device(), view, nullptr);
    vmaDestroyImage(m_core->allocator(), image, m_allocation);

    m_allocation = nullptr;
    this->image  = nullptr;
}

Buffer::~Buffer()
{
    if (m_buffer)
    {
        fmt::print("buffer did not freed\n");
    }
}

Image::~Image()
{
    if (image)
    {
        fmt::print("image did not freed\n");
    }
}

VkBufferMemoryBarrier Core::buffer_barrier(Buffer* buffer, VkAccessFlags src_access, VkAccessFlags dst_acces)
{
    return VkBufferMemoryBarrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask       = src_access,
        .dstAccessMask       = dst_acces,
        .srcQueueFamilyIndex = m_graphics_queue_family,
        .dstQueueFamilyIndex = m_graphics_queue_family,
        .buffer              = buffer->buffer(),
        .offset              = 0,
        .size                = VK_WHOLE_SIZE,
    };
}

} // namespace vke