#pragma once

#include <array>
#include <inttypes.h>
#include <memory>

#include <glm/vec3.hpp>

#include <vke/util.hpp>

// #include "../util/malloc_unique.hpp"
#include "tiles.hpp"

class Chunk
{
public:
    static constexpr int32_t chunk_size           = 32;
    static constexpr int32_t chunk_surface_area   = chunk_size * chunk_size;
    static constexpr int32_t chunk_volume         = chunk_size * chunk_size * chunk_size;
    static constexpr int32_t vertical_chunk_count = 8;

    static inline glm::ivec3 chunk_pos_real_pos(glm::ivec3 vec) { return vec << 5; }
    static inline glm::ivec3 real_pos_to_chunk_pos(glm::ivec3 vec) { return vec >> 5; }
    static inline glm::ivec3 real_pos_to_in_chunk_pos(glm::ivec3 vec) { return vec & 31; }

    Chunk();
    ~Chunk();

    Tile get_block(uint32_t x, uint32_t y, uint32_t z) const;
    void set_block(Tile t, uint32_t x, uint32_t y, uint32_t z);

    Tile* get_tile_array(uint32_t vertical_chunk);
    const Tile* get_tile_array(uint32_t vertical_chunk) const;

    inline uint32_t x() const { return m_pos_x; }
    inline uint32_t z() const { return m_pos_z; }

    inline void set_vertical_chunk(std::unique_ptr<Tile, Free> v_chunk, uint32_t vertical_chunk) { m_vertical_chunks[vertical_chunk] = std::move(v_chunk); }

    const Tile* get_tile_array_of_neighbor(uint32_t vertical_chunk, TileFacing dir) const;

    struct
    {
        Chunk* xp;
        Chunk* xn;
        Chunk* zp;
        Chunk* zn;

    } m_neighbor = {};

    uint32_t m_pos_x, m_pos_z;

private:
    std::array<std::unique_ptr<Tile, Free>, vertical_chunk_count> m_vertical_chunks;
};
