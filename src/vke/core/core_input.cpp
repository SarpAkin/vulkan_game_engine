#include "core.hpp"

#include <SDL2/SDL.h>

namespace vke
{

void Core::handle_input()
{
    m_mouse_delta_x = m_mouse_delta_y = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            m_running = false;
            break;
        case SDL_KEYDOWN:
            m_keystates[e.key.keysym.sym] = true;
            if (auto it = m_on_key_down.find(e.key.keysym.sym); it != m_on_key_down.end())
                for (auto& [_, f] : it->second)
                    f();
            break;
        case SDL_KEYUP:
            m_keystates[e.key.keysym.sym] = false;
            break;

        case SDL_MOUSEBUTTONDOWN:
            m_mouse_button_states[e.button.button] = true;
            if (auto it = m_on_mouse_click.find(e.button.button); it != m_on_mouse_click.end())
                for (auto& [_, f] : it->second)
                    f();

            break;

        case SDL_MOUSEBUTTONUP:
            m_mouse_button_states[e.button.button] = false;
            break;
        case SDL_MOUSEMOTION:

            m_mouse_delta_x = m_mouse_x - e.motion.x;
            m_mouse_delta_y = m_mouse_y - e.motion.y;

            m_mouse_x = e.motion.x;
            m_mouse_y = e.motion.y;
            break;
        }
    }

    if (m_mouse_captured)
    {

        SDL_WarpMouseInWindow(m_window, width() / 2, height() / 2);
        m_mouse_x = (float)width() / 2;
        m_mouse_y = (float)height() / 2;
    }

}

bool Core::is_key_pressed(uint32_t key) { return m_keystates[key]; }
glm::vec2 Core::mouse_delta() const { return glm::vec2(m_mouse_delta_x, m_mouse_delta_y); }
void Core::set_key_callback(uint32_t key, std::function<void()> f, void* owner) { m_on_key_down[key][owner] = f; };
void Core::remove_key_callback(uint32_t key, void* owner) { m_on_key_down[key].erase(owner); };

} // namespace vke
