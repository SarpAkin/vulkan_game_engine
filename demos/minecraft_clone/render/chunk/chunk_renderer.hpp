#pragma once

#include <unordered_map>

#include <glm/mat4x4.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vke/core/core.hpp>
#include <vke/renderpass.hpp>

#include "../irender_system.hpp"

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

    void prepare_frame(VkCommandBuffer cmd);

    void pre_render(VkCommandBuffer cmd, vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass,const glm::mat4& proj_view);
    void render(VkCommandBuffer cmd, vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view);

    [[deprecated]] void mesh_chunk(const Chunk* chunk);

    void mesh_vchunk(const Chunk* chunk, int vertical);

    void cleanup() override;

private:
    class MeshBuffer;
    struct FrameData;
    struct ChunkMeshStencil;

    ChunkMeshStencil* barrow_chunkmesh_stencil();
    void return_chunkmesh_stencil(ChunkMeshStencil* mb);

    uint32_t register_chunk(glm::ivec3 pos);
    uint32_t set_chunk_mesh(glm::ivec3 pos, MeshBuffer* mb, uint32_t v_offset, uint32_t v_count);
    inline FrameData& get_current_frame() { return m_frame_datas[m_core->frame_index()]; }

    MeshBuffer* allocate_new_meshbuffer();

    constexpr static uint32_t MESH_BUFFER_VERT_CAP = 1024 * 1024;
    // constexpr static uint32_t MAX_VCHUNKS          = 0xFFFF;
    constexpr static uint32_t MAX_CHUNKMESH_BUFFERS = 256;

    vke::Core* m_core;

    std::unique_ptr<vke::Image> m_block_textures;
    std::unique_ptr<vke::Buffer> m_quad_indicies;

    VkDescriptorSetLayout m_texture_set_layout;
    VkDescriptorSetLayout m_chunkpos_set_layout;

    VkDescriptorSetLayout m_chunkcull_d_layout;
    VkPipelineLayout m_chunkcull_p_layout;
    VkPipeline m_chunkcull_pipeline;

    VkDescriptorSet m_texture_set;
    VkSampler m_linear_sampler;

    VkPipelineLayout m_chunk_p_layout;

    std::vector<std::function<void()>> m_frame_cleanup;

    struct RPData
    {
        bool shadow;
        VkPipeline pipeline;
        std::unique_ptr<vke::Buffer> indirect_draw_buffer; // gpu only
        std::unique_ptr<vke::Buffer> chunk_draw_data;      // gpu only
        std::array<std::unique_ptr<vke::Buffer>, vke::Core::FRAME_OVERLAP> chunkpool_datas;
    };

    std::vector<std::unique_ptr<MeshBuffer>> m_meshbuffers;

    struct ChunkMeshData
    {
        uint32_t chunk_id;
        MeshBuffer* mesh_buffer;
    };

    std::unordered_map<glm::ivec3, ChunkMeshData> m_chunk_meshes;

    std::unique_ptr<vke::Buffer> m_chunk_gpudata;

    struct PendingChunkMeshTransfer
    {
        glm::ivec3 pos;
        uint32_t vert_count;
        uint32_t vert_offset;
    };

    struct ChunkMeshStencil
    {
        struct ReadyMeshes
        {
            glm::ivec3 pos;
            uint32_t vert_count;
            uint32_t vert_offset;
        };

        ChunkMeshStencil(vke::Core* core, uint32_t quad_cap);

        std::unique_ptr<vke::Buffer> buffer;
        std::vector<ReadyMeshes> meshes;
        uint32_t buffer_top = 0;
    };

    struct FrameData
    {
        std::unique_ptr<vke::Buffer> chunk_data_stencil;
        std::unique_ptr<ChunkMeshStencil> chunk_mesh_stencil;
    };

    std::array<FrameData, vke::Core::FRAME_OVERLAP> m_frame_datas;
    std::vector<VkBufferCopy> m_chunk_data_transfers;

    uint32_t m_chunk_capacity         = 8 * 1024;
    uint32_t m_chunk_data_stencil_top = 0;
    uint32_t m_chunk_id_counter       = 0;
    uint32_t m_meshbuffer_counter = 0;

    std::unordered_map<vke::RenderPass*, RPData> m_rpdata; // render pass data
};