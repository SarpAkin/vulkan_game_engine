#include "renderpass.hpp"

#include <cassert>
#include <memory>

#include "core/core.hpp"

#include "vkutil.hpp"

#include "util.hpp"

namespace vke
{

struct RenderPassBuilder::SubpassDesc
{
    std::vector<VkAttachmentReference> attachments;
    std::unique_ptr<VkAttachmentReference> depth_attachment;
    std::vector<VkAttachmentReference> input_attachments;
    VkSubpassDescription description;
};

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
        .loadOp         = clear_value ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = is_depth_format(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    });

    m_clear_values.push_back(clear_value.value_or(VkClearValue{}));

    return m_attachments.size() - 1;
}

uint32_t RenderPassBuilder::add_swapchain_attachment(Core* core, std::optional<VkClearValue> clear_value)
{
    assert(core);

    uint32_t index = add_attachment(core->swapchain_image_format(), clear_value);

    m_attachments[index].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    m_swapchain_attachment = SwapChainAttachment{
        .views = core->swapchain_image_views(),
        .index = index,
    };

    return index;
}

void RenderPassBuilder::add_subpass(const std::vector<uint32_t>& attachments_ids, const std::optional<uint32_t>& depth_stencil_attachment, const std::vector<uint32_t>& input_attachments)
{
    // assert(input_attachments.size() == 0 && "input attachments are'nt supported right now");

    m_subpasses.emplace_back();
    auto& subpass = m_subpasses.back();

    subpass.attachments = map_vec(attachments_ids, [&](uint32_t attachment_index) {
        return VkAttachmentReference{
            .attachment = attachment_index,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
    });

    subpass.input_attachments = map_vec(input_attachments, [&](uint32_t attachment_index) {
        return VkAttachmentReference{
            .attachment = attachment_index,
            .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    });

    subpass.description = VkSubpassDescription{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = static_cast<uint32_t>(subpass.input_attachments.size()),
        .pInputAttachments    = subpass.input_attachments.data(),
        .colorAttachmentCount = static_cast<uint32_t>(subpass.attachments.size()),
        .pColorAttachments    = subpass.attachments.data(),
    };

    if (depth_stencil_attachment)
    {
        subpass.depth_attachment = std::make_unique<VkAttachmentReference>(VkAttachmentReference{
            .attachment = *depth_stencil_attachment,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });

        subpass.description.pDepthStencilAttachment = subpass.depth_attachment.get();
    }
}

struct RenderPassArgs
{
    Core* core;
    VkRenderPass render_pass;
    std::vector<RenderPass::Attachmen> attachments;
    uint32_t width, height;
    std::optional<RenderPassBuilder::SwapChainAttachment> swc_att;
    std::vector<VkClearValue> clear_values;
    std::vector<RenderPass::Subpass> subpasses;
};

std::unique_ptr<RenderPass> RenderPassBuilder::build(Core* core, uint32_t width, uint32_t height)
{
    assert(core);

    auto dependencies = std::vector<VkSubpassDependency>();

    auto attachement_uses = std::vector<int>(m_attachments.size(), -1);

    auto rp_args_att = map_vec(m_attachments, [](const VkAttachmentDescription& des) {
        return RenderPass::Attachmen{
            .format = des.format,
            .layout = des.finalLayout,
        };
    });

    for (uint32_t i = 0; i < m_subpasses.size(); ++i)
    {
        auto& subpass = m_subpasses[i];

        for (auto& att : subpass.attachments)
        {
            int& att_use = attachement_uses[att.attachment];
            if (att_use == -1)
            {
                att_use = i;
            }
            else
            {
                dependencies.push_back(VkSubpassDependency{
                    .srcSubpass      = static_cast<uint32_t>(att_use),
                    .dstSubpass      = i,
                    .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                });
            }
        }

        if (subpass.depth_attachment)
        {
            auto& att    = *subpass.depth_attachment;
            int& att_use = attachement_uses[att.attachment];
            if (att_use == -1)
            {
                att_use = i;
            }
            else
            {
                dependencies.push_back(VkSubpassDependency{
                    .srcSubpass      = static_cast<uint32_t>(att_use),
                    .dstSubpass      = i,
                    .srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                });
            }
        }

        for (auto& input_att : subpass.input_attachments)
        {
            int& att_use   = attachement_uses[input_att.attachment];
            auto& att_desc = m_attachments[input_att.attachment];
            bool is_depth  = is_depth_format(att_desc.format);

            rp_args_att[input_att.attachment].is_input_attachment = true;

            if (att_use == -1)
            {
                assert("input attachment must be a previously use dattachment" && 0);
            }
            else
            {
                dependencies.push_back(VkSubpassDependency{
                    .srcSubpass      = static_cast<uint32_t>(att_use),
                    .dstSubpass      = i,
                    .srcStageMask    = is_depth ? (VkPipelineStageFlags)(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask   = is_depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                });
            }
        }
    }

    dependencies = [&] {
        auto new_dependencies = std::vector<VkSubpassDependency>();

        for (auto& dep : dependencies)
        {
            for (auto& ndep : new_dependencies)
            {
                if (ndep.srcSubpass == dep.srcSubpass && ndep.dstSubpass == dep.dstSubpass)
                {
                    ndep.srcStageMask |= dep.srcStageMask;
                    ndep.dstStageMask |= dep.dstStageMask;
                    ndep.srcAccessMask |= dep.srcAccessMask;
                    ndep.dstAccessMask |= dep.dstAccessMask;
                    ndep.dependencyFlags |= dep.dependencyFlags;
                    goto end;
                }
            }

            new_dependencies.push_back(dep);
        end:;
        }

        return new_dependencies;
    }();

    auto subpassses = map_vec(m_subpasses, [](const SubpassDesc& d) { return d.description; });

    VkRenderPassCreateInfo render_pass_info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments    = m_attachments.data(),
        .subpassCount    = static_cast<uint32_t>(subpassses.size()),
        .pSubpasses      = subpassses.data(),
        .dependencyCount = static_cast<uint32_t>(dependencies.size()),
        .pDependencies   = dependencies.data(),
    };

    VkRenderPass render_pass;

    VK_CHECK(vkCreateRenderPass(core->device(), &render_pass_info, nullptr, &render_pass));

    return std::make_unique<RenderPass>(RenderPassArgs{
        .core        = core,
        .render_pass = render_pass,
        .attachments = [&] {
            if (m_swapchain_attachment)
            {
                rp_args_att[m_swapchain_attachment->index].external              = true;
                rp_args_att[m_swapchain_attachment->index].swap_chain_attachment = true;
            }

            return rp_args_att;
        }(),
        .width        = width,
        .height       = height,
        .swc_att      = std::move(m_swapchain_attachment),
        .clear_values = m_clear_values,
        .subpasses    = map_vec(m_subpasses, [&](SubpassDesc& desc) {
            return RenderPass::Subpass{
                   .attachments = map_vec(desc.attachments, [&](VkAttachmentReference& d) { return d.attachment; }),
                   .depth_att   = desc.depth_attachment ? std::make_optional(desc.depth_attachment->attachment) : std::nullopt,
            };
           }),
    });
} // namespace vke

RenderPass::RenderPass(RenderPassArgs args)
    : m_core(args.core), m_renderpass(args.render_pass), m_width(args.width), m_height(args.height),
      m_attachments(std::move(args.attachments)), m_clear_values(args.clear_values), m_subpasses(std::move(args.subpasses))
{
    for(int i = 0;i < m_subpasses.size();++i)
    {
        m_subpasses[i].render_pass = this;
        m_subpasses[i].subpass_index = i;
    }

    if (args.swc_att)
    {
        m_swapchain_attachment_index = args.swc_att->index;
        m_swapchain_image_views      = std::move(args.swc_att->views);
    }

    create_frame_buffers();

    m_core->set_window_renderpass(this);
}

void RenderPass::create_frame_buffers()
{
    for (auto& att : m_attachments)
    {
        if (att.external) continue;

        auto im = m_core->allocate_image(att.format,
            (is_depth_format(att.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) //
                | (att.is_input_attachment ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0),
            m_width, m_height, false);

        att.image = im->image;
        att.view  = im->view;

        m_images.push_back(std::move(im));
    }

    auto attachment_views = map_vec(m_attachments, [](const Attachmen& att) { return att.view; });

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

        VK_CHECK(vkCreateFramebuffer(m_core->device(), &fb_info, nullptr, &m_framebuffers[i]));
    }
}

void RenderPass::clean_frame_buffers()
{
    for (auto& image : m_images)
        image->clean_up();

    for (auto& fb : m_framebuffers)
        vkDestroyFramebuffer(m_core->device(), fb, nullptr);
}

void RenderPass::clean()
{
    clean_frame_buffers();
    vkDestroyRenderPass(m_core->device(), m_renderpass, nullptr);
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
        .offset = {0, 0},
        .extent = {m_width, m_height},
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void RenderPass::next_subpass(VkCommandBuffer cmd)
{
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::end(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

} // namespace vke