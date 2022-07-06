#include "pipeline_builder.hpp"

#include <regex>

#include "core/core.hpp"
#include "renderpass.hpp"
#include "util.hpp"
#include "vkutil.hpp"

extern const std::unordered_map<std::string, std::pair<const uint32_t*, uint32_t>> embeded_sprvs;

namespace vke
{

VkPipelineLayout PipelineLayoutBuilder::build(VkDevice device)
{
    VkPipelineLayoutCreateInfo info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = static_cast<uint32_t>(m_set_layouts.size()),
        .pSetLayouts            = m_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(m_push_constants.size()),
        .pPushConstantRanges    = m_push_constants.data(),
    };

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &layout));

    return layout;
}

std::optional<VkPipeline> ComputePipelineBuilder::build(Core* core)
{

    auto cleanup = [&] {
        for (auto& module : shader_stages)
            vkDestroyShaderModule(core->device(), module.module, nullptr);
    };

    assert(shader_stages.size() == 1);

    VkComputePipelineCreateInfo pipeline_info{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = shader_stages[0],
        .layout = pipeline_layout,
    };

    VkPipeline new_pipeline;
    if (vkCreateComputePipelines(core->device(), core->pipeline_cache(), 1, &pipeline_info, nullptr, &new_pipeline) != VK_SUCCESS)
    {
        cleanup();
        return std::nullopt; // failed to create graphics pipeline
    }
    else
    {
        cleanup();
        return new_pipeline;
    }
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
    multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading     = 1.0f,
    };

    // color_blend_attachment = {
    //     .blendEnable    = false,
    //     .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    // };

    vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT);
    set_depth_testing(false);
}



void GraphicsPipelineBuilder::set_topology(VkPrimitiveTopology topology)
{
    input_assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology,
    };
}

void GraphicsPipelineBuilder::set_rasterization(VkPolygonMode polygon_mode, VkCullModeFlagBits cull_mode)
{
    rasterizer = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = polygon_mode,
        .cullMode    = cull_mode,
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.f,
    };
}

void GraphicsPipelineBuilder::set_depth_testing(bool depth_testing)
{
    depth_stencil = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = depth_testing,
        .depthWriteEnable = depth_testing,
        .depthCompareOp   = depth_testing ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_ALWAYS,
    };
}

std::optional<VkPipeline> GraphicsPipelineBuilder::build(Core* core, RenderPass* renderpass, uint32_t subpass_index)
{
    assert(core && renderpass);

    // BENCHMARK_FUNCTION();

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    uint32_t att_count = renderpass->get_subpass(subpass_index).attachments.size();
    if (color_blend_attachment.size() == 0)
    {
        color_blend_attachment.resize(att_count,
            VkPipelineColorBlendAttachmentState{
                .blendEnable    = false,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            });
    }
    else
    {
        assert(att_count == color_blend_attachment.size());
    }

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = att_count,
        .pAttachments    = color_blend_attachment.data(),

    };

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = static_cast<uint32_t>(shader_stages.size()),
        .pStages             = shader_stages.data(),
        .pVertexInputState   = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state_info,
        .layout              = pipeline_layout,
        .renderPass          = renderpass->renderpass(),
        .subpass             = subpass_index,
        .basePipelineHandle  = nullptr,
    };

    auto cleanup = [&] {
        for (auto& module : shader_stages)
            vkDestroyShaderModule(core->device(), module.module, nullptr);
    };

    // it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(core->device(), core->pipeline_cache(), 1, &pipeline_info, nullptr, &newPipeline) != VK_SUCCESS)
    {
        cleanup();
        return std::nullopt; // failed to create graphics pipeline
    }
    else
    {
        cleanup();
        return newPipeline;
    }
}

void GraphicsPipelineBuilder::_set_vertex_input()
{
    vertex_input_info = m_vertex_input->get_info();
}

namespace imp
{

std::optional<VkShaderModule> load_shader_module(VkDevice device, const std::string& shader_name)
{
    const std::regex re("(\\w+\\/\\.\\.\\/)");

    std::string out = shader_name;
    while (std::regex_search(out, re))
    {
        out = std::regex_replace(out, re, "");
    }

    auto it = embeded_sprvs.find(out);

    if (it == embeded_sprvs.end())
    {
        fmt::print(stderr, "no shader at {} module\n", out);
        return std::nullopt;
    }

    auto [ptr, len] = it->second;

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext                    = nullptr;
    create_info.codeSize                 = len * sizeof(uint32_t); // code length in bytes
    create_info.pCode                    = ptr;

    VkShaderModule shader_module;

    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
    {
        return std::nullopt;
    }

    return shader_module;
}

} // namespace imp

} // namespace vke
