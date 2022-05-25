//glsl version 4.5
#version 450

#include "../glsl_shared.hpp"


layout(set = 0, binding = 0) uniform sampler2D tex0;

layout (location = 0) in vec2  in_tex_cord;
layout (location = 1) in float in_tex_id;
layout (location = 2) in vec3  in_normal;

layout (location = 0) out vec4 out_albedo_spec;
// layout (location = 1) out vec3 out_normal;

const vec2 atlas_size = vec2(16,1);

void main()
{
	vec2 tex_cord = in_tex_cord - floor(in_tex_cord);
	tex_cord.x += in_tex_id;

	tex_cord /= atlas_size;
	vec3 tex_color = texture(tex0,tex_cord).xyz;

	out_albedo_spec = vec4 (tex_color,0.5);
	// out_normal = in_normal;

}
