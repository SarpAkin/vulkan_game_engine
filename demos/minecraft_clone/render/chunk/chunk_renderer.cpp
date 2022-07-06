#include "chunk_renderer.hpp"

#include <fmt/core.h>

#include <glm/gtx/transform.hpp>

#include <string.h>

#include <vke/descriptor_set_builder.hpp>
#include <vke/pipeline_builder.hpp>

#include "../../util/fill_array.hpp"
#include "../../util/vec_format.hpp"

#include "chunk_mesher.hpp"

#include "chunk_shared.hpp"

namespace
{
struct Push
{
    glm::mat4 mvp;
    uint32_t cpos_offset;
    uint32_t color;
};

struct CullPush
{
    glsl::Frustrum frustrum;
    uint32_t chunk_count;
};

} // namespace

class ChunkRenderer::MeshBuffer
{
public:
    MeshBuffer(vke::Core* core, uint32_t vert_capacity, uint32_t id, bool stencil)
        : vert_cap(vert_capacity), id(id)
    {
        buffer = core->allocate_buffer(VkBufferUsageFlagBits(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), vert_cap * sizeof(Quad::QuadVert), stencil);
    }

    struct VChunkMesh
    {
        uint32_t vert_count;
        uint32_t vert_offset;
    };

    const uint32_t vert_cap;
    const uint32_t id;
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

    uint32_t get_chunk_count()
    {
        return m_vchunks.size();
    }

    uint32_t get_mesh_buffer_id()
    {
        return id;
    }

private:
    std::unordered_map<glm::ivec3, VChunkMesh> m_vchunks;
    uint32_t m_top = 0;
};

ChunkRenderer::ChunkMeshStencil::ChunkMeshStencil(vke::Core* core, uint32_t quad_cap)
{
    buffer = core->allocate_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(Quad) * quad_cap, true);
}

ChunkRenderer::ChunkRenderer(vke::Core* core, vke::DescriptorPool& pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& init_cleanup_queue)
{
    assert(core != nullptr);
    m_core = core;

    m_block_textures = core->load_png("demos/minecraft_clone/textures/tileatlas.png", cmd, init_cleanup_queue);

    m_texture_set_layout  = vke::DescriptorSetLayoutBuilder().add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT).build(core->device());
    m_chunkpos_set_layout = vke::DescriptorSetLayoutBuilder().add_ssbo(VK_SHADER_STAGE_VERTEX_BIT).build(core->device());

    m_chunk_gpudata = core->allocate_buffer(VkBufferUsageFlagBits(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), sizeof(glm::ivec4) * m_chunk_capacity, false);

    for (auto& frame_data : m_frame_datas)
    {

        frame_data.chunk_data_stencil = core->allocate_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(glm::ivec4) * 1024, true);

        frame_data.chunk_mesh_stencil = std::make_unique<ChunkMeshStencil>(core, 1024 * 1024);
    }

    m_chunk_p_layout =
        vke::PipelineLayoutBuilder()
            .add_push_constant<Push>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
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

    m_chunkcull_d_layout =
        vke::DescriptorSetLayoutBuilder()
            .add_ssbo(VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(VK_SHADER_STAGE_COMPUTE_BIT)
            .build(m_core->device());

    m_chunkcull_p_layout =
        vke::PipelineLayoutBuilder()
            .add_set_layout(m_chunkcull_d_layout)
            .add_push_constant<CullPush>(VK_SHADER_STAGE_COMPUTE_BIT)
            .build(m_core->device());

    m_chunkcull_pipeline =
        vke::ComputePipelineBuilder()
            .set_pipeline_layout(m_chunkcull_p_layout)
            .add_shader_stage(VK_SHADER_STAGE_COMPUTE_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "chunk_cull2.comp").value())
            .build(m_core)
            .value();
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
    vkDestroyPipelineLayout(device, m_chunk_p_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_texture_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_chunkpos_set_layout, nullptr);

    for (auto& mesh_buffer : m_meshbuffers)
    {
        mesh_buffer->buffer->clean_up();
    }

    for (auto& fdata : m_frame_datas)
    {
    };
}

