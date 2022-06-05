#include "renderer.hpp"


#include "../../game/game.hpp"

#include "deferedlight_buffer.hpp"

VkRenderer::VkRenderer(Game* game)
{
    m_game  = game;
    m_world = game->world();

    m_core = std::make_unique<vke::Core>(1280, 720, "minecraft clone");

    input()->set_key_callback(
        'h', [this] { m_deferedlightning.render_mode = (m_deferedlightning.render_mode + 1) % 3; }, this);

    input()->set_key_callback(
        'y', [this] { m_deferedlightning.select_bias = (m_deferedlightning.select_bias + 1) % 2; }, this);

    float step = 0.001;

    input()->set_key_callback(
        'z', [this, step] { m_deferedlightning.shadow_bias[m_deferedlightning.select_bias] += step; }, this);

    input()->set_key_callback(
        'x', [this, step] { m_deferedlightning.shadow_bias[m_deferedlightning.select_bias] -= step; }, this);

    m_main_pass = [&] {
        auto builder         = vke::RenderPassBuilder();
        uint32_t albedo_spec = builder.add_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkClearValue{.color{1.f, 1.f, 1.f, 0.f}});
        uint32_t normal      = builder.add_attachment(VK_FORMAT_R16G16B16A16_UNORM, VkClearValue{.color{0.5f, 0.5f, 0.5f, 0.f}});
        uint32_t depth       = builder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil = {.depth = 1.f}});
        uint32_t swc         = builder.add_swapchain_attachment(m_core.get());
        builder.add_subpass({albedo_spec, normal}, depth);
        builder.add_subpass({swc}, std::nullopt, {albedo_spec, normal, depth});
        builder.add_subpass({swc},depth);

        return builder.build(m_core.get(), m_core->width(), m_core->height());
    }();

    m_shadow_pass = [&] {
        const uint32_t shadow_size = 1024 * 4;

        auto builder   = vke::RenderPassBuilder();
        uint32_t depth = builder.add_attachment(VK_FORMAT_D16_UNORM, VkClearValue{.depthStencil = {.depth = 1.f}}, true);
        builder.add_subpass({}, depth);
        return builder.build(m_core.get(), shadow_size, shadow_size);
    }();

    m_lifetime_pool = std::make_unique<vke::DescriptorPool>(m_core->device());

    for (auto& frame_data : m_frame_datas)
    {
        frame_data.pool = std::make_unique<vke::DescriptorPool>(m_core->device());
        frame_data.dubo = std::make_unique<vke::DynamicUBO>(m_core.get(), DYNAMIC_UBO_CAP);
    }
    m_deferedlightning.shadow_sampler = m_core->create_sampler(VK_FILTER_LINEAR,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    m_deferedlightning.set_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_dubo(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT)
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
    m_primative_renderer->cleanup();
    m_chunk_renderer->cleanup();
    m_textrenderer->cleanup();
    m_main_pass->clean();
    m_shadow_pass->clean();
    m_lifetime_pool->clean();

    auto device = m_core->device();

    vkDestroyPipelineLayout(device, m_deferedlightning.pipeline_layout, nullptr);
    vkDestroyPipeline(device, m_deferedlightning.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, m_deferedlightning.set_layout, nullptr);
    vkDestroySampler(device, m_deferedlightning.shadow_sampler, nullptr);

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
    m_chunk_renderer->register_renderpass(m_shadow_pass.get(), 0, true);

    m_textrenderer = std::make_unique<TextRenderer>(m_core.get(), m_lifetime_pool.get(), cmd, cleanup_queue);
    m_textrenderer->register_renderpass(m_main_pass.get(), 2);

    m_primative_renderer = std::make_unique<PrimativeRenderer>(m_core.get());
    m_primative_renderer->register_renderpass(m_main_pass.get(), 2);
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

    cleanup_queue.push_back([&, current_f] {
        current_f->pool->reset_pools();
        current_f->dubo->reset();
    });

    for (auto c : m_world->get_updated_chunks())
    {
        m_chunk_renderer->mesh_chunk(c);
    }

    const float shadow_width   = m_deferedlightning.shadow_width;
    glm::vec3 sun_pos          = {0.f, 120.f, 0.f};
    glm::mat4 shadow_proj_view = glm::ortho(-shadow_width, shadow_width, shadow_width, -shadow_width, 0.f, m_deferedlightning.shadow_far) *
                                 glm::lookAt(sun_pos, sun_pos + m_deferedlightning.sun_dir, glm::vec3(0.f, 1.f, 0.f));

    m_textrenderer->render_text_px(m_main_pass.get(),
        fmt::format("shadow bias min: {}\nshadow bias max: {}", m_deferedlightning.shadow_bias.x, m_deferedlightning.shadow_bias.y),
        glm::vec2(20.f, 25.f), glm::vec2(16.f, 16.f));

    m_primative_renderer->frustrum_frame(shadow_proj_view, 0xFF'00'00'00);

    m_shadow_pass->begin(cmd);

    render_objects(cmd, m_shadow_pass.get(), shadow_proj_view);

    m_shadow_pass->end(cmd);

    m_main_pass->begin(cmd);

    glm::mat4 proj_view = m_game->camera()->proj(m_main_pass->size()) * m_game->player()->view();
    render_objects(cmd, m_main_pass.get(), proj_view);

    m_main_pass->next_subpass(cmd);

    defered_lightning(cmd, proj_view, shadow_proj_view);

    m_main_pass->next_subpass(cmd);

    m_textrenderer->render(cmd, m_main_pass.get());

    m_primative_renderer->render(cmd, m_main_pass.get(), 1, proj_view);

    m_main_pass->end(cmd);
}

void VkRenderer::render_objects(VkCommandBuffer cmd, vke::RenderPass* render_pass, const glm::mat4& proj_view)
{
    m_chunk_renderer->render(cmd, current_frame().pool.get(), render_pass, 0, proj_view);
}

void VkRenderer::defered_lightning(VkCommandBuffer cmd, const glm::mat4& proj_view, const glm::mat4& shadow_proj_view)
{
    auto& dubo = current_frame().dubo;

    auto [offset, size] = dubo->push_data(glsl::SceneBuffer{
        .proj_view        = proj_view,
        .inv_proj_view    = glm::inverse(proj_view),
        .shadow_proj_view = {shadow_proj_view},
        .sun_light_dir    = glm::vec4(glm::normalize(m_deferedlightning.sun_dir), 1.f),
        .render_mode      = glm::ivec4(m_deferedlightning.render_mode),
        .shadow_bias      = {m_deferedlightning.shadow_bias / m_deferedlightning.shadow_far},
    });

    auto set =
        vke::DescriptorSetBuilder()
            .add_input_attachment(m_main_pass->get_subpass(0).get_attachment(0)->view)
            .add_input_attachment(m_main_pass->get_subpass(0).get_attachment(1)->view)
            .add_input_attachment(m_main_pass->get_subpass(0).get_depth_attachment()->view)
            .add_dubo(dubo->buffer(), offset, size, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_image_sampler(*m_shadow_pass->get_subpass(0).get_depth_attachment()->vke_image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_deferedlightning.shadow_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
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