#include "renderer.hpp"

#include <random>

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

    input()->set_key_callback(
        'u', [this] { m_shadow_blur_on = !m_shadow_blur_on; }, this);

    float step = 0.001;

    input()->set_key_callback(
        'z', [this, step] { m_deferedlightning.shadow_bias[m_deferedlightning.select_bias] += step; }, this);

    input()->set_key_callback(
        'x', [this, step] { m_deferedlightning.shadow_bias[m_deferedlightning.select_bias] -= step; }, this);

    m_main_pass = [&] {
        auto gbuilder = vke::RenderPassBuilder();

        uint32_t galbedo_spec = gbuilder.add_attachment(VK_FORMAT_R8G8B8A8_SRGB, VkClearValue{.color{1.f, 1.f, 1.f, 0.f}}, true);
        uint32_t gnormal      = gbuilder.add_attachment(VK_FORMAT_R16G16B16A16_UNORM, VkClearValue{.color{0.5f, 0.5f, 0.5f, 0.f}});
        uint32_t gdepth       = gbuilder.add_attachment(VK_FORMAT_D32_SFLOAT, VkClearValue{.depthStencil = {.depth = 1.f}});
        uint32_t gshadow      = gbuilder.add_attachment(VK_FORMAT_R8_UNORM, std::nullopt, true);
        gbuilder.add_subpass({galbedo_spec, gnormal}, gdepth);
        gbuilder.add_subpass({gshadow}, std::nullopt, {gnormal, gdepth});
        m_gpass = gbuilder.build(m_core.get(), m_core->width(), m_core->height());

        m_deferedlightning.gshadow_att  = gshadow;
        m_deferedlightning.gsalbedo_att = galbedo_spec;

        auto builder = vke::RenderPassBuilder();

        uint32_t swc   = builder.add_swapchain_attachment(m_core.get(), VkClearValue{.color = {0.f, 0.f, 0.f}});
        uint32_t depth = builder.add_external_attachment(m_gpass.get(), gdepth);
        builder.add_subpass({swc}, depth);

        return builder.build(m_core.get(), m_core->width(), m_core->height());
    }();

    for (int i = 0; i < 2; ++i)
    {
        auto builder     = vke::RenderPassBuilder();
        uint32_t blurred = builder.add_attachment(VK_FORMAT_R8_UNORM, std::nullopt, true);
        builder.add_subpass({blurred});
        m_blurpass[i] = builder.build(m_core.get(), m_core->width(), m_core->height());
    }

    m_shadow_pass = [&] {
        const uint32_t shadow_size = 1024 * 8;

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
    m_deferedlightning.linear_sampler = m_core->create_sampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    m_deferedlightning.gset_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_input_attachment(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_dubo(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_core->device());

    m_deferedlightning.gpipeline_layout = vke::PipelineLayoutBuilder().add_set_layout(m_deferedlightning.gset_layout).build(m_core->device());

    m_deferedlightning.gpipeline = [&] {
        auto builder = vke::PipelineBuilder();
        builder.set_depth_testing(false);
        builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
        builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "../screen_quad.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "defered_shadow.frag").value());
        builder.pipeline_layout = m_deferedlightning.gpipeline_layout;

        return builder.build(m_core.get(), m_gpass.get(), 1).value();
    }();

    m_deferedlightning.final_set_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_dubo(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_core->device());

    m_deferedlightning.final_pipeline_layout =
        vke::PipelineLayoutBuilder()
            .add_set_layout(m_deferedlightning.final_set_layout)
            .build(m_core->device());

    {
        auto builder = vke::PipelineBuilder();
        builder.set_depth_testing(false);
        builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
        builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "../screen_quad.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "final.frag").value());
        builder.pipeline_layout = m_deferedlightning.final_pipeline_layout;

        m_deferedlightning.final_pipeline = builder.build(m_core.get(), m_main_pass.get(), 0).value();
    }

    m_deferedlightning.blur_set_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_core->device());

    m_deferedlightning.blur_pipeline_layout =
        vke::PipelineLayoutBuilder()
            .add_set_layout(m_deferedlightning.blur_set_layout)
            .add_push_constant<uint32_t>(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(m_core->device());

    {
        auto builder = vke::PipelineBuilder();
        builder.set_depth_testing(false);
        builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
        builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "../screen_quad.vert").value());
        builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "gaussian_blur.frag").value());
        builder.pipeline_layout = m_deferedlightning.blur_pipeline_layout;

        m_deferedlightning.blur_pipeline = builder.build(m_core.get(), m_blurpass[0].get(), 0).value();
    }