void ChunkRenderer::register_renderpass(vke::RenderPass* render_pass, int subpass, bool shadow)
{

    m_rpdata[render_pass] = RPData{
        .shadow   = shadow,
        .pipeline = [&] {
            auto builder = vke::GraphicsPipelineBuilder();
            builder.set_vertex_input<Quad::QuadVert>();
            builder.set_depth_testing(true);
            builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.pipeline_layout = m_chunk_p_layout;
            builder.set_rasterization(VK_POLYGON_MODE_FILL, shadow ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_BACK_BIT);
            builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), (shadow ? "chunk_mesh.vert.DSHADOW_PASS" : "chunk_mesh.vert")).value());
            if (!shadow) builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, LOAD_LOCAL_SHADER_MODULE(m_core->device(), "chunk_mesh.frag").value());

            return builder.build(m_core, render_pass, subpass).value();
        }(),
        .indirect_draw_buffer = m_core->allocate_buffer(VkBufferUsageFlagBits(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), sizeof(VkDrawIndexedIndirectCommand) * m_chunk_capacity, true),
        .chunk_draw_data      = m_core->allocate_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4) * m_chunk_capacity, true),
        .chunkpool_datas      = fill_array<2>([&](int i) { return m_core->allocate_buffer(VkBufferUsageFlagBits(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), sizeof(glsl::MeshPoolData) * MAX_CHUNKMESH_BUFFERS, true); }),
    };
}

uint32_t ChunkRenderer::register_chunk(glm::ivec3 pos)
{

    if (auto it = m_chunk_meshes.find(pos); it != m_chunk_meshes.end())
    {
        return it->second.chunk_id;
    }

    uint32_t id = m_chunk_id_counter++;

    m_chunk_meshes[pos] = ChunkMeshData{
        .chunk_id = id,
    };

    return id;
}

uint32_t ChunkRenderer::set_chunk_mesh(glm::ivec3 pos, MeshBuffer* mb, uint32_t v_offset, uint32_t v_count)
{
    auto& cdata = m_chunk_meshes[pos];

    cdata.mesh_buffer = mb;

    uint32_t mb_id = mb->get_mesh_buffer_id();

    uint32_t stencil_id = m_chunk_data_stencil_top++;

    m_chunk_data_transfers.push_back(VkBufferCopy{
        .srcOffset = stencil_id * sizeof(glm::uvec4),
        .dstOffset = cdata.chunk_id * sizeof(glm::uvec4),
        .size      = sizeof(glm::uvec4),
    });

    glsl::ChunkGPUData pcdata{
        .pos  = pos,
        .mesh = {
            .buffer_id   = mb_id,
            .vert_offset = v_offset,
            .vert_count  = v_count,
        },
    };

    glm::uvec4 packed = glsl::pack_chunk_gpudata(pcdata);

    get_current_frame().chunk_data_stencil->get_data<glm::uvec4>()[stencil_id] = packed;

    if (auto bb = glsl::unpack_chunk_pos(packed); bb != pos)
    {
        fmt::print("{} {}\n", bb, pos);
        assert(0);
    }

    auto aa = glsl::unpack_mesh_data(glm::uvec2(packed.z, packed.w));
    if (memcmp(&aa, &pcdata.mesh, sizeof(pcdata.mesh)) != 0)
    {
        assert(0);
    }

    return cdata.chunk_id;
}

ChunkRenderer::ChunkMeshStencil* ChunkRenderer::barrow_chunkmesh_stencil()
{
    return get_current_frame().chunk_mesh_stencil.get();
}

void ChunkRenderer::return_chunkmesh_stencil(ChunkMeshStencil* mb)
{
}

void ChunkRenderer::mesh_vchunk(const Chunk* chunk, int vertical)
{
    auto& current_frame = get_current_frame();

    auto* cm_stencil = barrow_chunkmesh_stencil();

    Quad* buf_start = cm_stencil->buffer->get_data<Quad>() + cm_stencil->buffer_top;
    Quad* it        = buf_start;

    if (!mesh_vertical_chunk(chunk, vertical, it, cm_stencil->buffer->get_data_end<Quad>())) return;

    uint32_t vert_count = (it - buf_start) * 4;

    if (vert_count == 0) return;

    auto cpos = glm::ivec3(chunk->x(), vertical, chunk->z());

    if (auto it = m_chunk_meshes.find(cpos); it != m_chunk_meshes.end())
        it->second.mesh_buffer->free_chunkmesh(cpos);
    else
        register_chunk(cpos);

    cm_stencil->meshes.push_back(ChunkMeshStencil::ReadyMeshes{
        .pos         = cpos,
        .vert_count  = vert_count,
        .vert_offset = cm_stencil->buffer_top * 4,
    });

    cm_stencil->buffer_top += it - buf_start;
}

