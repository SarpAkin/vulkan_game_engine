#include "game.hpp"

Game::Game()
{
    m_world    = std::make_unique<World>();
    m_renderer = IRenderer::crate_vulkan_renderer(this);
    m_input    = m_renderer->input();
}

void Game::run()
{
    m_renderer->run([this](float delta_t) { this->update(delta_t); });
}

void Game::update(float delta_t)
{
    m_player.update(m_input, delta_t);
}
