#include "chunk.hpp"

#include <cassert>
#include <cstdlib>

Tile Chunk::get_block(uint32_t x, uint32_t y, uint32_t z)const
{
    uint32_t tile_index = x + z * chunk_size + (y % chunk_size) * chunk_surface_area;

    uint32_t vertical_chunk = y / chunk_size;

    assert(tile_index < chunk_volume && vertical_chunk < vertical_chunk_count);

    return m_vertical_chunks[vertical_chunk] ? m_vertical_chunks[vertical_chunk].get()[tile_index] : Tile::air;
}

void Chunk::set_block(Tile t, uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t tile_index = x + z * chunk_size + (y % chunk_size) * chunk_surface_area;

    uint32_t vertical_chunk = y / chunk_size;

    assert(tile_index < chunk_volume && vertical_chunk < vertical_chunk_count);

    if (m_vertical_chunks[vertical_chunk] == nullptr) m_vertical_chunks[vertical_chunk] = std::unique_ptr<Tile, Free>((Tile*)calloc(chunk_volume / 8, 8));

    m_vertical_chunks[vertical_chunk].get()[tile_index] = t;
}

Tile* Chunk::get_tile_array(uint32_t index)
{
    return index >= 0 && index < vertical_chunk_count ? m_vertical_chunks[index].get() : nullptr;
}

const Tile* Chunk::get_tile_array(uint32_t index) const
{
    return index >= 0 && index < vertical_chunk_count ? m_vertical_chunks[index].get() : nullptr;
}

const Tile* Chunk::get_tile_array_of_neighbor(uint32_t vertical_chunk, TileFacing dir)const
{
    switch (dir)
    {
    case TileFacing::yp:
        return get_tile_array(vertical_chunk + 1);
        break;
    case TileFacing::yn:
        return get_tile_array(vertical_chunk - 1);
        break;
    case TileFacing::xp:
        return m_neighbor.xp ? m_neighbor.xp->get_tile_array(vertical_chunk) : nullptr;
        break;
    case TileFacing::xn:
        return m_neighbor.xn ? m_neighbor.xn->get_tile_array(vertical_chunk) : nullptr;
        break;
    case TileFacing::zp:
        return m_neighbor.zp ? m_neighbor.zp->get_tile_array(vertical_chunk) : nullptr;
        break;
    case TileFacing::zn:
        return m_neighbor.zn ? m_neighbor.zn->get_tile_array(vertical_chunk) : nullptr;
        break;
    }

    return nullptr;
}

Chunk::Chunk()
{
}

Chunk::~Chunk()
{
}