void ChunkRenderer::mesh_chunk(const Chunk* chunk)
{
    for (int i = 0; i < Chunk::vertical_chunk_count; ++i)
    {
        mesh_vchunk(chunk, i);
    }
}

ChunkRenderer::MeshBuffer* ChunkRenderer::allocate_new_meshbuffer()
{
    auto mesh_buffer = std::make_unique<MeshBuffer>(m_core, MESH_BUFFER_VERT_CAP, m_meshbuffer_counter++, false);
    auto p_mb        = mesh_buffer.get();
    m_meshbuffers.push_back(std::move(mesh_buffer));
    return p_mb;
}

void ChunkRenderer::prepare_frame(VkCommandBuffer cmd)
{
    auto& current_frame = get_current_frame();



    auto* mesh_stencil = current_frame.chunk_mesh_stencil.get();

    auto meshes = std::move(mesh_stencil->meshes);
    std::sort(meshes.begin(), meshes.end(), [](ChunkMeshStencil::ReadyMeshes& a, ChunkMeshStencil::ReadyMeshes& b) {
        return a.vert_count < b.vert_count;
    });

    if (meshes.size() == 0) return;

    auto mesh_it = meshes.begin();

    for (auto& mesh_buffer : m_meshbuffers)
    {
        std::vector<VkBufferCopy> buffer_copy;

        while (mesh_it != meshes.end())
        {
            auto& mesh = *mesh_it;

            if (auto allocated_mesh = mesh_buffer->allocate_chunkmesh(mesh.pos, mesh.vert_count))
            {
                set_chunk_mesh(mesh.pos, mesh_buffer.get(), allocated_mesh->vert_offset, allocated_mesh->vert_count);

                buffer_copy.push_back(VkBufferCopy{
                    .srcOffset = mesh.vert_offset * sizeof(Quad::QuadVert),
                    .dstOffset = allocated_mesh->vert_offset * sizeof(Quad::QuadVert),
                    .size      = mesh.vert_count * sizeof(Quad::QuadVert),
                });

                mesh_it++;
            }
            else
            {
                break;
            }

        }

        if (buffer_copy.size())
            vkCmdCopyBuffer(cmd, mesh_stencil->buffer->buffer(), mesh_buffer->buffer->buffer(), buffer_copy.size(), buffer_copy.data());
    }

    while (mesh_it != meshes.end())
    {

        auto mesh_buffer = allocate_new_meshbuffer();
        std::vector<VkBufferCopy> buffer_copy;

        while (mesh_it != meshes.end())
        {
            auto& mesh = *mesh_it;

            if (auto allocated_mesh = mesh_buffer->allocate_chunkmesh(mesh.pos, mesh.vert_count))
            {
                set_chunk_mesh(mesh.pos, mesh_buffer, allocated_mesh->vert_offset, allocated_mesh->vert_count);

                buffer_copy.push_back(VkBufferCopy{
                    .srcOffset = mesh.vert_offset * sizeof(Quad::QuadVert),
                    .dstOffset = allocated_mesh->vert_offset * sizeof(Quad::QuadVert),
                    .size      = mesh.vert_count * sizeof(Quad::QuadVert),
                });

                mesh_it++;
            }
            else
            {
                break;
            }

        }

        if (buffer_copy.size())
            vkCmdCopyBuffer(cmd, mesh_stencil->buffer->buffer(), mesh_buffer->buffer->buffer(), buffer_copy.size(), buffer_copy.data());
    }

    if (m_chunk_data_transfers.size())
    {
        vkCmdCopyBuffer(cmd, current_frame.chunk_data_stencil->buffer(), m_chunk_gpudata->buffer(), m_chunk_data_transfers.size(), m_chunk_data_transfers.data());
        m_chunk_data_transfers.clear();
    }
    m_chunk_data_stencil_top = 0;

    mesh_stencil->buffer_top = 0;

    VkBufferMemoryBarrier barriers[]{
        m_core->buffer_barrier(m_chunk_gpudata.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),

    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, nullptr);
}

