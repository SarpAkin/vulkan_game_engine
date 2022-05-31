#include "../glsl_shared.hpp"

#ifdef LANG_CPP

namespace glsl
{
#endif

struct SceneBuffer
{
    mat4 proj_view;
    mat4 inv_proj_view;
    mat4 shadow_proj_view;
    vec4 sun_light;
    vec4 sun_light_dir;
};

#ifdef LANG_CPP

} // namespace glsl

#endif