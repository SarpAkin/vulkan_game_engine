#ifndef CHUNKR_SHARED_HPP
#define CHUNKR_SHARED_HPP

#include "../glsl_shared.hpp"

#ifdef LANG_CPP
namespace glsl
{
#endif

#define GROUP_X_SIZE 128

struct GhunkGPUMeshData
{
    uint buffer_id;
    uint vert_offset;
    uint vert_count;
};

struct MeshPoolData
{
    uint draw_offset;
    uint draw_count;
    uint padding;
    uint padding_;
};

struct ChunkGPUData
{
    ivec3 pos;
    GhunkGPUMeshData mesh;
};

INLINE uvec4 pack_chunk_gpudata(ChunkGPUData data)
{
    uint ybits = uint(data.pos.y + (1 << 7)) & 0xFF;

    // ybits = 0;

    uvec4 packed;
    packed.x = (ybits << 28)            | (uint(data.pos.x + int(1 << 27)) & 0xFFFFFFFu);
    packed.y = ((ybits & 0xF0) << 28)   | (uint(data.pos.z + int(1 << 27)) & 0xFFFFFFFu);
    packed.z = data.mesh.vert_offset;
    packed.w = (data.mesh.vert_count & 0xFFFFFu) | (data.mesh.buffer_id << 20u);

    return packed;
}

INLINE ivec3 unpack_chunk_pos(uvec2 packed)
{
    ivec3 pos;
    pos.x = int(packed.x & 0xFFFFFFFu) - int(1 << 27);
    pos.z = int(packed.y & 0xFFFFFFFu) - int(1 << 27);
    pos.y = int((packed.x >> 28) | ((packed.y >> 24) & 0xF0));

    return pos;
};

INLINE GhunkGPUMeshData unpack_mesh_data(uvec2 packed)
{
    GhunkGPUMeshData mesh;
    mesh.vert_offset = packed.x;
    mesh.vert_count  = packed.y & 0xFFFFFu;
    mesh.buffer_id   = packed.y >> 20u;

    return mesh;
}

#ifdef LANG_CPP
}
#endif

#endif