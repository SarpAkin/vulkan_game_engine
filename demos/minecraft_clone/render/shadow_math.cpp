#include "math.hpp"

#include <vector>

#include <glm/gtx/transform.hpp>

#include <vke/util.hpp>

#include "../game/player.hpp"
#include "glsl_shared.hpp"

auto fix_recursive_lambda(auto&& lambda)
{
    return [lambda](auto&&... args) {
        return lambda(lambda, args...);
    };
}

glm::mat4 calc_shadow_mat(const glm::mat4& proj, const glm::mat4& view, glm::vec3 sundir, float& shadow_far, float rotation, uint32_t* score)
{
    glm::mat4x4 proj_view = proj * view;
    glm::mat4x4 inv_pv    = glm::inverse(proj_view);

    glm::vec3 pv_corners[8] = {
        {01.f, 01.f, 01.f},
        {01.f, -1.f, 01.f},
        {-1.f, -1.f, 01.f},
        {-1.f, 01.f, 01.f},
        {01.f, 01.f, 00.f},
        {01.f, -1.f, 00.f},
        {-1.f, -1.f, 00.f},
        {-1.f, 01.f, 00.f},
    };

    glm::vec3 up_dir = {0.f, 1.f, 0.f};

    if (rotation != 0.f)
        up_dir = glm::rotate(rotation, sundir) * glm::vec4(0.f, 1.0, 0.f, 0.f);

    glm::mat4 shadow_view = glm::lookAt(glm::vec3(0.f, 0.f, 0.f), sundir, up_dir);

    float big_n      = 1'000'000.f;
    glm::vec3 big_v3 = glm::vec3(big_n, big_n, big_n);

    glsl::AABB object_boundry, frustrum_boundry;
    object_boundry = frustrum_boundry = {big_v3, -big_v3};

    for (int i = 0; i < 8; ++i)
    {
        // to world pos than to shadow view
        glm::vec4 corner4 = (shadow_view * inv_pv) * glm::vec4(pv_corners[i], 1.f);

        glm::vec3 corner = glm::vec3(corner4) / corner4.w;

        frustrum_boundry.min = glm::min(corner, frustrum_boundry.min);
        frustrum_boundry.max = glm::max(corner, frustrum_boundry.max);
    }

    // frustrum_boundry.min.z -= 2000.f;

    glm::vec3 extends = (frustrum_boundry.max - frustrum_boundry.min) * .5f;
    glm::vec3 mid     = (frustrum_boundry.max + frustrum_boundry.min) * .5f;

    mid.z = frustrum_boundry.max.z;

    glm::vec4 mid4 = glm::inverse(shadow_view) * glm::vec4(mid, 1.f);

    mid = glm::vec3(mid4) / mid4.w;

    if (score)
    {
        *score = extends.x * extends.y;
    }

    float z_extra = 200.f;

    shadow_far = extends.z * 2.f + z_extra;

    return glm::ortho(-extends.x, extends.x, extends.y, -extends.y, 0.f, shadow_far) *
           glm::lookAt(mid - glm::normalize(sundir) * z_extra, mid + sundir, up_dir);
}

struct ShadowMatResult
{
    glm::mat4 shadow;
    float shadoow_far;
    float rotation;
    uint32_t score;
};

ShadowMatResult best_rot_finder(const glm::mat4& proj, const glm::mat4& view, glm::vec3 sundir, ShadowMatResult& a, ShadowMatResult& b, int recursion_limit)
{
    ShadowMatResult c;
    c.rotation = (a.rotation + b.rotation) * 0.5f;
    c.shadow   = calc_shadow_mat(proj, view, sundir, c.shadoow_far, c.rotation, &c.score);

    if (recursion_limit == 0) return c;

    return best_rot_finder(proj, view, sundir, a.score > b.score ? a : b, c, recursion_limit - 1);
}

glm::mat4 calc_shadow_mat(const glm::mat4& proj, const glm::mat4& view, glm::vec3 sundir, float& shadow_far)
{
    ShadowMatResult a;
    a.rotation = glm::radians(0.f);
    a.shadow   = calc_shadow_mat(proj, view, sundir, a.shadoow_far, a.rotation, &a.score);

    ShadowMatResult b;
    b.rotation = glm::radians(180.f);
    b.shadow   = calc_shadow_mat(proj, view, sundir, b.shadoow_far, b.rotation, &b.score);

    auto res = best_rot_finder(proj, view, sundir, a, b, 10);

    shadow_far = res.shadoow_far;
    return res.shadow;
}

std::vector<std::tuple<glm::mat4, float,float>> calc_cascaded_shadows(float fovy, float aspect, float z_near, float z_far,
    const glm::mat4& view, glm::vec3 sundir, const std::vector<float>& cascade_sizes)
{
    float covered_z = z_near;

    return map_vec(cascade_sizes, [&](float cascade_size) {
        float shadow_far;
        glm::mat4 shadow = calc_shadow_mat(glm::perspective(fovy, aspect, covered_z, covered_z + cascade_size), view, sundir, shadow_far);
        covered_z += cascade_size;
        return std::make_tuple(shadow, shadow_far,covered_z);
    });

}

std::vector<std::tuple<glm::mat4, float,float>> calc_cascaded_shadows(const Camera& c, glm::vec2 aspects,
    const glm::mat4& view, glm::vec3 sundir, const std::vector<float>& cascade_sizes)
{
    return calc_cascaded_shadows(c.fov, aspects.x / aspects.y, c.near, c.far, view, sundir, cascade_sizes);
}