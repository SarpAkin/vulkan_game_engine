#include "world.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "../player.hpp"

#include "world_gen.hpp"

World::World()
{
    m_world_gen = std::make_unique<WorldGen>(0xfada23);
    m_world_gen->init(6);
}
World::~World()
{
}

void World::set_chunk(std::unique_ptr<Chunk> chunk, glm::ivec2 pos)
{
    chunk->m_pos_x = pos.x;
    chunk->m_pos_z = pos.y;

    if (auto c = get_chunk(pos))
    {
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
    if (auto c = const_cast<Chunk*>(get_chunk(glm::ivec2(pos.x, pos.z) >> 5)))
    {
        glm::ivec3 in_pos = pos & (Chunk::chunk_size - 1);
        c->set_block(tile, in_pos.x, in_pos.y, in_pos.z);
        m_updated_chunks.emplace(c);

        return true;
    }

    return false;
}

void World::update(float delta_t)
{
    std::vector<std::pair<glm::ivec2, std::unique_ptr<Chunk>>> new_chunks;
    m_world_gen->out_chunks.fetch_available(new_chunks, 10000);

    // if(new_chunks.size()) fmt::print("generated {} chunks\n",new_chunks.size());

    for (auto& [pos, nchunk] : new_chunks)
    {
        set_chunk(std::move(nchunk), pos);
    }

    if (!m_player) return;

    std::vector<glm::ivec2> chunks_to_gen;

    glm::ivec2 player_cpos = glm::floor(glm::vec2(m_player->pos.x, m_player->pos.z) / 32.f);

    if (player_cpos != m_player_old_pos)
    {
        glm::ivec2 old_player_cpos = m_player_old_pos;

        for (int x = -render_distance; x <= render_distance; ++x)
        {
            int y_range = static_cast<float>(std::sqrt(static_cast<float>(render_distance * render_distance - x * x)));

            for (int y = -y_range; y <= y_range; ++y)
            {
                auto c_pos = player_cpos + glm::ivec2(x, y);

                auto diff = c_pos - old_player_cpos;
                diff *= diff;

                if (old_render_dist * old_render_dist < diff.x + diff.y)
                {
                    if (m_chunks.find(c_pos) == m_chunks.end())
                    {
                        m_chunks[c_pos] = nullptr;
                        chunks_to_gen.push_back(c_pos);
                    }
                }
            }
        }

        if (chunks_to_gen.size())
        {
            // fmt::print("generating {}\n", chunks_to_gen.size());
            // fmt::print("chunks: {}",map_vec(chunks_to_gen, [](glm::ivec2& v){return fmt::format("({},{})",v.x,v.y);}));

            m_world_gen->in_chunk_poses.push(std::move(chunks_to_gen));
        }

        m_player_old_pos = player_cpos;
    }
}

std::unordered_set<const Chunk*> World::get_updated_chunks()
{
    return std::move(m_updated_chunks);
}