#if POISSON_DISC_SIZE != 0
    m_deferedlightning.m_poisson_disc = std::unique_ptr<glm::vec2[]>(new glm::vec2[POISSON_DISC_SIZE]);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0, 1.0);
    std::uniform_real_distribution<float> dis2(0.2, .6);

    for (int i = 0; i < POISSON_DISC_SIZE; ++i)
    {
        m_deferedlightning.m_poisson_disc[i] = glm::normalize(glm::vec2(dis(gen), dis(gen))) * dis2(gen) / 2000.f;
    }
#endif
}

VkRenderer::~VkRenderer()
{
    m_primative_renderer->cleanup();
    m_chunk_renderer->cleanup();
    m_textrenderer->cleanup();
    m_main_pass->clean();
    m_gpass->clean();
    m_shadow_pass->clean();
    for (auto& bp : m_blurpass)
        bp->clean();
    m_lifetime_pool->clean();

    auto device = m_core->device();

    vkDestroyPipelineLayout(device, m_deferedlightning.gpipeline_layout, nullptr);
    vkDestroyPipeline(device, m_deferedlightning.gpipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, m_deferedlightning.gset_layout, nullptr);

    vkDestroyPipelineLayout(device, m_deferedlightning.final_pipeline_layout, nullptr);
    vkDestroyPipeline(device, m_deferedlightning.final_pipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, m_deferedlightning.final_set_layout, nullptr);
    
    vkDestroyPipelineLayout(device, m_deferedlightning.blur_pipeline_layout, nullptr);
    vkDestroyPipeline(device, m_deferedlightning.blur_pipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, m_deferedlightning.blur_set_layout, nullptr);

    vkDestroySampler(device, m_deferedlightning.linear_sampler, nullptr);

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
    m_chunk_renderer->register_renderpass(m_gpass.get(), 0, false);
    m_chunk_renderer->register_renderpass(m_shadow_pass.get(), 0, true);

    m_textrenderer = std::make_unique<TextRenderer>(m_core.get(), m_lifetime_pool.get(), cmd, cleanup_queue);
    m_textrenderer->register_renderpass(m_main_pass.get(), 0);

    m_primative_renderer = std::make_unique<PrimativeRenderer>(m_core.get());
    m_primative_renderer->register_renderpass(m_main_pass.get(), 0);
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

    glm::mat4 proj_view = m_game->camera()->proj(m_gpass->size()) * m_game->player()->view();

    m_gpass->begin(cmd);

    render_objects(cmd, m_gpass.get(), proj_view);

    m_gpass->next_subpass(cmd);

    defered_lightning(cmd, proj_view, shadow_proj_view);

    m_gpass->end(cmd);

    if(m_shadow_blur_on) blur_shadows(cmd);

    m_main_pass->begin(cmd);

    final_lightning(cmd, proj_view);

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

    auto sc_buffer = glsl::SceneBuffer{
        .proj_view        = proj_view,
        .inv_proj_view    = glm::inverse(proj_view),
        .shadow_proj_view = {shadow_proj_view},
        .sun_light_dir    = glm::vec4(glm::normalize(m_deferedlightning.sun_dir), 1.f),
        .render_mode      = glm::ivec4(m_deferedlightning.render_mode),
        .shadow_bias      = {m_deferedlightning.shadow_bias / m_deferedlightning.shadow_far},
    };
#if POISSON_DISC_SIZE != 0
    memcpy(sc_buffer.poisson_disc, m_deferedlightning.m_poisson_disc.get(), POISSON_DISC_SIZE * sizeof(glm::vec2));
#endif

    auto [offset, size] = dubo->push_data(sc_buffer);

    // gpass shadows
    {

        auto set =
            vke::DescriptorSetBuilder()
                .add_input_attachment(m_gpass->get_subpass(0).get_attachment(1)->view)
                .add_input_attachment(m_gpass->get_subpass(0).get_depth_attachment()->view)
                .add_dubo(dubo->buffer(), offset, size, VK_SHADER_STAGE_FRAGMENT_BIT)
                .add_image_sampler(*m_shadow_pass->get_subpass(0).get_depth_attachment()->vke_image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    m_deferedlightning.linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(*current_frame().pool, m_deferedlightning.gset_layout);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.gpipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.gpipeline_layout, 0, 1, &set, 1, &offset);

        vkCmdDraw(cmd, 6, 1, 0, 0);
    }
}

