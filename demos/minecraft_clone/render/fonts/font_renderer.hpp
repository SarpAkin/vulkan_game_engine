#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <vke/core/core.hpp>

#include "../render_system.hpp"

namespace vke
{
class DescriptorPool;
}

class FontRenderer : public IRenderSystem
{
public:
    FontRenderer(vke::Core* core,vke::DescriptorPool* lifetime_pool,VkCommandBuffer cmd,std::vector<std::function<void()>>& cleanup_queue);

    void register_renderpass(vke::RenderPass* renderpass,int subpass);

    std::unique_ptr<vke::Buffer> mesh_string(const std::string& string);
    void render_text(VkCommandBuffer cmd,vke::RenderPass* render_pass,const vke::Buffer* mesh,glm::vec2 pos,glm::vec2 scale);

    void cleanup() override;

private:
    vke::Core* m_core;

    struct Pipelines
    {
        VkPipeline font_renderer2D;
    };

    VkSampler m_linear_sampler;
    VkDescriptorSetLayout m_texture_set_layout;
    VkDescriptorSet m_texture_set;
    VkPipelineLayout m_pipelinelayout;
    std::unordered_map<const vke::RenderPass*, Pipelines> m_pipelines;

    std::unique_ptr<vke::Image> m_font_texture;


};
