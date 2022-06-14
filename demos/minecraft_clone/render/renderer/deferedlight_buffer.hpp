#include "../glsl_shared.hpp"

#ifdef LANG_CPP

namespace glsl
{
#endif

#define N_CASCADES 3
#define POISSON_DISC_SIZE 0

struct SceneBuffer
{
    mat4 proj_view;
    mat4 inv_proj_view;
    mat4 shadow_proj_view[N_CASCADES];
    vec4 sun_light;
    vec4 sun_light_dir;
    vec4 near_far;
    vec4 shadow_bias[N_CASCADES]; // minx maxy padding padding
    vec4 cascade_ends;
#if POISSON_DISC_SIZE != 0
    vec2 poisson_disc[POISSON_DISC_SIZE];
#endif
};

#ifdef LANG_CPP

} // namespace glsl

#endif