void ChunkRenderer::pre_render(VkCommandBuffer cmd, vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass, const glm::mat4& proj_view)
{
    auto& rp_data    = m_rpdata[render_pass];
    auto& frame_data = get_current_frame();

    auto& chunkpool_data = rp_data.chunkpool_datas[m_core->frame_index()];

    uint32_t chunk_counter = 0;

    for (auto& meshbuffer : m_meshbuffers)
    {

        chunkpool_data->get_data<glsl::MeshPoolData>()[meshbuffer->get_mesh_buffer_id()] = {
            .draw_offset = chunk_counter,
            .draw_count  = 0,
        };

        chunk_counter += meshbuffer->get_chunk_count();
    }


    auto set =
        vke::DescriptorSetBuilder()
            .add_ssbo(*m_chunk_gpudata, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(*chunkpool_data, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(*rp_data.indirect_draw_buffer, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_ssbo(*rp_data.chunk_draw_data, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(*frame_pool, m_chunkcull_d_layout);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_chunkcull_p_layout, 0, 1, &set, 0, nullptr);

    CullPush push{
        .frustrum    = glsl::frustrum_from_projection(glm::inverse(proj_view)),
        .chunk_count = m_chunk_id_counter,
    };

    vkCmdPushConstants(cmd, m_chunkcull_p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPush), &push);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_chunkcull_pipeline);
    vkCmdDispatch(cmd, (m_chunk_id_counter + GROUP_X_SIZE - 1) / GROUP_X_SIZE, 1, 1);

    VkBufferMemoryBarrier barriers[]{
        m_core->buffer_barrier(chunkpool_data.get(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
        m_core->buffer_barrier(rp_data.indirect_draw_buffer.get(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
        m_core->buffer_barrier(rp_data.chunk_draw_data.get(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, nullptr);
}

void ChunkRenderer::render(VkCommandBuffer cmd, vke::DescriptorPool* frame_pool, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view)
{
    auto& rp_data = m_rpdata[render_pass];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rp_data.pipeline);
    vkCmdBindIndexBuffer(cmd, m_quad_indicies->buffer(), 0, VK_INDEX_TYPE_UINT16);

    auto& indirect_buffer = rp_data.indirect_draw_buffer;
    auto& chunkpos_buffer = rp_data.chunk_draw_data;

    auto cpos_set = vke::DescriptorSetBuilder().add_ssbo(*chunkpos_buffer, VK_SHADER_STAGE_VERTEX_BIT).build(*frame_pool, m_chunkpos_set_layout);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunk_p_layout, 0, 1, &m_texture_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunk_p_layout, 1, 1, &cpos_set, 0, nullptr);

    auto& meshbuffer_data_buffer = rp_data.chunkpool_datas[m_core->frame_index()];

    uint32_t counter = 0;

    const uint32_t colors[] = {
        0xFF'00'00'FF,
        0xFF'FF'00'FF,
        0xFF'00'FF'FF,
        0x00'FF'00'FF,
        0xFF'FF'FF'FF,
        0x00'00'00'FF,
        0x00'FF'FF'FF,
        0xFF'00'00'FF,
        0xFF'00'00'FF,
        0xFF'00'00'FF,
        0xFF'00'00'FF,
    };

    for (auto& mesh_buffer : m_meshbuffers)
    {
        // if (mesh_buffer->id == 0) continue;

        VkDeviceSize offsets = 0;

        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh_buffer->buffer->buffer(), &offsets);

        uint32_t mesh_buffer_id = mesh_buffer->get_mesh_buffer_id();
        uint32_t draw_offset    = meshbuffer_data_buffer->get_data<glsl::MeshPoolData>()[mesh_buffer_id].draw_offset;

        Push push{
            .mvp         = proj_view,
            .cpos_offset = draw_offset,
            .color = colors[counter],
        };

        vkCmdPushConstants(cmd, m_chunk_p_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push), &push);
        vkCmdDrawIndexedIndirectCount(cmd, indirect_buffer->buffer(), draw_offset * sizeof(VkDrawIndexedIndirectCommand),
            meshbuffer_data_buffer->buffer(), mesh_buffer_id * sizeof(glsl::MeshPoolData) + offsetof(glsl::MeshPoolData, draw_count),
            mesh_buffer->get_chunk_count(), sizeof(VkDrawIndexedIndirectCommand));

        counter++;
    }

    // fmt::print("drawn {} chunks with {} draw calls\n", counter, m_meshbuffers.size());
}