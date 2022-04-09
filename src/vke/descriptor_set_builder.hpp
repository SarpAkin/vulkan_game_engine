#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vke
{
class Buffer;
class Image;

class DescriptorSetLayoutBuilder
{
public:
    inline DescriptorSetLayoutBuilder& add_ubo(VkShaderStageFlags stage, uint32_t count = 1)
    {
        add_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage, count);
        return *this;
    }
    inline DescriptorSetLayoutBuilder& add_ssbo(VkShaderStageFlags stage, uint32_t count = 1)
    {
        add_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage, count);
        return *this;
    }
    inline DescriptorSetLayoutBuilder& add_image_sampler(VkShaderStageFlags stage, uint32_t count = 1)
    {
        add_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage, count);
        return *this;
    }
    inline DescriptorSetLayoutBuilder& add_input_attachment(VkShaderStageFlags stage, uint32_t count = 1)
    {
        add_binding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, stage, count);
        return *this;
    }

    VkDescriptorSetLayout build(VkDevice device);

private:
    void add_binding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t count);

    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

class DescriptorPool;

class DescriptorSetBuilder
{
public:
    inline DescriptorSetBuilder& add_ubo(const Buffer& buffer, VkShaderStageFlags stage) { return add_buffer(buffer, stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); }
    inline DescriptorSetBuilder& add_ssbo(const Buffer& buffer, VkShaderStageFlags stage) { return add_buffer(buffer, stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); }

    inline DescriptorSetBuilder& add_ubo(std::vector<const Buffer*> buffer, VkShaderStageFlags stage) { return add_buffer(buffer, stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); }
    inline DescriptorSetBuilder& add_ssbo(std::vector<const Buffer*> buffer, VkShaderStageFlags stage) { return add_buffer(buffer, stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); }

    inline DescriptorSetBuilder& add_image_sampler(const Image& images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage)
    {
        return add_image(images, layout, sampler, stage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    inline DescriptorSetBuilder& add_image_sampler(std::vector<const Image*> images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage)
    {
        return add_image(images, layout, sampler, stage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    VkDescriptorSet build(DescriptorPool& pool, VkDescriptorSetLayout layout);

private:
    ;
    inline DescriptorSetBuilder& add_buffer(const Buffer& buffer, VkShaderStageFlags stage, VkDescriptorType type)
    {
        return add_buffer({&buffer}, stage, type);
    }
    DescriptorSetBuilder& add_buffer(std::vector<const Buffer*> buffer, VkShaderStageFlags stage, VkDescriptorType type);

    inline DescriptorSetBuilder& add_image(const Image& images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage, VkDescriptorType type)
    {
        return add_image({&images}, layout, sampler, stage, type);
    }
    DescriptorSetBuilder& add_image(std::vector<const Image*> images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage, VkDescriptorType type);

    struct ImageBinding
    {
        std::vector<VkDescriptorImageInfo> image_infos;
        uint32_t binding;
        VkDescriptorType type;
    };

    std::vector<ImageBinding> m_image_bindings;

    struct BufferBinding
    {
        std::vector<VkDescriptorBufferInfo> buffer_infos;
        uint32_t binding;
        VkDescriptorType type;
    };

    std::vector<BufferBinding> m_buffer_bindings;

    uint32_t m_counter = 0;
};

} // namespace vke
