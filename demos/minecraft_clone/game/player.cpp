#include "player.hpp"

#include <algorithm>

#include <glm/gtx/transform.hpp>

#include <vke/iinput.hpp>

void Player::update(vke::IInput* input, float delta_t)
{
    pitch += input->mouse_delta().y * mouse_speed;
    yaw -= input->mouse_delta().x * mouse_speed;

    pitch = std::clamp(pitch, glm::radians(-89.f), glm::radians(89.f));

    dir = {
        cos(yaw) * cos(pitch), // x
        sin(pitch),            // y
        sin(yaw) * cos(pitch), // z
    };

    glm::vec3 move_vector = {};

    if (input->is_key_pressed('a')) move_vector.z += 1.f;
    if (input->is_key_pressed('d')) move_vector.z -= 1.f;
    if (input->is_key_pressed('w')) move_vector.x += 1.f;
    if (input->is_key_pressed('s')) move_vector.x -= 1.f;
    if (input->is_key_pressed(' ')) move_vector.y += 1.f;
    if (input->is_key_pressed('c')) move_vector.y -= 1.f;

    move_vector = glm::normalize(glm::vec3(dir.x, 0.f, dir.z)) * move_vector.x +  //
                  glm::normalize(glm::vec3(dir.z, 0.f, -dir.x)) * move_vector.z + //
                  glm::vec3(0.f, move_vector.y, 0.f);

    pos += move_vector * delta_t * speed;
}

glm::mat4 Player::view() { return glm::lookAt(pos, pos + dir, glm::vec3(0.f, 1.f, 0.f)); }

glm::mat4 Camera::proj(glm::vec2 size) const
{
    auto proj = glm::perspective(fov, size.x / size.y, near, far);
    proj[1][1] *= -1.f;
    return proj;
}