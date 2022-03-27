#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <glm/vector_relational.hpp>

#include "foreachmacro.hpp"

namespace vke
{

class Core;
class RenderPass;

class PipelineLayoutBuilder
{
public:
    template <typename T>
    PipelineLayoutBuilder& add_push_constant(VkShaderStageFlags stage)
    {
        m_push_constants.push_back(VkPushConstantRange{
            .stageFlags = stage,
            .offset     = 0,
            .size       = sizeof(T),
        });

        return *this;
    }

    inline PipelineLayoutBuilder& add_set_layout(VkDescriptorSetLayout layout)
    {
        m_set_layouts.push_back(layout);

        return *this;
    }

    VkPipelineLayout build(VkDevice device);

private:
    std::vector<VkPushConstantRange> m_push_constants;
    std::vector<VkDescriptorSetLayout> m_set_layouts;
};

namespace imp
{
class VertexInputDescription;
}

class PipelineBuilder
{
public:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    VkPipelineLayout pipeline_layout                           = nullptr;
    VkPipelineDepthStencilStateCreateInfo depth_stencil        = {}; //
    VkPipelineVertexInputStateCreateInfo vertex_input_info     = {}; //
    VkPipelineInputAssemblyStateCreateInfo input_assembly      = {}; //
    VkPipelineRasterizationStateCreateInfo rasterizer          = {}; //
    VkPipelineMultisampleStateCreateInfo multisampling         = {}; ///
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};

    PipelineBuilder();

    void add_shader_stage(VkShaderStageFlagBits stage, VkShaderModule shader_module);
    void set_topology(VkPrimitiveTopology topology);
    void set_rasterization(VkPolygonMode polygon_mode, VkCullModeFlagBits cull_mode);
    void set_depth_testing(bool depth_testing);

    template <typename T>
    void set_vertex_input()
    {
        m_vertex_input = std::make_unique<imp::VertexInputDescription>(T::get_vertex_input_description());
        _set_vertex_input();
    }

    std::optional<VkPipeline> build(Core& core, RenderPass& renderpass, uint32_t subpass_index);

private:
    std::unique_ptr<imp::VertexInputDescription> m_vertex_input;

    void _set_vertex_input();
};

namespace imp
{

template <typename T>
struct Type
{
};

struct VertexInputDescription
{

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;

    VkPipelineVertexInputStateCreateInfo get_info() const
    {
        VkPipelineVertexInputStateCreateInfo info = {};
        info.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.pVertexBindingDescriptions           = bindings.data();
        info.vertexBindingDescriptionCount        = bindings.size();
        info.pVertexAttributeDescriptions         = attributes.data();
        info.vertexAttributeDescriptionCount      = attributes.size();

        return info;
    }

    template <typename T>
    void push_binding(VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX)
    {
        VkVertexInputBindingDescription binding = {};
        binding.binding                         = bindings.size();
        binding.stride                          = sizeof(T);
        binding.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

        bindings.push_back(binding);
    }

    template <typename T>
    void push_attribute(size_t offset)
    {
        assert(bindings.size() != 0 && "a binding must be pushed first");

        _push_attribute(Type<T>(), offset);
    }

private:
    template <int N>
    void _push_attribute(Type<glm::vec<N, float>>, size_t offset)
    {
        static_assert(N < 5);
        constexpr VkFormat formats[] = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

        VkVertexInputAttributeDescription attribute = {};
        attribute.binding                           = bindings.back().binding;
        attribute.location                          = attributes.size();
        attribute.format                            = formats[N];
        attribute.offset                            = offset;

        attributes.push_back(attribute);
    }

    template <int N>
    void _push_attribute(Type<glm::vec<N, uint32_t>>, size_t offset)
    {
        static_assert(N < 5);
        constexpr VkFormat formats[] = {VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT};

        VkVertexInputAttributeDescription attribute = {};
        attribute.binding                           = bindings.back().binding;
        attribute.location                          = attributes.size();
        attribute.format                            = formats[N];
        attribute.offset                            = offset;

        attributes.push_back(attribute);
    }

    void _push_attribute(Type<float>, size_t offset)
    {
        VkVertexInputAttributeDescription attribute = {};
        attribute.binding                           = bindings.back().binding;
        attribute.location                          = attributes.size();
        attribute.format                            = VK_FORMAT_R32_SFLOAT;
        attribute.offset                            = offset;

        attributes.push_back(attribute);
    }

    void _push_attribute(Type<uint32_t>, size_t offset)
    {
        VkVertexInputAttributeDescription attribute = {};
        attribute.binding                           = bindings.back().binding;
        attribute.location                          = attributes.size();
        attribute.format                            = VK_FORMAT_R32_UINT;
        attribute.offset                            = offset;

        attributes.push_back(attribute);
    }
};

#define REFLECT_VERTEX_INPUT_DESCRIPTION(class_type, fields...)            \
    static vke::imp::VertexInputDescription get_vertex_input_description() \
    {                                                                      \
        vke::imp::VertexInputDescription desc;                             \
        typedef class_type THIS;                                           \
        desc.push_binding<class_type>();                                   \
        FOREACH(REFLECT_VERTEX_INPUT_DESCRIPTION_FIELD_FUNC, fields);      \
        return desc;                                                       \
    }

#define REFLECT_VERTEX_INPUT_DESCRIPTION_FIELD_FUNC(field) desc.push_attribute<decltype(field)>(offsetof(THIS, field));

std::optional<VkShaderModule> load_shader_module(VkDevice device, const std::string& shader_filename);

#define LOAD_LOCAL_SHADER_MODULE(device, shader_filename) [&] {                                                                     \
    std::string file = __FILE__;                                                                                                    \
    return vke::imp::load_shader_module(device, std::string(file.begin(), file.begin() + (file.rfind("/") + 1)) + shader_filename); \
}()

} // namespace imp

} // namespace vke