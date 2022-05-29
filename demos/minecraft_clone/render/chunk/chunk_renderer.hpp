#pragma once

#include <unordered_map>

#include <glm/mat4x4.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vke/core/core.hpp>
#include <vke/renderpass.hpp>

#include "../render_system.hpp"

namespace vke
{
class DescriptorPool;
}

class Chunk;

class ChunkRenderer : public IRenderSystem
{
public:
    ChunkRenderer(vke::Core* core, vke::DescriptorPool& pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& init_cleanup_queue);

    void register_renderpass(vke::RenderPass* render_pass, int subpass, bool shadow);

    void pre_render(VkCommandBuffer cmd, vke::RenderPass* render_pass);
    void render(VkCommandBuffer cmd, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view);

    void mesh_chunk(const Chunk* chunk);
    void delete_chunk(glm::ivec2 chunk_pos);

    void cleanup() override;

private:
    vke::Core* m_core;

    std::unique_ptr<vke::Image> m_block_textures;
    std::unique_ptr<vke::Buffer> m_quad_indicies;

    VkDescriptorSetLayout m_texture_set_layout;
    VkDescriptorSet m_texture_set;
    VkSampler m_linear_sampler;

    VkPipelineLayout m_chunk_playout;

    std::vector<std::function<void()>> m_frame_cleanup;

    struct RPData
    {
        bool shadow;
        VkPipeline pipeline;
    };

    struct VerticalChunk
    {
        uint32_t vert_offset;
        uint32_t vert_count;
        int32_t y;
    };

    struct ChunkMesh
    {
        std::unique_ptr<vke::Buffer> vert_buffer;
        std::vector<VerticalChunk> vertical_chunks;
    };

    std::unordered_map<glm::ivec2, ChunkMesh> m_chunk_meshes;

    std::unordered_map<vke::RenderPass*, RPData> m_rpdata; // render pass data
};