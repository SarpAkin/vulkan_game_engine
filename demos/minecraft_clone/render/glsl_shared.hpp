#ifndef BOUND_CHECKS_HPP
#define BOUND_CHECKS_HPP

#ifdef LANG_CPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <glm/mat4x4.hpp>

namespace glsl
{

using mat4 = glm::mat4x4;
using uint = uint32_t;
using namespace glm;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

#define INLINE inline

#else

#define INLINE

#endif

struct SceneBuffer
{
    mat4 proj_view;
    mat4 shadow_proj_view;
    vec4 sun_light;
    vec3 sun_light_dir;
    float shodow_plane_far;
    float shadow_bias_max;
    float shadow_bias_min;
};

struct AABB
{
    vec3 min, max;
};

struct Plane
{
    vec3 dir;
    float distance;
    // vec4 pos;
};

struct Frustrum
{
    Plane planes[6];
};

//https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
//https://old.cescg.org/CESCG-2002/DSykoraJJelinek/index.html
//https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
INLINE bool frustrum_vs_aabb(Frustrum f, AABB b)
{
    bool ret = true;

    vec3 b_cen = (b.min + b.max) * 0.5f;
    vec3 b_ext = b.max - b_cen;

    for (int i = 0; i < 6; ++i)
    {
        Plane p = f.planes[i];

        float m = dot(p.dir, b_cen) - p.distance;
        float r = dot(b_ext, abs(p.dir));

        ret = ret && bool(-r <= m);
    }

    return ret;
}

INLINE Plane plane_from_points(vec3 a, vec3 b, vec3 c)
{
    vec3 ab = a - b;
    vec3 ac = a - c;

    Plane p;

    p.dir      = normalize(cross(ac, ab));
    p.distance = dot(p.dir, b);
    // p.pos = vec4((a + b + c) / 3.f,1.f);

    return p;
}

struct FrustrumBoundry
{
    vec3 boundries[2][2][2];
};

INLINE FrustrumBoundry frustrum_boundry_from_projection(mat4 inv_proj)
{
    FrustrumBoundry b;

    for (int x = 0; x < 2; ++x)
    {
        float xf = x;
        xf -= 0.5f;
        xf *= 2.f;

        for (int y = 0; y < 2; ++y)
        {
            float yf = y;
            yf -= 0.5f;
            yf *= 2.f;

            for (int z = 0; z < 2; ++z)
            {
                vec4 res             = inv_proj * vec4(xf, yf, float(z), 1.f);
                b.boundries[x][y][z] = vec3(res.x / res.w, res.y / res.w, res.z / res.w);
            }
        }
    }

    return b;
}

INLINE Frustrum frustrum_from_projection(mat4 inv_proj)
{
    Frustrum f;

    vec3 boundries[2][2][2];

    for (int x = 0; x < 2; ++x)
    {
        float xf = x;
        xf -= 0.5f;
        xf *= 2.f;

        for (int y = 0; y < 2; ++y)
        {
            float yf = y;
            yf -= 0.5f;
            yf *= 2.f;

            for (int z = 0; z < 2; ++z)
            {
                vec4 res           = inv_proj * vec4(xf, yf, float(z), 1);
                boundries[x][y][z] = vec3(res.x / res.w, res.y / res.w, res.z / res.w);
            }
        }
    }

    f.planes[0] = plane_from_points(boundries[0][0][0], boundries[0][1][0], boundries[1][1][0]);
    f.planes[1] = plane_from_points(boundries[0][0][1], boundries[1][1][1], boundries[0][1][1]);

    f.planes[2] = plane_from_points(boundries[0][1][0], boundries[0][1][1], boundries[1][1][0]);
    f.planes[3] = plane_from_points(boundries[0][0][0], boundries[1][0][0], boundries[0][0][1]);

    f.planes[4] = plane_from_points(boundries[1][0][1], boundries[1][0][0], boundries[1][1][0]);
    f.planes[5] = plane_from_points(boundries[0][1][0], boundries[0][0][0], boundries[0][0][1]);

    return f;
}

#ifdef LANG_CPP
}

#endif

#undef INLINE
#endif
