#include "descriptor_set_builder.hpp"

#include "core/core.hpp"

#include "descriptor_pool.hpp"
#include "util.hpp"
#include "vkutil.hpp"

namespace vke
{

void DescriptorSetLayoutBuilder::add_binding(VkDescriptorType type, VkShaderStageFlags stage, uint32_t count)
{
    m_bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding         = static_cast<uint32_t>(m_bindings.size()),
        .descriptorType  = type,
        .descriptorCount = count,
        .stageFlags      = stage,
    });
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::build(VkDevice device)
{
    VkDescriptorSetLayoutCreateInfo info{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(m_bindings.size()),
        .pBindings    = m_bindings.data(),
    };

    VkDescriptorSetLayout layout;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));

    return layout;
}

DescriptorSetBuilder& DescriptorSetBuilder::add_buffer(std::vector<const Buffer*> buffer, VkShaderStageFlags stage, VkDescriptorType type)
{
    return add_buffer(map_vec(buffer, [](const Buffer* b) { return std::make_tuple(b, 0U, static_cast<uint32_t>(b->size())); }), stage, type);
}

DescriptorSetBuilder& DescriptorSetBuilder::add_buffer(std::vector<std::tuple<const Buffer*, uint32_t, uint32_t>> buffer, VkShaderStageFlags stage, VkDescriptorType type)
{
    m_buffer_bindings.push_back(BufferBinding{
        .buffer_infos = map_vec(buffer, [&](std::tuple<const Buffer*, uint32_t, uint32_t>& tuple) {
            auto& [buffer, offset, size] = tuple;

            return VkDescriptorBufferInfo{
                .buffer = buffer->buffer(),
                .offset = offset,
                .range  = size,
            };
        }),
        .binding      = m_counter++,
        .type         = type,
    });

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::add_image(const std::vector<const Image*>& images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage, VkDescriptorType type)
{
    add_image(map_vec(images, [&](const Image* image) { return image->view; }), layout, sampler, stage, type);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::add_image(const std::vector<VkImageView>& images, VkImageLayout layout, VkSampler sampler, VkShaderStageFlags stage, VkDescriptorType type)
{
    m_image_bindings.push_back(ImageBinding{
        .image_infos = map_vec(images, [&](VkImageView image_view) {
            return VkDescriptorImageInfo{
                .sampler     = sampler,
                .imageView   = image_view,
                .imageLayout = layout,
            };
        }),
        .binding     = m_counter++,
        .type        = type,
    });

    return *this;
}

VkDescriptorSet DescriptorSetBuilder::build(DescriptorPool& pool, VkDescriptorSetLayout layout)
{
    VkDescriptorSet set = pool.allocate_set(layout);

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_buffer_bindings.size() + m_image_bindings.size());

    for (auto& buffer_binding : m_buffer_bindings)
        writes.push_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = buffer_binding.binding,
            .descriptorCount = static_cast<uint32_t>(buffer_binding.buffer_infos.size()),
            .descriptorType  = buffer_binding.type,
            .pBufferInfo     = buffer_binding.buffer_infos.data(),
        });

    for (auto& image_binding : m_image_bindings)
        writes.push_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = image_binding.binding,
            .descriptorCount = static_cast<uint32_t>(image_binding.image_infos.size()),
            .descriptorType  = image_binding.type,
            .pImageInfo      = image_binding.image_infos.data(),
        });

    vkUpdateDescriptorSets(pool.device(), writes.size(), writes.data(), 0, nullptr);

    return set;
}

} // namespace vke

// std::vector<VkWriteDescriptorSet> m_writes;
