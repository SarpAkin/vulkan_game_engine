#pragma once

#include <vector>

#include <vke/iinput.hpp>

#include "player.hpp"
#include "world.hpp"

#include "../render/renderer/irenderer.hpp"

class Game
{
public:
    Game();

    void run();
    World* world() { return m_world.get(); }
private:
    void update(float delta_t);

private:
    std::unique_ptr<World> m_world;
    std::unique_ptr<IRenderer> m_renderer;
    vke::IInput* m_input;

    Player m_player;
    Camera m_camera;
};