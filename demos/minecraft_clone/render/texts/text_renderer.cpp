#include "text_renderer.hpp"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vke/descriptor_set_builder.hpp>
#include <vke/pipeline_builder.hpp>

namespace
{

struct Push
{
    glm::vec2 pos;
    glm::vec2 scale;
    glm::vec4 color;
};
} // namespace

struct TextRenderer::FontVert
{
    glm::vec2 pos;
    glm::vec2 uv;
    REFLECT_VERTEX_INPUT_DESCRIPTION(FontVert, pos, uv);
};
extern uint64_t ascii8x8[128];

TextRenderer::TextRenderer(vke::Core* core, vke::DescriptorPool* pool, VkCommandBuffer cmd, std::vector<std::function<void()>>& cleanup_queue)
{
    m_core = core;

    auto buffer  = core->allocate_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 1 * 8 * (8 * 128), true);
    auto data_it = buffer->get_data<uint8_t>();

    for (int i = 0; i < 128; ++i)
    {
        uint64_t bitmap = ascii8x8[i];
        for (int y = 0; y < 8; ++y)
        {
            uint64_t row = (bitmap >> (y)*8) & 0xFF;

            for (int x = 0; x < 8; ++x)
            {
                *(data_it++) = ((row >> x) & 1) * 255;
            }
        }
    }

    m_font_texture = core->buffer_to_image(cmd, buffer.get(), VK_FORMAT_R8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, 8, 8 * 128);

    m_linear_sampler = core->create_sampler(VK_FILTER_NEAREST);

    m_texture_set_layout = vke::DescriptorSetLayoutBuilder().add_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT).build(core->device());
    m_texture_set =
        vke::DescriptorSetBuilder()
            .add_image_sampler(*m_font_texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_linear_sampler, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(*pool, m_texture_set_layout);

    m_pipelinelayout = vke::PipelineLayoutBuilder()
                           .add_set_layout(m_texture_set_layout)
                           .add_push_constant<Push>(VK_SHADER_STAGE_VERTEX_BIT)
                           .build(core->device());

    cleanup_queue.push_back([buffer = std::shared_ptr(std::move(buffer))]() mutable {
        buffer->clean_up();
    });

    for (auto& buf : m_internal_fontbufs)
    {
        buf = core->allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MAX_CHARS * sizeof(FontVert) * 6, true);
    }
}

void TextRenderer::cleanup()
{
    auto device = m_core->device();

    for (auto& [renderpass, pipelines] : m_pipelines)
    {
        vkDestroyPipeline(device, pipelines.font_renderer2D, nullptr);
    }

    vkDestroyPipelineLayout(device, m_pipelinelayout, nullptr);
    vkDestroySampler(device, m_linear_sampler, nullptr);
    vkDestroyDescriptorSetLayout(device, m_texture_set_layout, nullptr);

    m_font_texture->clean_up();

    for (auto& buf : m_internal_fontbufs)
    {
        buf->clean_up();
    }
}

void TextRenderer::register_renderpass(vke::RenderPass* renderpass, int subpass)
{
    m_pipelines[renderpass] = Pipelines{
        .font_renderer2D = [&] {
            auto builder = vke::PipelineBuilder();
            builder.set_vertex_input<FontVert>();
            builder.pipeline_layout = m_pipelinelayout;
            builder.set_depth_testing(false);
            builder.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.set_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);
            builder.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, /**/ LOAD_LOCAL_SHADER_MODULE(m_core->device(), "font.frag").value());
            builder.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, /****/ LOAD_LOCAL_SHADER_MODULE(m_core->device(), "font.vert").value());

            return builder.build(m_core, renderpass, subpass).value();
        }(),
    };
}

void TextRenderer::mesh_string_into_buffer(FontVert*& it, FontVert* vert_buf_end, const std::string& string)
{
    glm::vec2 cursor = {0.f, 0.f};

    const float ATLAS_SIZE      = 128.f;
    const glm::vec2 square_size = {1.f, 1.f / ATLAS_SIZE};

    for (uint8_t c : string)
    {
        if (c > 127) continue;
        if (c == '\n')
        {
            cursor.x = 0.f;
            cursor.y += 8.f;
            continue;
        }
        if (c == ' ' || c == '\t')
        {
            cursor.x += c == '\t' ? 32.f : 8.f;
            continue;
        }

        glm::vec2 base_uv = {0.f, c / ATLAS_SIZE};

        if (it + 6 > vert_buf_end)
            return;

        it[0] = FontVert{.pos = cursor + glm::vec2(0.f, 00.f), .uv = base_uv + glm::vec2(0.f, 0.f)},
        it[1] = FontVert{.pos = cursor + glm::vec2(0.f, -8.f), .uv = base_uv + glm::vec2(0.f, square_size.y)},
        it[2] = FontVert{.pos = cursor + glm::vec2(8.f, 00.f), .uv = base_uv + glm::vec2(square_size.x, 0.f)},
        it[5] = FontVert{.pos = cursor + glm::vec2(8.f, -8.f), .uv = base_uv + glm::vec2(square_size.x, square_size.y)},

        it[3] = it[2];
        it[4] = it[1];

        cursor.x += 8.f;

        it += 6;
    }
}

