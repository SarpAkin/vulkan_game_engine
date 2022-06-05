#pragma once

#include <functional>

#include <glm/vec2.hpp>

namespace vke
{
class IInput
{
public:
    virtual bool is_key_pressed(uint32_t key) = 0;
    virtual glm::vec2 mouse_delta() const     = 0;
    virtual void set_key_callback(uint32_t key, std::function<void()> f, void* owner)=0;
    virtual void remove_key_callback(uint32_t key,void* owner)=0;
};
} // namespace vke
