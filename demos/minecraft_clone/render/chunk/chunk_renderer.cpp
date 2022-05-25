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
};
} // namespace

ChunkRenderer::ChunkRenderer(vke::Core* core, vke::DescriptorPool& pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& init_cleanup_queue)
{
    assert(core != nullptr);
    m_core = core;

    m_block_textures = core->load_png("demos/minecraft_clone/textures/tileatlas.png", cmd, init_cleanup_queue);

    fmt::print("aa {}\n", init_cleanup_queue.size());

    m_texture_set_layout = vke::DescriptorSetLayoutBuilder().add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT).build(core->device());

    m_chunk_playout =
        vke::PipelineLayoutBuilder()
            .add_push_constant<Push>(VK_SHADER_STAGE_VERTEX_BIT)
            .add_set_layout(m_texture_set_layout)
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

void ChunkRenderer::cleanup()
{
    auto device = m_core->device();

    for (auto& [renderpass, data] : m_rpdata)
    {
        vkDestroyPipeline(device, data.pipeline, nullptr);
    }

    for (auto& [chunk, mesh] : m_chunk_meshes)
    {
        mesh.vert_buffer->clean_up();
    }

    m_block_textures->clean_up();
    m_quad_indicies->clean_up();

    vkDestroySampler(device, m_linear_sampler, nullptr);
    vkDestroyPipelineLayout(device, m_chunk_playout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_texture_set_layout, nullptr);
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

void ChunkRenderer::mesh_chunk(const Chunk* chunk)
{
    const uint32_t buf_size = 1024 * 64;
    Quad buf[buf_size];

    Quad* it     = &buf[0];
    Quad* it_end = it + buf_size;

    std::vector<VerticalChunk> vchunks;

    for (int i = 0; i < Chunk::vertical_chunk_count; ++i)
    {
        Quad* it_old = it;
        if (mesh_vertical_chunk(chunk, i, it, it_end))
        {
            vchunks.push_back(VerticalChunk{
                .vert_offset = static_cast<uint32_t>((it_old - &buf[0]) * 4),
                .vert_count  = static_cast<uint32_t>((it - it_old) * 4),
                .y           = i,
            });
        }
        else
        {
            it = it_old;
        }
    }

    size_t data_size = (it - &buf[0]) * sizeof(Quad);
    auto gpu_buf     = m_core->allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, data_size, true);
    memcpy(gpu_buf->get_data(), buf, data_size);

    m_chunk_meshes[chunk] = ChunkMesh{
        .vert_buffer     = std::move(gpu_buf),
        .vertical_chunks = std::move(vchunks),
    };
}

void ChunkRenderer::pre_render(VkCommandBuffer cmd, vke::RenderPass* render_pass)
{
}

void ChunkRenderer::render(VkCommandBuffer cmd, vke::RenderPass* render_pass, int subpass, const glm::mat4& proj_view)
{
    auto& rp_data = m_rpdata[render_pass];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rp_data.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_chunk_playout, 0, 1, &m_texture_set, 0, nullptr);
    vkCmdBindIndexBuffer(cmd, m_quad_indicies->buffer(), 0, VK_INDEX_TYPE_UINT16);

    for (auto& [chunk, mesh] : m_chunk_meshes)
    {
        VkDeviceSize offsets = 0;

        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vert_buffer->buffer(), &offsets);

        for (auto& vchunk : mesh.vertical_chunks)
        {
            Push push{
                .mvp = proj_view * glm::translate(glm::vec3(chunk->x(), vchunk.y, chunk->z()) * static_cast<float>(Chunk::chunk_size)),
            };

            vkCmdPushConstants(cmd, m_chunk_playout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);
            vkCmdDrawIndexed(cmd, vchunk.vert_count / 4 * 6, 1, 0, vchunk.vert_offset, 0);
        }
    }
}