void TextRenderer::render_text(vke::RenderPass* render_pass, const std::string& s, glm::vec2 pos, glm::vec2 scale)
{
    auto& text_buf = m_internal_fontbufs[m_core->frame_index()];

    auto it       = text_buf->get_data<FontVert>() + m_vert_counter;
    auto it_start = it;
    mesh_string_into_buffer(it, text_buf->get_data_end<FontVert>(), s);
    uint32_t vert_count = it - it_start;
    m_vert_counter += vert_count;
    m_text_args.push_back(TextArgs{
        .render_pass = render_pass,
        .vert_count  = vert_count,
        .vert_offset = static_cast<uint32_t>(it_start - text_buf->get_data<FontVert>()),
        .pos         = pos,
        .scale       = scale,
        .color       = glm::vec4(0.2f, 0.2f, 0.2f, 1.f),
    });
}

std::unique_ptr<vke::Buffer> TextRenderer::mesh_string(const std::string& string)
{
    auto buf = m_core->allocate_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(FontVert) * 6 * (string.size() + 1), true);

    auto it = buf->get_data<FontVert>();
    mesh_string_into_buffer(it, it + (string.size() * 6), string);

    return buf;
}

void TextRenderer::render(VkCommandBuffer cmd, vke::RenderPass* render_pass)
{
    const auto& pipelines = m_pipelines[render_pass];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.font_renderer2D);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelinelayout, 0, 1, &m_texture_set, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_internal_fontbufs[m_core->frame_index()]->buffer(), &offset);

    for(auto& text_arg : m_text_args)
    {
        if(text_arg.render_pass != render_pass) continue;

        Push push{
            .pos   = text_arg.pos,
            .scale = text_arg.scale,
            .color = text_arg.color,
        };
        vkCmdPushConstants(cmd, m_pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

        vkCmdDraw(cmd, text_arg.vert_count, 1, text_arg.vert_offset, 0);
    }
    m_text_args.resize(0);
    m_vert_counter = 0;
}

void TextRenderer::render_textbuf(VkCommandBuffer cmd, vke::RenderPass* render_pass, const vke::Buffer* mesh, glm::vec2 pos, glm::vec2 scale)
{
    const auto& pipelines = m_pipelines[render_pass];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.font_renderer2D);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelinelayout, 0, 1, &m_texture_set, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->buffer(), &offset);
    Push push{
        .pos   = pos,
        .scale = scale,
        .color = glm::vec4(0.2, 0.2, 0.2, 1.0),
    };
    vkCmdPushConstants(cmd, m_pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

    vkCmdDraw(cmd, mesh->size() / sizeof(FontVert), 1, 0, 0);
}

