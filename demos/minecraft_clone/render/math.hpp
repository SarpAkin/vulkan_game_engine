#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

class Camera;

glm::mat4 calc_shadow_mat(const glm::mat4& proj, const glm::mat4& view, glm::vec3 sundir, float& shadow_far, float rotation,uint32_t* score = nullptr);

glm::mat4 calc_shadow_mat(const glm::mat4 &proj, const glm::mat4 &view, glm::vec3 sundir, float &shadow_far);

std::vector<std::tuple<glm::mat4, float,float>> calc_cascaded_shadows(float fovy, float aspect, float z_near, float z_far,
    const glm::mat4& view, glm::vec3 sundir, const std::vector<float>& cascade_sizes);

std::vector<std::tuple<glm::mat4, float,float>> calc_cascaded_shadows(const Camera& c, glm::vec2 aspects,
    const glm::mat4& view, glm::vec3 sundir, const std::vector<float>& cascade_sizes);