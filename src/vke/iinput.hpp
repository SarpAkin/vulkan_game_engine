#pragma once

#include <glm/vec2.hpp>

namespace vke
{
class IInput
{
public:
    virtual bool is_key_pressed(uint32_t key) = 0;
    virtual glm::vec2 mouse_delta() const     = 0;
};
} // namespace vke