uint64_t ascii8x8[128] = {
    0x0000000000000000, // U+0000 (nul)
    0x0000000000000000, // U+0001
    0x0000000000000000, // U+0002
    0x0000000000000000, // U+0003
    0x0000000000000000, // U+0004
    0x0000000000000000, // U+0005
    0x0000000000000000, // U+0006
    0x0000000000000000, // U+0007
    0x0000000000000000, // U+0008
    0x0000000000000000, // U+0009
    0x0000000000000000, // U+000A
    0x0000000000000000, // U+000B
    0x0000000000000000, // U+000C
    0x0000000000000000, // U+000D
    0x0000000000000000, // U+000E
    0x0000000000000000, // U+000F
    0x0000000000000000, // U+0010
    0x0000000000000000, // U+0011
    0x0000000000000000, // U+0012
    0x0000000000000000, // U+0013
    0x0000000000000000, // U+0014
    0x0000000000000000, // U+0015
    0x0000000000000000, // U+0016
    0x0000000000000000, // U+0017
    0x0000000000000000, // U+0018
    0x0000000000000000, // U+0019
    0x0000000000000000, // U+001A
    0x0000000000000000, // U+001B
    0x0000000000000000, // U+001C
    0x0000000000000000, // U+001D
    0x0000000000000000, // U+001E
    0x0000000000000000, // U+001F
    0x0000000000000000, // U+0020 (space)
    0x183C3C1818001800, // U+0021 (!)
    0x3636000000000000, // U+0022 (")
    0x36367F367F363600, // U+0023 (#)
    0x0C3E031E301F0C00, // U+0024 ($)
    0x006333180C666300, // U+0025 (%)
    0x1C361C6E3B336E00, // U+0026 (&)
    0x0606030000000000, // U+0027 (')
    0x180C0606060C1800, // U+0028 (()
    0x060C1818180C0600, // U+0029 ())
    0x00663CFF3C660000, // U+002A (*)
    0x000C0C3F0C0C0000, // U+002B (+)
    0x00000000000C0C06, // U+002C (,)
    0x0000003F00000000, // U+002D (-)
    0x00000000000C0C00, // U+002E (.)
    0x6030180C06030100, // U+002F (/)
    0x3E63737B6F673E00, // U+0030 (0)
    0x0C0E0C0C0C0C3F00, // U+0031 (1)
    0x1E33301C06333F00, // U+0032 (2)
    0x1E33301C30331E00, // U+0033 (3)
    0x383C36337F307800, // U+0034 (4)
    0x3F031F3030331E00, // U+0035 (5)
    0x1C06031F33331E00, // U+0036 (6)
    0x3F3330180C0C0C00, // U+0037 (7)
    0x1E33331E33331E00, // U+0038 (8)
    0x1E33333E30180E00, // U+0039 (9)
    0x000C0C00000C0C00, // U+003A (:)
    0x000C0C00000C0C06, // U+003B (;)
    0x180C0603060C1800, // U+003C (<)
    0x00003F00003F0000, // U+003D (=)
    0x060C1830180C0600, // U+003E (>)
    0x1E3330180C000C00, // U+003F (?)
    0x3E637B7B7B031E00, // U+0040 (@)
    0x0C1E33333F333300, // U+0041 (A)
    0x3F66663E66663F00, // U+0042 (B)
    0x3C66030303663C00, // U+0043 (C)
    0x1F36666666361F00, // U+0044 (D)
    0x7F46161E16467F00, // U+0045 (E)
    0x7F46161E16060F00, // U+0046 (F)
    0x3C66030373667C00, // U+0047 (G)
    0x3333333F33333300, // U+0048 (H)
    0x1E0C0C0C0C0C1E00, // U+0049 (I)
    0x7830303033331E00, // U+004A (J)
    0x6766361E36666700, // U+004B (K)
    0x0F06060646667F00, // U+004C (L)
    0x63777F7F6B636300, // U+004D (M)
    0x63676F7B73636300, // U+004E (N)
    0x1C36636363361C00, // U+004F (O)
    0x3F66663E06060F00, // U+0050 (P)
    0x1E3333333B1E3800, // U+0051 (Q)
    0x3F66663E36666700, // U+0052 (R)
    0x1E33070E38331E00, // U+0053 (S)
    0x3F2D0C0C0C0C1E00, // U+0054 (T)
    0x3333333333333F00, // U+0055 (U)
    0x33333333331E0C00, // U+0056 (V)
    0x6363636B7F776300, // U+0057 (W)
    0x6363361C1C366300, // U+0058 (X)
    0x3333331E0C0C1E00, // U+0059 (Y)
    0x7F6331184C667F00, // U+005A (Z)
    0x1E06060606061E00, // U+005B ([)
    0x03060C1830604000, // U+005C (\)
    0x1E18181818181E00, // U+005D (])
    0x081C366300000000, // U+005E (^)
    0x00000000000000FF, // U+005F (_)
    0x0C0C180000000000, // U+0060 (`)
    0x00001E303E336E00, // U+0061 (a)
    0x0706063E66663B00, // U+0062 (b)
    0x00001E3303331E00, // U+0063 (c)
    0x3830303e33336E00, // U+0064 (d)
    0x00001E333f031E00, // U+0065 (e)
    0x1C36060f06060F00, // U+0066 (f)
    0x00006E33333E301F, // U+0067 (g)
    0x0706366E66666700, // U+0068 (h)
    0x0C000E0C0C0C1E00, // U+0069 (i)
    0x300030303033331E, // U+006A (j)
    0x070666361E366700, // U+006B (k)
    0x0E0C0C0C0C0C1E00, // U+006C (l)
    0x0000337F7F6B6300, // U+006D (m)
    0x00001F3333333300, // U+006E (n)
    0x00001E3333331E00, // U+006F (o)
    0x00003B66663E060F, // U+0070 (p)
    0x00006E33333E3078, // U+0071 (q)
    0x00003B6E66060F00, // U+0072 (r)
    0x00003E031E301F00, // U+0073 (s)
    0x080C3E0C0C2C1800, // U+0074 (t)
    0x0000333333336E00, // U+0075 (u)
    0x00003333331E0C00, // U+0076 (v)
    0x0000636B7F7F3600, // U+0077 (w)
    0x000063361C366300, // U+0078 (x)
    0x00003333333E301F, // U+0079 (y)
    0x00003F190C263F00, // U+007A (z)
    0x380C0C070C0C3800, // U+007B ({)
    0x1818180018181800, // U+007C (|)
    0x070C0C380C0C0700, // U+007D (})
    0x6E3B000000000000, // U+007E (~)
    0x0000000000000000, // U+007F
};
