#include <fmt/format.h>

#include <iostream>

#include <glm/vec3.hpp>

#include "../vke/core/core.hpp"
#include "../vke/pipeline_builder.hpp"
#include "../vke/renderpass.hpp"

int main()
{

    uint32_t width = 800, height = 500;

    auto core = vke::Core(width, height, "app");

    auto renderpass = [&] {
        auto rp_builder = vke::RenderPassBuilder();
        uint32_t sw_att = rp_builder.add_swapchain_attachment(core, VkClearValue{.color = {0.f, 0.f, 0.f, 0.f}});
        uint32_t dp_att = rp_builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil{1.f}});
        rp_builder.add_subpass({sw_att}, dp_att);
        return rp_builder.build(core, width, height);
    }();

    auto playout = vke::PipelineLayoutBuilder().build(core.device());

    struct Vertex
    {
        glm::vec3 pos;

        REFLECT_VERTEX_INPUT_DESCRIPTION(Vertex, pos);
    };

    auto pipeline = [&] {
        vke::PipelineBuilder builder;
        builder.set_vertex_input<Vertex>();
        builder.pipeline_layout = playout;
        builder.set_depth_testing(false);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(core.device(), "a.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(core.device(), "a.frag").value());

        return builder.build(core, *renderpass, 0).value();
    }();

    auto buffer       = core.allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * 4, true);
    Vertex* verticies = buffer.get_data<Vertex>();
    verticies[0].pos  = glm::vec3(-0.5, -0.5, 0.1);
    verticies[1].pos  = glm::vec3(0.5, -0.5, 0.1);
    verticies[2].pos  = glm::vec3(0, 0.5, 0.1);

    float total_time = 0.f;

    core.run([&](vke::Core::FrameArgs args) {
        // fmt::print("{}\n", args.delta_t);
        auto cmd = args.cmd;

        total_time += args.delta_t;

        renderpass->set_attachment_clear_value(0, VkClearValue{.color{std::sin(total_time) * 0.5f + 0.5f,0.f,0.f,0.f}});

        renderpass->begin(cmd);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        size_t offsets = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.buffer, &offsets);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        renderpass->end(cmd);
    });

    buffer.clean_up();

    vkDestroyPipelineLayout(core.device(), playout, nullptr);
    vkDestroyPipeline(core.device(), pipeline, nullptr);

    renderpass->clean();

    return 0;
}