void VkRenderer::blur_shadows(VkCommandBuffer cmd)
{
    for(int i = 0;i < 2;++i)
    {        
        m_blurpass[i]->begin(cmd);

        auto set =
            vke::DescriptorSetBuilder()
                .add_image_sampler(i == 0 ? (*m_gpass->get_attachment(m_deferedlightning.gshadow_att).vke_image) : *m_blurpass[0]->get_attachment(0).vke_image,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    m_deferedlightning.linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(*current_frame().pool, m_deferedlightning.blur_set_layout);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.blur_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.blur_pipeline_layout, 0, 1, &set, 0, nullptr);
        uint32_t axis = i;
        vkCmdPushConstants(cmd, m_deferedlightning.blur_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(axis), &axis);

        vkCmdDraw(cmd, 6, 1, 0, 0);

        m_blurpass[i]->end(cmd);
    }
}

void VkRenderer::final_lightning(VkCommandBuffer cmd, const glm::mat4& proj_view)
{
    auto& dubo = current_frame().dubo;

    auto sc_buffer = glsl::SceneBuffer{
        .proj_view     = proj_view,
        .inv_proj_view = glm::inverse(proj_view),
        .sun_light_dir = glm::vec4(glm::normalize(m_deferedlightning.sun_dir), 1.f),
        .render_mode   = glm::ivec4(m_deferedlightning.render_mode),
        .shadow_bias   = {m_deferedlightning.shadow_bias / m_deferedlightning.shadow_far},
    };
#if POISSON_DISC_SIZE != 0
    memcpy(sc_buffer.poisson_disc, m_deferedlightning.m_poisson_disc.get(), POISSON_DISC_SIZE * sizeof(glm::vec2));
#endif

    auto [offset, size] = dubo->push_data(sc_buffer);

    auto set =
        vke::DescriptorSetBuilder()
            .add_image_sampler(*m_gpass->get_attachment(m_deferedlightning.gsalbedo_att).vke_image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_deferedlightning.linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_image_sampler(!m_shadow_blur_on ? *m_gpass->get_attachment(m_deferedlightning.gshadow_att).vke_image : *m_blurpass[1]->get_attachment(0).vke_image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m_deferedlightning.linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_dubo(dubo->buffer(), offset, size, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(*current_frame().pool, m_deferedlightning.final_set_layout);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.final_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferedlightning.final_pipeline_layout, 0, 1, &set, 1, &offset);

    vkCmdDraw(cmd, 6, 1, 0, 0);
}

// irenderer.hpp
std::unique_ptr<IRenderer> IRenderer::crate_vulkan_renderer(Game* game)
{
    return std::make_unique<VkRenderer>(game);
}