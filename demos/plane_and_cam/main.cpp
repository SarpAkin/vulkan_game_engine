#include <fmt/format.h>

#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vke/core/core.hpp>
#include <vke/descriptor_pool.hpp>
#include <vke/descriptor_set_builder.hpp>
#include <vke/pipeline_builder.hpp>
#include <vke/renderpass.hpp>
#include <vke/util.hpp>

struct Vertex
{
    glm::vec3 pos;

    REFLECT_VERTEX_INPUT_DESCRIPTION(Vertex, pos);
};

struct Mesh
{
    std::unique_ptr<vke::Buffer> vb;
    uint32_t vertex_count;
};

Mesh create_plane_mesh(vke::Core& core, glm::ivec2 size)
{
    uint32_t vertex_count = (size.x * size.y * 6);

    auto buffer       = core.allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * (vertex_count + 1), true);
    Vertex* verticies = buffer->get_data<Vertex>();

    for (int y = 0; y < size.y; ++y)
    {
        for (int x = 0; x < size.x; ++x)
        {
            uint32_t quad_index = (y * size.x + x) * 6;

            glm::vec3 quad_pos = glm::vec3(x, 0.f, y);

            verticies[quad_index + 0].pos = quad_pos + glm::vec3(0.f, 0.f, 0.f);
            verticies[quad_index + 2].pos = quad_pos + glm::vec3(0.f, 0.f, 1.f);
            verticies[quad_index + 1].pos = quad_pos + glm::vec3(1.f, 0.f, 0.f);
            verticies[quad_index + 4].pos = quad_pos + glm::vec3(1.f, 0.f, 1.f);
            verticies[quad_index + 3].pos = quad_pos + glm::vec3(1.f, 0.f, 0.f);
            verticies[quad_index + 5].pos = quad_pos + glm::vec3(0.f, 0.f, 1.f);
        }
    }

    return Mesh{
        .vb           = std::move(buffer),
        .vertex_count = vertex_count};
}

int main()
{


    uint32_t width = 800, height = 500;

    auto core = vke::Core(width, height, "app");

    auto renderpass = [&] {
        auto rp_builder = vke::RenderPassBuilder();
        uint32_t sw_att = rp_builder.add_swapchain_attachment(&core, VkClearValue{.color = {.25f, .3f, .9f, 0.f}});
        uint32_t dp_att = rp_builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil{1.f}});
        rp_builder.add_subpass({sw_att}, dp_att);
        return rp_builder.build(&core, width, height);
    }();

    struct PushConstant
    {
        glm::mat4 mvp;
    };

    struct UBO
    {
        glm::mat4 cam;
    };

    auto ubo = core.allocate_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO), true);

    auto d_pool = vke::DescriptorPool(core.device());

    auto set_layout = vke::DescriptorSetLayoutBuilder().add_ubo(VK_SHADER_STAGE_VERTEX_BIT).build(core.device());
    auto d_set      = vke::DescriptorSetBuilder().add_ubo(*ubo, VK_SHADER_STAGE_VERTEX_BIT).build(d_pool, set_layout);

    auto p_layout = vke::PipelineLayoutBuilder().add_push_constant<PushConstant>(VK_SHADER_STAGE_VERTEX_BIT).add_set_layout(set_layout).build(core.device());

    auto pipeline_fn = [&] {
        BENCHMARK_FUNCTION();

        vke::PipelineBuilder builder;

        builder.set_vertex_input<Vertex>();
        builder.pipeline_layout = p_layout;
        builder.set_depth_testing(true);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(core.device(), "a.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(core.device(), "a.frag").value());

        return builder.build(&core, renderpass.get(), 0).value();
    };

    vkDestroyPipeline(core.device(), pipeline_fn(), nullptr);

    auto pipeline = pipeline_fn();

    auto buffer       = core.allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * 4, true);
    Vertex* verticies = buffer->get_data<Vertex>();

    verticies[0].pos = glm::vec3(-0.5, -0.5, 0.1);
    verticies[1].pos = glm::vec3(0, 0.5, 0.1);
    verticies[2].pos = glm::vec3(0.5, -0.5, 0.1);

    float total_time = 0.f;

    float cam_pitch = 0.f, cam_yaw = 0.f;
    float mouse_speed = 0.05f;

    glm::vec3 pos  = {0.f, 7.f, 2.f};
    glm::mat4 proj = glm::perspective(glm::radians(80.f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 500.f);
    // proj[1][1] *= -1.f;

    Mesh plane_mesh = create_plane_mesh(core, {10, 10});

    core.run([&](vke::Core::FrameArgs args) {
        // fmt::print("{}\n", args.delta_t);
        auto cmd = args.cmd;

        total_time += args.delta_t;

        cam_pitch += core.mouse_delta().y * mouse_speed;
        cam_yaw -= core.mouse_delta().x * mouse_speed;

        cam_pitch = std::clamp(cam_pitch, glm::radians(-89.f), glm::radians(89.f));

        glm::vec3 dir_vector = {
            cos(cam_yaw) * cos(cam_pitch), // x
            sin(cam_pitch),                // y
            sin(cam_yaw) * cos(cam_pitch), // z
        };

        glm::vec3 move_vector = {};

        if (core.is_key_pressed('a')) move_vector.z += 1.f;
        if (core.is_key_pressed('d')) move_vector.z -= 1.f;
        if (core.is_key_pressed('w')) move_vector.x += 1.f;
        if (core.is_key_pressed('s')) move_vector.x -= 1.f;
        if (core.is_key_pressed(' ')) move_vector.y += 1.f;
        if (core.is_key_pressed('c')) move_vector.y -= 1.f;

        move_vector = glm::vec3(dir_vector.x, 0.f, dir_vector.z) * move_vector.x +  //
                      glm::vec3(dir_vector.z, 0.f, -dir_vector.x) * move_vector.z + //
                      glm::vec3(0.f, move_vector.y, 0.f);

        // fmt::print("{: .2f} {: .2f} {: .2f}\n", move_vector.x, move_vector.y, move_vector.z);

        pos += move_vector * args.delta_t;

        glm::mat4 view = glm::lookAt(pos, pos + dir_vector, glm::vec3(0.f, 1.f, 0.f));

        // renderpass->set_attachment_clear_value(0, VkClearValue{.color{std::sin(total_time) * 0.5f + 0.5f,0.f,0.f,0.f}});

        renderpass->begin(cmd);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        PushConstant push = {proj * view * glm::translate(glm::vec3(-5.f, 0.f, -5.f))};

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p_layout, 0, 1, &d_set, 0, nullptr);

        vkCmdPushConstants(cmd, p_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push);
        size_t offsets = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &plane_mesh.vb->buffer(), &offsets);

        vkCmdDraw(cmd, plane_mesh.vertex_count, 1, 0, 0);

        renderpass->end(cmd);
    });

    ubo->clean_up();
    buffer->clean_up();

    vkDestroyDescriptorSetLayout(core.device(), set_layout, nullptr);
    
    d_pool.clean();

    plane_mesh.vb->clean_up();

    vkDestroyPipelineLayout(core.device(), p_layout, nullptr);
    vkDestroyPipeline(core.device(), pipeline, nullptr);

    renderpass->clean();



    return 0;
}