#version 450

#include "deferedlight_buffer.hpp"

layout(location = 0) in vec2 screen_pos;

layout(set = 0,binding = 0) uniform sampler2D albedo_spec;
layout(set = 0,binding = 1) uniform sampler2D shadow_tex;

layout(set = 0,binding = 2) uniform _
{
	SceneBuffer lights;
};

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 ambient = vec3(0.2,0.2,0.2);
    vec2 tex_coords = screen_pos * 0.5 + 0.5;

    vec3 albedo = texture(albedo_spec,tex_coords).rgb;
    float shadow = texture(shadow_tex,tex_coords).r;

    out_color = vec4(mix(albedo,albedo * ambient,shadow),1.0);
}