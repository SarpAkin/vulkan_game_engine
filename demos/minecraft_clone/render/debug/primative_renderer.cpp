#include "primative_renderer.hpp"

#include <string.h>

#include <vke/core/core.hpp>
#include <vke/pipeline_builder.hpp>
#include <vke/renderpass.hpp>

#include "../glsl_shared.hpp"

struct PrimativeRenderer::LineVert
{
    glm::vec3 pos;
    uint32_t color;

    REFLECT_VERTEX_INPUT_DESCRIPTION(PrimativeRenderer::LineVert, pos, color);
};

struct Push
{
    glm::mat4x4 proj_view;
};

void PrimativeRenderer::render(VkCommandBuffer cmd, vke::RenderPass* rp, int subpass, const glm::mat4& proj_view)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rp_data[rp].line_pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_line_buffer[m_core->frame_index()]->buffer(), &offset);

    Push push = {
        .proj_view = proj_view,
    };

    vkCmdPushConstants(cmd, m_line_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

    vkCmdDraw(cmd, m_vert_counter, 1, 0, 0);

    m_vert_counter = 0;
}

void PrimativeRenderer::cleanup()
{
    auto device = m_core->device();

    vkDestroyPipelineLayout(device, m_line_pipeline_layout, nullptr);
    for (auto& [rp, data] : m_rp_data)
        vkDestroyPipeline(device, data.line_pipeline, nullptr);

    for (auto& b : m_line_buffer)
        b->clean_up();
}

PrimativeRenderer::PrimativeRenderer(vke::Core* core)
{
    m_core = core;

    m_line_pipeline_layout = vke::PipelineLayoutBuilder().add_push_constant<Push>(VK_SHADER_STAGE_VERTEX_BIT).build(m_core->device());
    for (auto& b : m_line_buffer)
        b = m_core->allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(LineVert) * MAX_VERTS, true);
}

PrimativeRenderer ::~PrimativeRenderer()
{
}

void PrimativeRenderer::register_renderpass(vke::RenderPass* rp, int subpass)
{
    m_rp_data[rp] = RPData{
        .line_pipeline = [&] {
            auto builder = vke::PipelineBuilder();
            builder.set_vertex_input<LineVert>();
            builder.pipeline_layout = m_line_pipeline_layout;
            builder.set_depth_testing(true);
            builder.set_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
            builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
            builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, /**/ LOAD_LOCAL_SHADER_MODULE(m_core->device(), "line.frag").value());
            builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, /****/ LOAD_LOCAL_SHADER_MODULE(m_core->device(), "line.vert").value());
            return builder.build(m_core, rp, subpass).value();
        }(),
    };
}

void PrimativeRenderer::line(glm::vec3 a, glm::vec3 b, uint32_t rgba_color)
{
    m_line_buffer[m_core->frame_index()]->get_data<LineVert>()[m_vert_counter++] = {a, rgba_color};
    m_line_buffer[m_core->frame_index()]->get_data<LineVert>()[m_vert_counter++] = {b, rgba_color};
}

void PrimativeRenderer::box_frame(const glsl::AABB& aabb, uint32_t rgba_color) { box_frame(aabb.min, aabb.max, rgba_color); }

void PrimativeRenderer::box_frame(glm::vec3 a, glm::vec3 b, uint32_t rgba_color)
{
    line({a.x, a.y, a.z}, {a.x, b.y, a.z}, rgba_color);
    line({a.x, a.y, b.z}, {a.x, b.y, b.z}, rgba_color);
    line({b.x, a.y, a.z}, {b.x, b.y, a.z}, rgba_color);
    line({b.x, a.y, b.z}, {b.x, b.y, b.z}, rgba_color);

    line({a.x, a.y, a.z}, {a.x, a.y, b.z}, rgba_color);
    line({a.x, b.y, a.z}, {a.x, b.y, b.z}, rgba_color);
    line({b.x, a.y, a.z}, {b.x, a.y, b.z}, rgba_color);
    line({b.x, b.y, a.z}, {b.x, b.y, b.z}, rgba_color);

    line({a.x, a.y, a.z}, {b.x, a.y, a.z}, rgba_color);
    line({a.x, b.y, a.z}, {b.x, b.y, a.z}, rgba_color);
    line({a.x, a.y, b.z}, {b.x, a.y, b.z}, rgba_color);
    line({a.x, b.y, b.z}, {b.x, b.y, b.z}, rgba_color);
}

void PrimativeRenderer::frustrum_frame(const glm::mat4x4& proj_view, uint32_t rgba_color)
{
    auto b = glsl::frustrum_boundry_from_projection(glm::inverse(proj_view));

    line(b.boundries[0][0][0], b.boundries[0][1][0], rgba_color);
    line(b.boundries[0][0][1], b.boundries[0][1][1], rgba_color);
    line(b.boundries[1][0][0], b.boundries[1][1][0], rgba_color);
    line(b.boundries[1][0][1], b.boundries[1][1][1], rgba_color);

    line(b.boundries[0][0][0], b.boundries[0][0][1], rgba_color);
    line(b.boundries[0][1][0], b.boundries[0][1][1], rgba_color);
    line(b.boundries[1][0][0], b.boundries[1][0][1], rgba_color);
    line(b.boundries[1][1][0], b.boundries[1][1][1], rgba_color);

    line(b.boundries[0][0][0], b.boundries[1][0][0], rgba_color);
    line(b.boundries[0][1][0], b.boundries[1][1][0], rgba_color);
    line(b.boundries[0][0][1], b.boundries[1][0][1], rgba_color);
    line(b.boundries[0][1][1], b.boundries[1][1][1], rgba_color);
}