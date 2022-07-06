//glsl version 4.5
#version 450

#include "../glsl_shared.hpp"


layout(set = 0, binding = 0) uniform sampler2D tex0;

layout (location = 0) in vec2  in_tex_cord;
layout (location = 1) in float in_tex_id;
layout (location = 2) in vec3  in_normal;

layout (location = 0) out vec4 out_albedo_spec;
layout (location = 1) out vec3 out_normal;

const vec2 atlas_size = vec2(16,1);

layout (push_constant) uniform PushConstants
{
    mat4 proj_view;
    uint cpos_offset;
    uint color;
}push;

void main()
{
	vec2 tex_cord = in_tex_cord - floor(in_tex_cord);
	tex_cord.x += in_tex_id;

	if(true)
	{
		tex_cord /= atlas_size;
		vec3 tex_color = texture(tex0,tex_cord).xyz;
		out_albedo_spec = vec4 (tex_color,0.5);
	}
	else{
		out_albedo_spec = vec4(
			float((push.color >> 24) & 0xFF) / 255.0,
			float((push.color >> 16) & 0xFF) / 255.0,
			float((push.color >>  8) & 0xFF) / 255.0,
			float((push.color >>  0) & 0xFF) / 255.0
		);
	}


	out_normal = in_normal;

}
