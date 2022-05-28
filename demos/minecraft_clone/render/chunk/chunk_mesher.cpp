#include "chunk_mesher.hpp"

#include <math.h>

#include <fmt/format.h>

// #pragma GCC optimize("O3")
// #pragma CLANG optimize("O3")

namespace
{
const Tile empty_vertical_chunk[Chunk::chunk_volume] = {};

bool create_plane(TextureID* out_plane, const Chunk* chunk, uint32_t vertical_chunk_index, uint32_t layer, const TextureID* texture_id_lookup, TileFacing dir)
{

    int32_t x_offset;
    int32_t y_offset;
    int32_t z_offset;

    TextureID texture_or = 0;

    {
        int32_t x_offset_table[] = {Chunk::chunk_surface_area, Chunk::chunk_surface_area, 1, 1, 1, 1};
        int32_t y_offset_table[] = {Chunk::chunk_size, Chunk::chunk_size, Chunk::chunk_size, Chunk::chunk_size, Chunk::chunk_surface_area, Chunk::chunk_surface_area};
        int32_t z_offset_table[] = {1, -1, Chunk::chunk_surface_area, -Chunk::chunk_surface_area, Chunk::chunk_size, -Chunk::chunk_size};

        x_offset = x_offset_table[(int)dir];
        y_offset = y_offset_table[(int)dir];
        z_offset = z_offset_table[(int)dir];
    }

    TextureID* out_plane_it = out_plane;

    const Tile* tile_it = chunk->get_tile_array(vertical_chunk_index) + layer * std::abs(z_offset);

    int32_t facing_plane_layer = layer + z_offset / std::abs(z_offset);

    const Tile* facing_tile_it = tile_it + z_offset;

    if (facing_plane_layer < 0)
    {

        facing_tile_it = chunk->get_tile_array_of_neighbor(vertical_chunk_index, dir);

        if (facing_tile_it == nullptr) facing_tile_it = empty_vertical_chunk;

        facing_tile_it += std::abs(z_offset) * (Chunk::chunk_size - 1);
    }
    else if (facing_plane_layer >= Chunk::chunk_size)
    {
        facing_tile_it = chunk->get_tile_array_of_neighbor(vertical_chunk_index, dir);

        if (facing_tile_it == nullptr) facing_tile_it = empty_vertical_chunk;
    }

    for (int y = 0; y < Chunk::chunk_size; ++y)
    {

        for (int x = 0; x < Chunk::chunk_size; ++x)
        {
            TextureID tile_texture = texture_id_lookup[(uint32_t)tile_it[x * x_offset]];

            tile_texture *= facing_tile_it[x * x_offset] == Tile::air;

            texture_or |= tile_texture;

            out_plane_it[x] = tile_texture;
        }

        out_plane_it += Chunk::chunk_size;
        tile_it += y_offset;
        facing_tile_it += y_offset;
    }

    return texture_or != 0;
}

struct Group
{
    uint8_t start_x;
    uint8_t start_y;
    uint8_t end_x;
    uint8_t end_y;
    TextureID t_id;
};

constexpr uint32_t compress_vec(glm::ivec3 vec)
{
    return ((vec.x & 0x1f) << 10) | ((vec.y & 0x1f) << 5) | ((vec.z & 0x1f) << 0);
}

template <TileFacing dir>
inline void append_mesh_from_groups(Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index)
{

    Quad* quad_it = quad_it_;

    if (group_end - group_start > quad_buf_end - quad_it) [[unlikely]]
        throw std::runtime_error("can't append any more quad quad_buffer is full");

    for (; group_start < group_end; group_start++)
    {

        auto group = *group_start;

        // group.end_x++;
        // group.end_y++;

        auto& quad_ = *(quad_it++);

        constexpr uint32_t dir_to_plane_bits_table[6] = {
            0 << 18, // xp
            0 << 18, // xn
            1 << 18, // yp
            1 << 18, // yn
            2 << 18, // zp
            2 << 18, // zn
        };

        constexpr uint32_t plane_bits = dir_to_plane_bits_table[(size_t)dir];

        // if(group.t_id != 1) {std::cout << "errr\n";}

        uint32_t texture_id = group.t_id << 20;

        uint32_t quad_verts[4] = {texture_id, texture_id, texture_id, texture_id};

        if constexpr (dir == TileFacing::yp)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b010 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b110 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b011 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b111 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({group.start_x, plane_index, group.start_y});
            quad_verts[2] |= top_left_corner_bits | compress_vec({group.start_x, plane_index, group.end_y});
            quad_verts[1] |= bottom_right_corner_bits | compress_vec({group.end_x, plane_index, group.start_y});
            quad_verts[3] |= top_right_corner_bits | compress_vec({group.end_x, plane_index, group.end_y});
        }
        else if constexpr (dir == TileFacing::yn)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b000 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b100 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b001 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b101 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({group.start_x, plane_index, group.start_y});
            quad_verts[1] |= top_left_corner_bits | compress_vec({group.start_x, plane_index, group.end_y});
            quad_verts[2] |= bottom_right_corner_bits | compress_vec({group.end_x, plane_index, group.start_y});
            quad_verts[3] |= top_right_corner_bits | compress_vec({group.end_x, plane_index, group.end_y});
        }
        if constexpr (dir == TileFacing::zp)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b001 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b101 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b011 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b111 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({group.start_x, group.start_y, plane_index});
            quad_verts[1] |= top_left_corner_bits | compress_vec({group.start_x, group.end_y, plane_index});
            quad_verts[2] |= bottom_right_corner_bits | compress_vec({group.end_x, group.start_y, plane_index});
            quad_verts[3] |= top_right_corner_bits | compress_vec({group.end_x, group.end_y, plane_index});
        }
        else if constexpr (dir == TileFacing::zn)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b000 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b100 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b010 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b110 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({group.start_x, group.start_y, plane_index});
            quad_verts[2] |= top_left_corner_bits | compress_vec({group.start_x, group.end_y, plane_index});
            quad_verts[1] |= bottom_right_corner_bits | compress_vec({group.end_x, group.start_y, plane_index});
            quad_verts[3] |= top_right_corner_bits | compress_vec({group.end_x, group.end_y, plane_index});
        }
        if constexpr (dir == TileFacing::xp)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b100 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b110 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b101 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b111 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({plane_index, group.start_x, group.start_y});
            quad_verts[1] |= top_left_corner_bits | compress_vec({plane_index, group.start_x, group.end_y});
            quad_verts[2] |= bottom_right_corner_bits | compress_vec({plane_index, group.end_x, group.start_y});
            quad_verts[3] |= top_right_corner_bits | compress_vec({plane_index, group.end_x, group.end_y});
        }
        else if constexpr (dir == TileFacing::xn)
        {
            constexpr uint32_t bottom_left_corner_bits  = (0b000 << 15) | plane_bits;
            constexpr uint32_t bottom_right_corner_bits = (0b010 << 15) | plane_bits;
            constexpr uint32_t top_left_corner_bits     = (0b001 << 15) | plane_bits;
            constexpr uint32_t top_right_corner_bits    = (0b011 << 15) | plane_bits;

            quad_verts[0] |= bottom_left_corner_bits | compress_vec({plane_index, group.start_x, group.start_y});
            quad_verts[2] |= top_left_corner_bits | compress_vec({plane_index, group.start_x, group.end_y});
            quad_verts[1] |= bottom_right_corner_bits | compress_vec({plane_index, group.end_x, group.start_y});
            quad_verts[3] |= top_right_corner_bits | compress_vec({plane_index, group.end_x, group.end_y});
        }

        quad_.verts[0].data = quad_verts[0];
        quad_.verts[1].data = quad_verts[1];
        quad_.verts[2].data = quad_verts[2];
        quad_.verts[3].data = quad_verts[3];
    }

    quad_it_ = quad_it;
}

