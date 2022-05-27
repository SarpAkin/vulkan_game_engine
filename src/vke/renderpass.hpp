#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

// #include "vulkan/core/core.hpp"

namespace vke
{

// core/core.hpp
class Core;
class Image;

class RenderPassBuilder;
struct RenderPassArgs;

class RenderPass
{
public:
    struct Subpass
    {
        RenderPass* render_pass;
        std::vector<uint32_t> attachments;
        uint32_t subpass_index;
    };

    struct Attachments
    {
        VkFormat format;
        VkImageLayout layout;
        VkImage image;
        VkImageView view;
        bool external;
        bool swap_chain_attachment;
    };

    RenderPass(RenderPassArgs args);

    void begin(VkCommandBuffer cmd);
    void next_subpass(VkCommandBuffer cmd);
    void end(VkCommandBuffer cmd);

    inline void set_swapchain_image_index(uint32_t index) { m_framebuffer_index = index; }
    inline auto renderpass() { return m_renderpass; }
    inline void set_attachment_clear_value(uint32_t index, VkClearValue val)
    {
        if (m_clear_values.size() > index) m_clear_values[index] = val;
    }
    inline glm::vec2 size(){return glm::vec2(m_width,m_height);}

    void clean();

private:
    void create_frame_buffers();
    void clean_frame_buffers();

private:
    Core* m_core;
    VkRenderPass m_renderpass;
    uint32_t m_width, m_height;
    std::vector<Attachments> m_attachments;
    std::vector<std::unique_ptr<Image>> m_images;
    std::vector<Subpass> m_subpasses;
    std::vector<VkFramebuffer> m_framebuffers;
    uint32_t m_framebuffer_index = 0;
    uint32_t m_swapchain_attachment_index;
    std::vector<VkImageView> m_swapchain_image_views;
    std::vector<VkClearValue> m_clear_values;
};

class RenderPassBuilder
{
public:
    struct SwapChainAttachment
    {
        std::vector<VkImageView> views;
        uint32_t index;
    };

    RenderPassBuilder();
    ~RenderPassBuilder();

    uint32_t
    add_attachment(VkFormat format, std::optional<VkClearValue> clear_value = std::nullopt);
    uint32_t add_swapchain_attachment(Core* core, std::optional<VkClearValue> clear_value = std::nullopt);
    void add_subpass(const std::vector<uint32_t>& attachments_ids, const std::optional<uint32_t>& depth_stencil_attachment = std::nullopt, const std::vector<uint32_t>& input_attachments = {});
    std::unique_ptr<RenderPass> build(Core* core, uint32_t width, uint32_t height);

private:
    struct SubpassDesc;

    std::vector<VkAttachmentDescription> m_attachments;
    std::vector<SubpassDesc> m_subpasses;
    std::optional<SwapChainAttachment> m_swapchain_attachment;
    std::vector<VkClearValue> m_clear_values;
};

} // namespace vke