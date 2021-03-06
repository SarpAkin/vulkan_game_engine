#pragma once

#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

namespace vke
{

class DescriptorPool
{
public:
    DescriptorPool(VkDevice device, uint32_t max_sets = 100,
        const std::unordered_map<VkDescriptorType, float>& pool_sizes = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
            // {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
            // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
            // {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
            // {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
            // {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
            // {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
            // {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f},
        }) : m_device(device), m_max_sets(max_sets), m_pool_sizes(pool_sizes)
    {
    }

    VkDescriptorSet allocate_set(VkDescriptorSetLayout layout);
    void reset_pools();
    void clean();

    auto device() { return m_device; }

public:
    std::unordered_map<VkDescriptorType, float> m_pool_sizes;
    uint32_t m_max_sets = 100;

private:
    std::vector<VkDescriptorPool> m_free_pools, m_used_pools;
    VkDescriptorPool m_current_pool = nullptr;
    VkDevice m_device               = nullptr;

    void next_pool();
};

} // namespace vke