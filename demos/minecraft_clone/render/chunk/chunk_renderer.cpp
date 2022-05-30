#include "chunk_renderer.hpp"

#include <fmt/core.h>

#include <glm/gtx/transform.hpp>

#include <string.h>

#include <vke/descriptor_set_builder.hpp>
#include <vke/pipeline_builder.hpp>

#include "chunk_mesher.hpp"

namespace
{
struct Push
{
    glm::mat4 mvp;
    uint32_t cpos_offset;
};
} // namespace

class ChunkRenderer::MeshBuffer
{
public:
    MeshBuffer(vke::Core* core, uint32_t vert_capacity, bool stencil)
        : vert_cap(vert_capacity)
    {
        buffer = core->allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vert_cap * sizeof(Quad::QuadVert), stencil);
    }

    struct VChunkMesh
    {
        uint32_t vert_count;
        uint32_t vert_offset;
    };

    const uint32_t vert_cap;
    std::unique_ptr<vke::Buffer> buffer;

    inline const auto& vchunks() const { return m_vchunks; }

    std::optional<VChunkMesh> allocate_chunkmesh(glm::ivec3 pos, uint32_t vert_count)
    {
        if (m_top + vert_count > vert_cap) return std::nullopt;

        VChunkMesh mesh{
            .vert_count  = vert_count,
            .vert_offset = m_top,
        };

        m_top += vert_count;

        m_vchunks[pos] = mesh;
        return mesh;
    }

    void free_chunkmesh(glm::ivec3 pos)
    {
        m_vchunks.erase(pos);
    }

private:
    std::unordered_map<glm::ivec3, VChunkMesh> m_vchunks;
    uint32_t m_top = 0;
};

ChunkRenderer::ChunkRenderer(vke::Core* core, vke::DescriptorPool& pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& init_cleanup_queue)
{
    assert(core != nullptr);
    m_core = core;

    m_block_textures = core->load_png("demos/minecraft_clone/textures/tileatlas.png", cmd, init_cleanup_queue);

    m_texture_set_layout  = vke::DescriptorSetLayoutBuilder().add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT).build(core->device());
    m_chunkpos_set_layout = vke::DescriptorSetLayoutBuilder().add_ssbo(VK_SHADER_STAGE_VERTEX_BIT).build(core->device());

    for (auto& frame_data : m_frame_datas)
    {
        frame_data.chunkpos_buffer      = core->allocate_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4) * MAX_VCHUNKS, true);
        frame_data.indirect_draw_buffer = core->allocate_buffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, sizeof(VkDrawIndexedIndirectCommand) * MAX_VCHUNKS, true);
    }

    m_chunk_playout =
        vke::PipelineLayoutBuilder()
            .add_push_constant<Push>(VK_SHADER_STAGE_VERTEX_BIT)
            .add_set_layout(m_texture_set_layout)
            .add_set_layout(m_chunkpos_set_layout)
            .build(core->device());

    m_linear_sampler = core->create_sampler(VK_FILTER_NEAREST);

    m_texture_set =
        vke::DescriptorSetBuilder()
            .add_image_sampler(*m_block_textures, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(pool, m_texture_set_layout);

    const size_t MAX_QUAD_COUNT = 16384;

    m_quad_indicies = m_core->allocate_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MAX_QUAD_COUNT * 6 * sizeof(uint16_t), true);

    uint16_t* ib_data = m_quad_indicies->get_data<uint16_t>();

    for (int i = 0; i < MAX_QUAD_COUNT; ++i)
    {
        ib_data[i * 6 + 0] = i * 4 + 0;
        ib_data[i * 6 + 1] = i * 4 + 1;
        ib_data[i * 6 + 2] = i * 4 + 2;
        ib_data[i * 6 + 3] = i * 4 + 2;
        ib_data[i * 6 + 4] = i * 4 + 1;
        ib_data[i * 6 + 5] = i * 4 + 3;
    }
}

ChunkRenderer::~ChunkRenderer()
{
}

void ChunkRenderer::cleanup()
{
    auto device = m_core->device();

    for (auto& [renderpass, data] : m_rpdata)
    {
        vkDestroyPipeline(device, data.pipeline, nullptr);
    }

    m_block_textures->clean_up();
    m_quad_indicies->clean_up();

    vkDestroySampler(device, m_linear_sampler, nullptr);
    vkDestroyPipelineLayout(device, m_chunk_playout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_texture_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_chunkpos_set_layout, nullptr);

    for (auto& mesh_buffer : m_meshbuffers)
    {
        mesh_buffer->buffer->clean_up();
    }

    for (auto& fdata : m_frame_datas)
    {
        fdata.chunkpos_buffer->clean_up();
        fdata.indirect_draw_buffer->clean_up();
    };
}

