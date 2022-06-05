#pragma once

#include <fmt/format.h>

#include <iostream>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vke/core/core.hpp>
#include <vke/descriptor_pool.hpp>
#include <vke/descriptor_set_builder.hpp>
#include <vke/dynamic_ubo.hpp>
#include <vke/pipeline_builder.hpp>
#include <vke/renderpass.hpp>
#include <vke/util.hpp>

#include "../chunk/chunk_renderer.hpp"
#include "../texts/text_renderer.hpp"
#include "../debug/primative_renderer.hpp"

#include "../../game/player.hpp"
#include "../../game/world/chunk.hpp"
#include "../../game/world/world.hpp"

#include "irenderer.hpp"

class VkRenderer : public IRenderer
{
public:
    VkRenderer(Game* game);
    ~VkRenderer();

    void run(std::function<void(float)>&& update) override;

    vke::IInput* input() override { return m_core->input(); }

private:
    void init(VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue);
    void frame(std::function<void(float)>& update, vke::Core::FrameArgs& args);

    void render_objects(VkCommandBuffer cmd, vke::RenderPass* render_pass, const glm::mat4& porj_view);
    void defered_lightning(VkCommandBuffer cmd, const glm::mat4& proj_view,const glm::mat4& shadow_proj_view);

    auto& current_frame() { return m_frame_datas[m_core->frame_index()]; }

private:
    std::unique_ptr<vke::Core> m_core;
    std::unique_ptr<vke::RenderPass> m_main_pass;
    std::unique_ptr<vke::RenderPass> m_shadow_pass;
    std::unique_ptr<vke::DescriptorPool> m_lifetime_pool;
    std::unique_ptr<TextRenderer> m_textrenderer;
    std::unique_ptr<ChunkRenderer> m_chunk_renderer;
    std::unique_ptr<PrimativeRenderer> m_primative_renderer;

    struct
    {
        VkDescriptorSetLayout set_layout;
        VkPipelineLayout pipeline_layout;
        VkPipeline pipeline;
        VkSampler shadow_sampler;
        glm::vec3 sun_dir = glm::normalize(glm::vec3(-0.3f,-0.9f,0.3f));
        glm::vec2 shadow_bias = glm::vec2(0.002, 0.080);
        float shadow_far = 300.f;
        float shadow_width = 100.f;
        uint32_t render_mode = 0;
        uint32_t select_bias = 0;
    } m_deferedlightning;

    Game* m_game   = nullptr;
    World* m_world = nullptr;

    constexpr static uint32_t DYNAMIC_UBO_CAP = 1024 * 1024;

    struct FrameData
    {
        std::unique_ptr<vke::DescriptorPool> pool;
        std::unique_ptr<vke::DynamicUBO> dubo;
    };

    std::array<FrameData, vke::Core::FRAME_OVERLAP> m_frame_datas;

    bool initialized = false;
};