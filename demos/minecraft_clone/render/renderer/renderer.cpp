#include "renderer.hpp"

#include "../../game/game.hpp"

#include "deferedlight_buffer.hpp"

VkRenderer::VkRenderer(Game* game)
{
    m_game  = game;
    m_world = game->world();

    m_core = std::make_unique<vke::Core>(1280, 720, "minecraft clone");

    m_main_pass = [&] {
        auto builder         = vke::RenderPassBuilder();
        uint32_t albedo_spec = builder.add_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkClearValue{.color{1.f, 1.f, 1.f, 0.f}});
        uint32_t normal      = builder.add_attachment(VK_FORMAT_R16G16B16A16_UNORM, VkClearValue{.color{0.5f, 0.5f, 0.5f, 0.f}});
        uint32_t depth       = builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil = {.depth = 1.f}});
        uint32_t swc         = builder.add_swapchain_attachment(m_core.get());
        builder.add_subpass({albedo_spec, normal}, depth);
        builder.add_subpass({swc}, std::nullopt, {albedo_spec, normal, depth});

        return builder.build(m_core.get(), m_core->width(), m_core->height());
    }();

    m_lifetime_pool = std::make_unique<vke::DescriptorPool>(m_core->device());

    for (auto& frame_data : m_frame_datas)
    {
        frame_data.pool = std::make_unique<vke::DescriptorPool>(m_core->device());
        frame_data.dubo = std::make_unique<vke::DynamicUBO>(m_core.get(), DYNAMIC_UBO_CAP);
    }

    m_deferedlightning.set_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_dubo(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_core->device());

    m_deferedlightning.pipeline_layout = vke::PipelineLayoutBuilder().add_set_layout(m_deferedlightning.set_layout).build(m_core->device());

    m_deferedlightning.pipeline = [&] {
        auto builder = vke::PipelineBuilder();
        builder.set_depth_testing(false);
        builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
        builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "../screen_quad.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "defered_lightning.frag").value());
        builder.pipeline_layout = m_deferedlightning.pipeline_layout;

        return builder.build(m_core.get(), m_main_pass.get(), 1).value();
    }();

    
}

VkRenderer::~VkRenderer()
{
    m_chunk_renderer->cleanup();
    m_textrenderer->cleanup();
    m_main_pass->clean();
    m_lifetime_pool->clean();

    auto device = m_core->device();

    vkDestroyPipelineLayout(device, m_deferedlightning.pipeline_layout,nullptr);
    vkDestroyPipeline(device, m_deferedlightning.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, m_deferedlightning.set_layout, nullptr);

    for (auto& frame_data : m_frame_datas)
    {
        frame_data.pool->clean();
        frame_data.dubo->cleanup();
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
    m_textrenderer->register_renderpass(m_main_pass.get(), 1);
}

void VkRenderer::frame(std::function<void(float)>& update, vke::Core::FrameArgs& args)
{

    auto cmd            = args.cmd;
    auto delta_t        = args.delta_t;
    auto& cleanup_queue = args.cleanup_queue;

    if (!initialized)
    {
        init(cmd, cleanup_queue);
        initialized = true;
    }

    update(delta_t);

    auto* current_f = &current_frame();

    cleanup_queue.push_back([&,current_f ] {
        current_f->pool->reset_pools();
        current_f->dubo->reset();
    });

    for (auto c : m_world->get_updated_chunks())
    {
        m_chunk_renderer->mesh_chunk(c);
    }

    m_main_pass->begin(cmd);

    glm::mat4 proj_view = m_game->camera()->proj(m_main_pass->size()) * m_game->player()->view();
    render_objects(cmd, m_main_pass.get(), proj_view);

    m_main_pass->next_subpass(cmd);

    defered_lightning(cmd, proj_view);

    m_textrenderer->render_text_px(m_main_pass.get(), "const std::\nString &s", glm::vec2(20.f, 25.f), glm::vec2(16.f, 16.f));
    m_textrenderer->render(cmd, m_main_pass.get());

    m_main_pass->end(cmd);
}

void VkRenderer::render_objects(VkCommandBuffer cmd, vke::RenderPass* render_pass, const glm::mat4& proj_view)
{
    m_chunk_renderer->render(cmd, current_frame().pool.get(), m_main_pass.get(), 0, proj_view);
}

void VkRenderer::defered_lightning(VkCommandBuffer cmd, const glm::mat4& proj_view)
{
    auto& dubo = current_frame().dubo;



    auto [offset,size] = dubo->push_data(glsl::SceneBuffer{
        .proj_view     = proj_view,
        .inv_proj_view = glm::inverse(proj_view),
        // .shadow_proj_view = glm::mat4(1),
    });

    auto set =
        vke::DescriptorSetBuilder()
            .add_input_attachment(m_main_pass->get_subpass(0).get_attachment(0)->view)
            .add_input_attachment(m_main_pass->get_subpass(0).get_attachment(1)->view)
            .add_input_attachment(m_main_pass->get_subpass(0).get_depth_attachment()->view)
            .add_dubo(dubo->buffer(),offset,size, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(*current_frame().pool, m_deferedlightning.set_layout);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.pipeline_layout, 0, 1, &set, 1, &offset);

    vkCmdDraw(cmd, 6, 1, 0, 0);
}

// irenderer.hpp
std::unique_ptr<IRenderer> IRenderer::crate_vulkan_renderer(Game* game)
{
    return std::make_unique<VkRenderer>(game);
}