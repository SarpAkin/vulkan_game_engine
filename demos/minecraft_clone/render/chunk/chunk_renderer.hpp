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
    ~ChunkRenderer();

    void register_renderpass(vke::RenderPass* render_pass, int subpass, bool shadow);

    void pre_render(VkCommandBuffer cmd, vke::RenderPass* render_pass);
    void render(VkCommandBuffer cmd,vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view);

    void mesh_chunk(const Chunk* chunk);
    void delete_chunk(glm::ivec2 chunk_pos);

    void cleanup() override;

private:
    constexpr static uint32_t MESH_BUFFER_VERT_CAP = 1024 * 1024;
    constexpr static uint32_t MAX_VCHUNKS = 0xFFFF;

    class MeshBuffer;

    vke::Core* m_core;

    std::unique_ptr<vke::Image> m_block_textures;
    std::unique_ptr<vke::Buffer> m_quad_indicies;

    VkDescriptorSetLayout m_texture_set_layout;
    VkDescriptorSetLayout m_chunkpos_set_layout;

    VkDescriptorSet m_texture_set;
    VkSampler m_linear_sampler;

    VkPipelineLayout m_chunk_playout;

    std::vector<std::function<void()>> m_frame_cleanup;

    struct RPData
    {
        bool shadow;
        VkPipeline pipeline;
    };

    std::vector<std::unique_ptr<MeshBuffer>> m_meshbuffers;
    std::unordered_map<glm::ivec3, MeshBuffer*> m_chunk_meshes;

    // struct VerticalChunk
    // {
    //     uint32_t vert_offset;
    //     uint32_t vert_count;
    //     int32_t y;
    // };

    // struct ChunkMesh
    // {
    //     std::unique_ptr<vke::Buffer> vert_buffer;
    //     std::vector<VerticalChunk> vertical_chunks;
    // };

    // std::unordered_map<glm::ivec2, ChunkMesh> m_chunk_meshes;

    struct FrameData{   
        std::unique_ptr<vke::Buffer> chunkpos_buffer;
        std::unique_ptr<vke::Buffer> indirect_draw_buffer;
    };

    std::array<FrameData, vke::Core::FRAME_OVERLAP> m_frame_datas;

    std::unordered_map<vke::RenderPass*, RPData> m_rpdata; // render pass data
};