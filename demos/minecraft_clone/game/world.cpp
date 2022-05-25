#include "world.hpp"

void World::set_chunk(std::unique_ptr<Chunk> chunk, glm::ivec2 pos)
{
    chunk->m_pos_x = pos.x;
    chunk->m_pos_z = pos.y;

    if(auto c = get_chunk(pos)){
        m_updated_chunks.erase(c);
    }

    auto neighbors = chunk->m_neighbor = {
        .xp = const_cast<Chunk*>(get_chunk(pos + glm::ivec2(1, 0))),
        .xn = const_cast<Chunk*>(get_chunk(pos + glm::ivec2(-1, 0))),
        .zp = const_cast<Chunk*>(get_chunk(pos + glm::ivec2(0, 1))),
        .zn = const_cast<Chunk*>(get_chunk(pos + glm::ivec2(0, -1))),
    };

    if (neighbors.xp) neighbors.xp->m_neighbor.xn = chunk.get();
    if (neighbors.xn) neighbors.xn->m_neighbor.xp = chunk.get();
    if (neighbors.zp) neighbors.zp->m_neighbor.zn = chunk.get();
    if (neighbors.zn) neighbors.zn->m_neighbor.zp = chunk.get();

    m_updated_chunks.emplace(chunk.get());

    m_chunks[pos] = std::move(chunk);
}

const Chunk* World::get_chunk(glm::ivec2 pos) const
{
    if (auto it = m_chunks.find(pos); it != m_chunks.end())
    {
        return it->second.get();
    }

    return nullptr;
}

bool World::set_block(const glm::ivec3& pos, Tile tile)
{
    if(auto c = const_cast<Chunk*>(get_chunk(glm::ivec2(pos.x,pos.z) >> 5))){
        glm::ivec3 in_pos = pos & (Chunk::chunk_size - 1);
        c->set_block(tile,in_pos.x, in_pos.y, in_pos.z);
        m_updated_chunks.emplace(c);

        return true;
    }

    return false;
}

std::unordered_set<const Chunk*> World::get_updated_chunks()
{
    return std::move(m_updated_chunks);
}
