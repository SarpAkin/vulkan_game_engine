#include "renderpass.hpp"

#include <cassert>
#include <memory>

#include "core/core.hpp"

#include "vkutil.hpp"

#include "util.hpp"

namespace vke
{

RenderPassBuilder::RenderPassBuilder()
{
}

RenderPassBuilder::~RenderPassBuilder()
{
}

uint32_t RenderPassBuilder::add_attachment(VkFormat format, std::optional<VkClearValue> clear_value)
{
    m_attachments.push_back(VkAttachmentDescription{
        .format         = format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = clear_value ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = is_depth_format(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    });

    m_clear_values.push_back(clear_value.value_or(VkClearValue{}));

    return m_attachments.size() - 1;
}

uint32_t RenderPassBuilder::add_swapchain_attachment(Core& core, std::optional<VkClearValue> clear_value)
{
    uint32_t index = add_attachment(core.swapchain_image_format(), clear_value);

    m_attachments[index].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    m_swapchain_attachment = SwapChainAttachment{
        .views = core.swapchain_image_views(),
        .index = index,
    };

    return index;
}

struct RenderPassBuilder::SubpassDesc
{
    std::vector<VkAttachmentReference> attachments;
    std::unique_ptr<VkAttachmentReference> dp_ref;
    VkSubpassDescription description;
};

void RenderPassBuilder::add_subpass(const std::vector<uint32_t>& attachments_ids, const std::optional<uint32_t>& depth_stencil_attachment, const std::vector<uint32_t>& input_attachments)
{
    assert(input_attachments.size() == 0 && "input attachments are'nt supported right now");

    m_subpasses.emplace_back();
    auto& subpass = m_subpasses.back();

    subpass.attachments = map_vec(attachments_ids, [&](uint32_t attachment_index) {
        return VkAttachmentReference{
            .attachment = attachment_index,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
    });

    subpass.description = VkSubpassDescription{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = static_cast<uint32_t>(subpass.attachments.size()),
        .pColorAttachments    = subpass.attachments.data(),
    };

    if (depth_stencil_attachment)
    {
        subpass.dp_ref = std::make_unique<VkAttachmentReference>(VkAttachmentReference{
            .attachment = *depth_stencil_attachment,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });

        subpass.description.pDepthStencilAttachment = subpass.dp_ref.get();
    }
}

struct RenderPassArgs
{
    Core& core;
    VkRenderPass render_pass;
    std::vector<RenderPass::Attachments> attachments;
    uint32_t width, height;
    std::optional<RenderPassBuilder::SwapChainAttachment> swc_att;
    std::vector<VkClearValue> clear_values;
};

std::unique_ptr<RenderPass> RenderPassBuilder::build(Core& core, uint32_t width, uint32_t height)
{
    auto subpassses = map_vec(m_subpasses, [](const SubpassDesc& d) { return d.description; });

    VkRenderPassCreateInfo render_pass_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments    = m_attachments.data(),
        .subpassCount    = static_cast<uint32_t>(subpassses.size()),
        .pSubpasses      = subpassses.data(),

    };

    VkRenderPass render_pass;

    VK_CHECK(vkCreateRenderPass(core.device(), &render_pass_info, nullptr, &render_pass));

    return std::make_unique<RenderPass>(RenderPassArgs{
        .core        = core,
        .render_pass = render_pass,
        .attachments = [&] {
            auto vec = map_vec(m_attachments, [](const VkAttachmentDescription& des) {
                return RenderPass::Attachments{
                    .format = des.format,
                    .layout = des.finalLayout,
                };
            });

            if (m_swapchain_attachment)
            {
                vec[m_swapchain_attachment->index].external              = true;
                vec[m_swapchain_attachment->index].swap_chain_attachment = true;
            }

            return vec;
        }(),
        .width        = width,
        .height       = height,
        .swc_att      = std::move(m_swapchain_attachment),
        .clear_values = m_clear_values});
}

RenderPass::RenderPass(RenderPassArgs args)
    : m_core(args.core), m_renderpass(args.render_pass), m_width(args.width), m_height(args.height),
      m_attachments(std::move(args.attachments)), m_clear_values(args.clear_values)
{
    if (args.swc_att)
    {
        m_swapchain_attachment_index = args.swc_att->index;
        m_swapchain_image_views      = std::move(args.swc_att->views);
    }

    create_frame_buffers();

    m_core.set_window_renderpass(this);
}

void RenderPass::create_frame_buffers()
{
    for (auto& att : m_attachments)
    {
        if (att.external) continue;

        Image im = m_core.allocate_image(att.format,
            is_depth_format(att.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            m_width, m_height, false);

        att.image = im.image;
        att.view  = im.view;

        m_images.push_back(im);
    }

    auto attachment_views = map_vec(m_attachments, [](const Attachments& att) { return att.view; });

    uint32_t framebuffer_size = m_swapchain_image_views.size() != 0 ? m_swapchain_image_views.size() : 1;
    m_framebuffers.resize(framebuffer_size);

    for (int i = 0; i < framebuffer_size; ++i)
    {
        if (m_swapchain_image_views.size())
        {
            attachment_views[m_swapchain_attachment_index] = m_swapchain_image_views[i];
        }

        VkFramebufferCreateInfo fb_info = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = m_renderpass,
            .attachmentCount = static_cast<uint32_t>(attachment_views.size()),
            .pAttachments    = attachment_views.data(),
            .width           = m_width,
            .height          = m_height,
            .layers          = 1,
        };

        VK_CHECK(vkCreateFramebuffer(m_core.device(), &fb_info, nullptr, &m_framebuffers[i]));
    }
}

void RenderPass::clean_frame_buffers()
{
    for (auto& image : m_images)
        image.clean_up();

    for (auto& fb : m_framebuffers)
        vkDestroyFramebuffer(m_core.device(), fb, nullptr);
}

void RenderPass::clean()
{
    clean_frame_buffers();
    vkDestroyRenderPass(m_core.device(), m_renderpass, nullptr);
}

void RenderPass::begin(VkCommandBuffer cmd)
{
    VkRenderPassBeginInfo rp_begin_info{
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = m_renderpass,
        .framebuffer = m_framebuffers[m_framebuffer_index],
        .renderArea  = {
             .offset = {0, 0},
             .extent = {m_width, m_height},
        },
        .clearValueCount = static_cast<uint32_t>(m_clear_values.size()),
        .pClearValues    = m_clear_values.data(),
    };

    vkCmdBeginRenderPass(cmd, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport view_port = {
        .x        = 0.f,
        .y        = 0.f,
        .width    = static_cast<float>(m_width),
        .height   = static_cast<float>(m_height),
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    vkCmdSetViewport(cmd, 0, 1, &view_port);

    VkRect2D scissor = {
        .offset = {0,0},
        .extent = {m_width,m_height},
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void RenderPass::next_subpass(VkCommandBuffer cmd)
{
}

void RenderPass::end(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

} // namespace vke