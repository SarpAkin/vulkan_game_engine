#include "renderer.hpp"

#include "../../game/game.hpp"

VkRenderer::VkRenderer(Game* game)
{
    m_game = game;
    m_world = game->world();

    m_core = std::make_unique<vke::Core>(1280, 720, "minecraft clone");

    auto gpass = [&] {
        auto builder         = vke::RenderPassBuilder();
        uint32_t albedo_spec = builder.add_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkClearValue{.color{1.f, 1.f, 1.f, 0.f}});
        uint32_t normal      = builder.add_attachment(VK_FORMAT_R16G16B16A16_UNORM, VkClearValue{.color{0.5f, 0.5f, 0.5f, 0.f}});
        uint32_t depth       = builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil = {.depth = 1.f}});
        uint32_t swc         = builder.add_swapchain_attachment(m_core.get());
        builder.add_subpass({albedo_spec, normal}, depth);
        builder.add_subpass({swc}, std::nullopt, {albedo_spec, normal, depth});

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
}

VkRenderer ::~VkRenderer()
{
    m_chunk_renderer->cleanup();
    m_textrenderer->cleanup();
    m_main_pass->clean();
    m_lifetime_pool->clean();

    for (auto& frame_data : m_frame_datas)
    {
        frame_data.m_frame_pool->clean();
    }
}

void VkRenderer::run(std::function<void(float)>&& update)
{
    m_core->run([&](auto&&... args) { return frame(update, args...); });
}

void VkRenderer::init(VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue)
{
    m_chunk_renderer = std::make_unique<ChunkRenderer>(m_core.get(), *m_lifetime_pool, cmd, cleanup_queue);
    m_chunk_renderer->register_renderpass(m_main_pass.get(), 0, false);

    m_textrenderer = std::make_unique<TextRenderer>(m_core.get(), m_lifetime_pool.get(), cmd, cleanup_queue);
    m_textrenderer->register_renderpass(m_main_pass.get(), 0);
}

void VkRenderer::frame(std::function<void(float)>& update, vke::Core::FrameArgs& args)
{
    auto& current_frame = m_frame_datas[m_core->frame_index()];
    auto cmd            = args.cmd;
    auto delta_t        = args.delta_t;
    auto& cleanup_queue = args.cleanup_queue;

    if (!initialized)
    {
        init(cmd, cleanup_queue);
        initialized = true;
    }

    update(delta_t);

    cleanup_queue.push_back([&] {
        current_frame.m_frame_pool->reset_pools();
    });

    for (auto c : m_world->get_updated_chunks())
    {
        m_chunk_renderer->mesh_chunk(c);
    }

    glm::mat4 proj_view = m_game->camera()->proj(m_main_pass->size()) * m_game->player()->view();

    m_main_pass->begin(cmd);

    m_chunk_renderer->render(cmd, m_main_pass.get(), 0, proj_view);

    m_textrenderer->render_text_px(m_main_pass.get(), "const std::\nString &s", glm::vec2(20.f, 25.f), glm::vec2(16.f, 16.f));

    m_textrenderer->render(cmd, m_main_pass.get());

    m_main_pass->end(cmd);
}

//irenderer.hpp
std::unique_ptr<IRenderer> IRenderer::crate_vulkan_renderer(Game* game)
{
    return std::make_unique<VkRenderer>(game);
}