#pragma once

#include "../../game/world/chunk.hpp"

#include <vke/pipeline_builder.hpp>

struct Quad
{
    struct QuadVert
    {
        uint32_t data;

        REFLECT_VERTEX_INPUT_DESCRIPTION(QuadVert, data);
    };

    QuadVert verts[4];

    static constexpr int vert_count = 4;
};

bool mesh_vertical_chunk(const Chunk* chunk,size_t vertical_index,Quad*& quad_buf_it,Quad* quad_buf_end);