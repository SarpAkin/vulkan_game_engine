#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/vec2.hpp>

#include "chunk.hpp"

class WorldGen;
class Player;

class World
{
public:
    World(); ~World();

    void set_chunk(std::unique_ptr<Chunk> chunk, glm::ivec2 pos);
    const Chunk* get_chunk(glm::ivec2 pos) const;
    
    bool set_block(const glm::ivec3& pos,Tile tile);

    std::unordered_set<const Chunk*> get_updated_chunks();

    void update(float delta_t);

    void set_player(Player* p){m_player=p;};

private:
    int render_distance = 10;
    int old_render_dist = 10;

    std::unique_ptr<WorldGen> m_world_gen;
    std::unordered_set<const Chunk*> m_updated_chunks;
    std::unordered_map<glm::ivec2, std::unique_ptr<Chunk>> m_chunks;

    Player* m_player;
    glm::ivec2 m_player_old_pos = {0xFFF,0xFFF}; 

};