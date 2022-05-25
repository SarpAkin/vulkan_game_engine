#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/vec2.hpp>

#include "chunk.hpp"

class World
{
public:
    void set_chunk(std::unique_ptr<Chunk> chunk, glm::ivec2 pos);
    const Chunk* get_chunk(glm::ivec2 pos) const;
    
    bool set_block(const glm::ivec3& pos,Tile tile);

    std::unordered_set<const Chunk*> get_updated_chunks();

private:

    std::unordered_set<const Chunk*> m_updated_chunks;
    std::unordered_map<glm::ivec2, std::unique_ptr<Chunk>> m_chunks;
};