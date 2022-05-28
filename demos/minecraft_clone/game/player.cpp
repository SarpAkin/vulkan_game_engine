#include "player.hpp"

#include <glm/gtx/transform.hpp>

#include <vke/core/core.hpp>

void Player::update(vke::Core& core, float delta_t)
{
    pitch += core.mouse_delta().y * mouse_speed;
    yaw -= core.mouse_delta().x * mouse_speed;

    pitch = std::clamp(pitch, glm::radians(-89.f), glm::radians(89.f));

    dir = {
        cos(yaw) * cos(pitch), // x
        sin(pitch),            // y
        sin(yaw) * cos(pitch), // z
    };

    glm::vec3 move_vector = {};

    if (core.is_key_pressed('a')) move_vector.z += 1.f;
    if (core.is_key_pressed('d')) move_vector.z -= 1.f;
    if (core.is_key_pressed('w')) move_vector.x += 1.f;
    if (core.is_key_pressed('s')) move_vector.x -= 1.f;
    if (core.is_key_pressed(' ')) move_vector.y += 1.f;
    if (core.is_key_pressed('c')) move_vector.y -= 1.f;

    move_vector = glm::vec3(dir.x, 0.f, dir.z) * move_vector.x +  //
                  glm::vec3(dir.z, 0.f, -dir.x) * move_vector.z + //
                  glm::vec3(0.f, move_vector.y, 0.f);

    pos += move_vector * delta_t;
}

glm::mat4 Player::view() { return glm::lookAt(pos, pos + dir, glm::vec3(0.f, 1.f, 0.f)); }

glm::mat4 Camera::proj(vke::Core& core)
{
    auto proj = glm::perspective(fov, static_cast<float>(core.width()) / static_cast<float>(core.height()), near, far);
    proj[1][1] *= -1.f;
    return proj;
}