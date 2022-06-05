#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vke/fwd.hpp>

#include <vke/core/core.hpp>

#include "../irender_system.hpp"

namespace glsl
{
struct AABB;
}

class PrimativeRenderer : public IRenderSystem
{
public:
    PrimativeRenderer(vke::Core* core);
    ~PrimativeRenderer();

    void render(VkCommandBuffer cmd, vke::RenderPass* rp, int subpass, const glm::mat4&);

    void register_renderpass(vke::RenderPass* rp, int subpass);

    void cleanup() override;

    const char* name() const
    {
        return "PrimativeRenderer";
    }

    static constexpr uint32_t default_color = 0x22'33'FF'FF;

    // primatives
    void line(glm::vec3 a, glm::vec3 b, uint32_t rgba_color = default_color);
    void box_frame(glm::vec3 start, glm::vec3 end, uint32_t rgba_color = default_color);
    void box_frame(const glsl::AABB& aabb, uint32_t rgba_color = default_color);
    void frustrum_frame(const glm::mat4x4& proj_view, uint32_t rgba_color = default_color);

private:
    vke::Core* m_core;

    struct LineVert;

    const uint32_t line_vert_cap = 1024 * 4;


    std::array<std::unique_ptr<vke::Buffer>, vke::Core::FRAME_OVERLAP> m_line_buffer;

    struct RPData
    {
        VkPipeline line_pipeline;
    };
    std::unordered_map<vke::RenderPass*, RPData> m_rp_data;

    VkPipelineLayout m_line_pipeline_layout;

    uint32_t m_vert_counter = 0;

    static constexpr uint32_t MAX_VERTS = 32 * 1024;
};