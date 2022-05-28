#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vke
{
class Core;
}

class Player
{
public:
    glm::vec3 pos = {}, dir = {};
    float pitch = 0, yaw = 0;
    float mouse_speed = 0.05;

public:
    void update(vke::Core& core, float delta_t);

    glm::mat4 view();
};

class Camera
{
public:
    glm::mat4 proj(vke::Core& core);

public:
    float fov = 70.f, near = 0.1, far = 400.f;
};