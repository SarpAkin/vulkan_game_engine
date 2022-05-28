#include <fmt/format.h>

#include <iostream>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vke/core/core.hpp>
#include <vke/descriptor_pool.hpp>
#include <vke/descriptor_set_builder.hpp>
#include <vke/pipeline_builder.hpp>
#include <vke/renderpass.hpp>
#include <vke/util.hpp>

#include "render/chunk/chunk_renderer.hpp"
#include "render/texts/text_renderer.hpp"

#include "game/chunk.hpp"
#include "game/player.hpp"
#include "game/world.hpp"

class App
{
public:
    App()
    {
        m_core = std::make_unique<vke::Core>(1280, 720, "minecraft clone");

        auto gpass = [&] {
            auto builder         = vke::RenderPassBuilder();
            uint32_t albedo_spec = builder.add_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkClearValue{.color{1.f, 1.f, 1.f, 0.f}});
            uint32_t normal      = builder.add_attachment(VK_FORMAT_R16G16B16A16_UNORM, VkClearValue{.color{0.5f, 0.5f, 0.5f, 0.f}});
            uint32_t depth       = builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil = {.depth = 1.f}});
            uint32_t swc         = builder.add_swapchain_attachment(m_core.get(), VkClearValue{.color = {1.f, 1.f, 1.f}});
            builder.add_subpass({albedo_spec,normal}, depth);
            builder.add_subpass({swc}, std::nullopt, {albedo_spec, normal,depth});

            return builder.build(m_core.get(), m_core->width(), m_core->height());
        }();
        gpass->clean();

        m_main_pass = [&] {
            auto builder = vke::RenderPassBuilder();

            uint32_t sw_att = builder.add_swapchain_attachment(m_core.get(), VkClearValue{.color = {1.f, 1.f, 1.f}});
            uint32_t ds_att = builder.add_attachment(VK_FORMAT_D16_UNORM, VkClearValue{.depthStencil = {.depth = 1.f, .stencil = 0}});
            builder.add_subpass({sw_att}, ds_att);

            return builder.build(m_core.get(), m_core->width(), m_core->height());
        }();

        

        m_lifetime_pool = std::make_unique<vke::DescriptorPool>(m_core->device());

        for (auto& frame_data : m_frame_datas)
        {
            frame_data.m_frame_pool = std::make_unique<vke::DescriptorPool>(m_core->device());
        }

        m_world = std::make_unique<World>();
    }

    ~App()
    {
        m_chunk_renderer->cleanup();
        m_textrenderer->cleanup();
        m_main_pass->clean();
        m_lifetime_pool->clean();

        m_text->clean_up();

        for (auto& frame_data : m_frame_datas)
        {
            frame_data.m_frame_pool->clean();
        }
    }

    void run()
    {
        m_core->run([&](auto&&... args) { return frame(args...); });
    }

private:
    void frame(vke::Core::FrameArgs& args)
    {

        auto& current_frame = m_frame_datas[m_core->frame_index()];
        auto cmd            = args.cmd;
        auto delta_t        = args.delta_t;
        auto& cleanup_queue = args.cleanup_queue;
        cleanup_queue.push_back([&] {
            current_frame.m_frame_pool->reset_pools();
        });

        if (m_chunk_renderer == nullptr)
        {
            m_chunk_renderer = std::make_unique<ChunkRenderer>(m_core.get(), *m_lifetime_pool, cmd, cleanup_queue);
            m_chunk_renderer->register_renderpass(m_main_pass.get(), 0, false);

            for (int i = 0; i < 2; ++i)
            {
                auto c = std::make_unique<Chunk>();
                for (int y = 0; y < 32; ++y)
                {
                    for (int x = 0; x < 32; ++x)
                    {
                        c->set_block(Tile::grass, x, 4, y);
                    }
                }
                m_world->set_chunk(std::move(c), glm::ivec2(i, 0));
            }
        }

        if (m_textrenderer == nullptr)
        {
            m_textrenderer = std::make_unique<TextRenderer>(m_core.get(), m_lifetime_pool.get(), cmd, cleanup_queue);
            m_textrenderer->register_renderpass(m_main_pass.get(), 0);
            m_text = m_textrenderer->mesh_string("const std::string &stringga");
        }

        for (auto c : m_world->get_updated_chunks())
        {
            m_chunk_renderer->mesh_chunk(c);
        }

        m_player.update(*m_core, delta_t);

        glm::mat4 proj_view = m_camera.proj(*m_core) * m_player.view();

        m_main_pass->begin(cmd);

        m_chunk_renderer->render(cmd, m_main_pass.get(), 0, proj_view);

        m_textrenderer->render_text_px(m_main_pass.get(), "const std::\nString &s", glm::vec2(20.f, 25.f), glm::vec2(16.f, 16.f));

        m_textrenderer->render(cmd, m_main_pass.get());

        m_main_pass->end(cmd);
    }

private:
    std::unique_ptr<vke::Core> m_core;
    std::unique_ptr<vke::RenderPass> m_main_pass;
    std::unique_ptr<vke::DescriptorPool> m_lifetime_pool;
    std::unique_ptr<World> m_world;
    std::unique_ptr<TextRenderer> m_textrenderer;
    std::unique_ptr<vke::Buffer> m_text;

    std::unique_ptr<ChunkRenderer> m_chunk_renderer;

    struct FrameData
    {
        std::unique_ptr<vke::DescriptorPool> m_frame_pool;
    };

    std::array<FrameData, vke::Core::FRAME_OVERLAP> m_frame_datas;

    Player m_player;
    Camera m_camera;
};

int main()
{
    App app;
    app.run();
}