void ChunkRenderer::register_renderpass(vke::RenderPass* render_pass, int subpass, bool shadow)
{

    m_rpdata[render_pass] = RPData{
        .shadow   = shadow,
        .pipeline = [&] {
            auto builder = vke::PipelineBuilder();
            builder.set_vertex_input<Quad::QuadVert>();
            builder.set_depth_testing(true);
            builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.pipeline_layout = m_chunk_playout;
            builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT);
            builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "chunk_mesh.frag").value());
            builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "chunk_mesh.vert").value());

            return builder.build(m_core, render_pass, subpass).value();
        }(),

    };
}

void ChunkRenderer::delete_chunk(glm::ivec2 chunk_pos)
{
}

void ChunkRenderer::mesh_chunk(const Chunk* chunk)
{
    const uint32_t buf_size = 1024 * 64;
    Quad buf[buf_size];

    Quad* it_end = buf + buf_size;

    // delete_chunk(chunk->pos());

    for (int i = 0; i < Chunk::vertical_chunk_count; ++i)
    {
        Quad* it = buf;
        if (mesh_vertical_chunk(chunk, i, it, it_end))
        {
            uint32_t vert_count = (it - buf) * 4;

            auto cpos = glm::ivec3(chunk->x(), i, chunk->z());

            if (auto it = m_chunk_meshes.find(cpos); it != m_chunk_meshes.end())
                it->second->free_chunkmesh(cpos);

            for (auto& mesh_buffer : m_meshbuffers)
            {
                if (auto mesh = mesh_buffer->allocate_chunkmesh(cpos, vert_count))
                {
                    m_chunk_meshes[cpos] = mesh_buffer.get();
                    memcpy(mesh_buffer->buffer->get_data<Quad::QuadVert>() + mesh->vert_offset, buf, sizeof(Quad::QuadVert) * vert_count);
                    goto outer_loop_end;
                }
            }

            auto mesh_buffer = std::make_unique<MeshBuffer>(m_core, MESH_BUFFER_VERT_CAP, true);
            if (auto mesh = mesh_buffer->allocate_chunkmesh(cpos, vert_count))
            {
                m_chunk_meshes[cpos] = mesh_buffer.get();
                memcpy(mesh_buffer->buffer->get_data<Quad::QuadVert>() + mesh->vert_offset, buf, sizeof(Quad::QuadVert) * vert_count);
            }
            else
            {
                fmt::print("failed to allocate chunkmesh\n");
            }
            m_meshbuffers.push_back(std::move(mesh_buffer));
        }

    outer_loop_end:;
    }

    // m_chunk_meshes[chunk->pos()] = ChunkMesh{
    //     .vert_buffer     = std::move(gpu_buf),
    //     .vertical_chunks = std::move(vchunks),
    // };
}

void ChunkRenderer::pre_render(VkCommandBuffer cmd, vke::RenderPass* render_pass)
{
}

void ChunkRenderer::render(VkCommandBuffer cmd, vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view)
{
    auto& rp_data = m_rpdata[render_pass];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rp_data.pipeline);
    vkCmdBindIndexBuffer(cmd, m_quad_indicies->buffer(), 0, VK_INDEX_TYPE_UINT16);

    auto& indirect_buffer = m_frame_datas[m_core->frame_index()].indirect_draw_buffer;
    auto& chunkpos_buffer = m_frame_datas[m_core->frame_index()].chunkpos_buffer;

    auto* indirect_cmds = indirect_buffer->get_data<VkDrawIndexedIndirectCommand>();
    auto* chunk_poses   = chunkpos_buffer->get_data<glm::vec4>();

    auto cpos_set = vke::DescriptorSetBuilder().add_ssbo(*chunkpos_buffer, VK_SHADER_STAGE_VERTEX_BIT).build(*frame_pool, m_chunkpos_set_layout);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunk_playout, 0, 1, &m_texture_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunk_playout, 1, 1, &cpos_set, 0, nullptr);

    uint32_t counter = 0;

    for (auto& mesh_buffer : m_meshbuffers)
    {
        VkDeviceSize offsets = 0;

        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh_buffer->buffer->buffer(), &offsets);

        uint32_t counter_beg = counter;

        for (auto& [cpos, mesh] : mesh_buffer->vchunks())
        {
            indirect_cmds[counter].firstIndex    = 0;
            indirect_cmds[counter].instanceCount = 1;
            indirect_cmds[counter].indexCount    = (mesh.vert_count / 4) * 6;
            indirect_cmds[counter].vertexOffset  = mesh.vert_offset;
            indirect_cmds[counter].firstInstance = 0;

            chunk_poses[counter] = glm::vec4(cpos * Chunk::chunk_size, 0);

            counter++;
        }

        Push push{
            .mvp         = proj_view,
            .cpos_offset = counter_beg,
        };

        vkCmdPushConstants(cmd, m_chunk_playout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);
        vkCmdDrawIndexedIndirect(cmd, indirect_buffer->buffer(), counter_beg * sizeof(VkDrawIndexedIndirectCommand), counter - counter_beg, sizeof(VkDrawIndexedIndirectCommand));
    }

    fmt::print("drawn {} chunks with {} draw calls\n", counter, m_meshbuffers.size());
}