//function table to get rid of branches
void (*group_mesh_table[])(Group*, Group*, Quad*&, Quad*, size_t) = {
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::xp>(group_start, group_end, quad_it_, quad_buf_end, plane_index); },
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::xn>(group_start, group_end, quad_it_, quad_buf_end, plane_index); },
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::yp>(group_start, group_end, quad_it_, quad_buf_end, plane_index); },
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::yn>(group_start, group_end, quad_it_, quad_buf_end, plane_index); },
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::zp>(group_start, group_end, quad_it_, quad_buf_end, plane_index); },
    +[](Group* group_start, Group* group_end, Quad*& quad_it_, Quad* quad_buf_end, size_t plane_index) { append_mesh_from_groups<TileFacing::zn>(group_start, group_end, quad_it_, quad_buf_end, plane_index); }};

void mesh_plane(const TextureID* plane, TileFacing dir, uint32_t layer, Quad*& quad_it, Quad* quad_buf_end)
{

    Group group_buffers[2][Chunk::chunk_size];

    auto append_mesh = group_mesh_table[(int)dir];
    
    struct
    {
        Group* buffer;
        Group* top;

        inline void push(Group g)
        {
            *(top++) = g;
        }

        inline void reset()
        {
            top = buffer;
        }

        inline void erase(size_t index)
        {
            top--;
            buffer[index] = *top;
        }

        inline size_t size() const
        {
            return top - buffer;
        }

        inline Group& operator[](size_t index)
        {
            return buffer[index];
        }

    } current_vec{group_buffers[0], group_buffers[0]}, prev_vec{group_buffers[1], group_buffers[1]};

    for (uint8_t y = 0; y < Chunk::chunk_size; ++y)
    {
        auto plane_slice = plane + y * Chunk::chunk_size;

        Group current = {0, y, 0, y, 0};

        for (uint8_t x = 0; x < Chunk::chunk_size; ++x)
        {
            if (auto texture_id = plane_slice[x]; current.t_id != texture_id)
            {

                if (current.t_id != 0)
                {
                    current.end_x = x - 1;

                    for (int i = 0; i < prev_vec.size(); ++i)
                    {
                        Group other = prev_vec[i];
                        if (current.t_id == other.t_id && current.start_x == other.start_x && current.end_x == other.end_x)
                        {
                            current.start_y = other.start_y;
                            prev_vec.erase(i);
                            break;
                        }
                    }

                    current_vec.push(current);
                }

                current = {x, y, 0, y, texture_id};
            }
        }

        current.end_x = Chunk::chunk_size - 1;

        if (current.t_id != 0)
        {
            for (int i = 0; i < prev_vec.size(); ++i)
            {
                Group other = prev_vec[i];
                if (current.t_id == other.t_id && current.start_x == other.start_x && current.end_x == other.end_x)
                {
                    current.start_y = other.start_y;
                    prev_vec.erase(i);
                    break;
                }
            }

            current_vec.push(current);
        }

        //append to mesh

        append_mesh(prev_vec.buffer, prev_vec.top, quad_it, quad_buf_end, layer);

        prev_vec.reset();

        std::swap(current_vec, prev_vec);
    }

    append_mesh(prev_vec.buffer, prev_vec.top, quad_it, quad_buf_end, layer);
}

}; // namespace

bool mesh_vertical_chunk(const Chunk* chunk, size_t vertical_index, Quad*& quad_buf_it, Quad* quad_buf_end)
{
    // BENCHMARK_FUNCTION();

    if (chunk->get_tile_array(vertical_index) == nullptr) return false;

    TextureID plane_buf[Chunk::chunk_surface_area];

    for (int dir = 0; dir < 6; dir += 2)
    {
        for (int i = 0; i < Chunk::chunk_size; ++i)
        {
            if (create_plane(plane_buf, chunk, vertical_index, i, tile_texture_table, (TileFacing)dir))
                mesh_plane(plane_buf, (TileFacing)dir, i, quad_buf_it, quad_buf_end);

            if (create_plane(plane_buf, chunk, vertical_index, i, tile_texture_table, (TileFacing)(dir + 1)))
                mesh_plane(plane_buf, (TileFacing)(dir + 1), i, quad_buf_it, quad_buf_end);
        }
    }

    return true;
}