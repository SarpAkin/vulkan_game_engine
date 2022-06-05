#version 450

#include "../glsl_shared.hpp"

layout(location = 0) in vec3 pos;
layout(location = 1) in uint rgba_color;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform P{
    mat4 proj_view;
};

void main()
{
    out_color = vec4(
        float((rgba_color >> 24) & 0xff) / 255.0,
        float((rgba_color >> 16) & 0xff) / 255.0,
        float((rgba_color >>  8) & 0xff) / 255.0,
        float((rgba_color >>  0) & 0xff) / 255.0
    );

    gl_Position = proj_view * vec4(pos,1.0);
}