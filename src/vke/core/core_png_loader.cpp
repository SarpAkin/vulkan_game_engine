#include "core.hpp"

#include <fmt/core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace vke
{
std::unique_ptr<Image> Core::load_png(const char* path, VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue)
{
    int tex_width, tex_height, tex_channels;

    stbi_uc* pixels = stbi_load(path, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    if (!pixels)
    {
        throw std::runtime_error(fmt::format("failed to load file: {}", path));
    }

    size_t buf_size = tex_width * tex_height * 4;

    auto stencil = this->allocate_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, buf_size, true);

    memcpy(stencil->get_data(), pixels, buf_size);

    auto texture = this->allocate_image(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, tex_width, tex_height, false);

    VkImageMemoryBarrier image_transfer_barrier = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask    = 0,
        .dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image            = texture->image,
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    // barrier the image into the transfer-receive layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_transfer_barrier);

    VkBufferImageCopy copy_region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = VkImageSubresourceLayers{
             .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
             .mipLevel       = 0,
             .baseArrayLayer = 0,
             .layerCount     = 1,
        },
        .imageExtent = VkExtent3D{
            .width  = static_cast<uint32_t>(tex_width),
            .height = static_cast<uint32_t>(tex_height),
            .depth  = 1,
        },
    };

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stencil->buffer(), texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    VkImageMemoryBarrier image_readable_barrier = image_transfer_barrier;

    image_readable_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_readable_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_readable_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_readable_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // barrier the image into the shader readable layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_readable_barrier);

    cleanup_queue.push_back([stencil = std::shared_ptr(std::move(stencil))]() mutable {
        stencil->clean_up();
    });

    return texture;
}

} // namespace vke
