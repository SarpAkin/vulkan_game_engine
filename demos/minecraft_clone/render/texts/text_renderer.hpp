#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vke/core/core.hpp>

#include "../render_system.hpp"

namespace vke
{
class DescriptorPool;
}

class TextRenderer : public IRenderSystem
{
public:
    TextRenderer(vke::Core* core, vke::DescriptorPool* lifetime_pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue);

    void register_renderpass(vke::RenderPass* renderpass, int subpass);

    std::unique_ptr<vke::Buffer> mesh_string(const std::string& string);
    void render_textbuf(VkCommandBuffer cmd, vke::RenderPass* render_pass, const vke::Buffer* mesh, glm::vec2 pos, glm::vec2 scale);
    void render_text(vke::RenderPass* render_pass, const std::string& s, glm::vec2 pos, glm::vec2 scale);

    void render(VkCommandBuffer cmd,vke::RenderPass* render_pass);

    inline void render_textbuf_px(VkCommandBuffer cmd, vke::RenderPass* render_pass, const vke::Buffer* mesh, glm::vec2 pos, glm::vec2 scale)
    {
        auto rp_size = render_pass->size() * 0.5f;
        render_textbuf(cmd, render_pass, mesh, (pos / rp_size) - 1.f, (scale / 8.f) / rp_size);
    }

    inline void render_text_px(vke::RenderPass* render_pass, const std::string& s, glm::vec2 pos, glm::vec2 scale)
    {
        auto rp_size = render_pass->size() * 0.5f;
        render_text(render_pass, s, (pos / rp_size) - 1.f, (scale / 8.f) / rp_size);
    }

    void cleanup() override;

private:
    struct FontVert;

    static void mesh_string_into_buffer(FontVert*& vert_buf_it,FontVert* vert_buf_end,const std::string& string);

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
    std::array<std::unique_ptr<vke::Buffer>, vke::Core::FRAME_OVERLAP> m_internal_fontbufs;

    struct TextArgs
    {
        vke::RenderPass* render_pass;
        uint32_t vert_count;
        uint32_t vert_offset;
        glm::vec2 pos;
        glm::vec2 scale;
        glm::vec4 color;
    };
    uint32_t m_vert_counter = 0;
    std::vector<TextArgs> m_text_args;
    static constexpr size_t MAX_CHARS = 8192;
};
