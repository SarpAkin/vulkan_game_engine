#pragma once

#include <vector>

#include <vke/iinput.hpp>

#include "player.hpp"
#include "world/world.hpp"

#include "../render/renderer/irenderer.hpp"

class Game
{
public:
    Game();

    void run();
    inline World* world() { return m_world.get(); }
    inline Player* player() { return &m_player; }
    inline Camera* camera() { return &m_camera; }

private:
    void update(float delta_t);

private:
    std::unique_ptr<World> m_world;
    std::unique_ptr<IRenderer> m_renderer;
    vke::IInput* m_input;

    Player m_player;
    Camera m_camera;
};