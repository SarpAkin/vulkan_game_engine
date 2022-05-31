#version 460

#include "deferedlight_buffer.hpp"

layout(set = 0,binding = 3) uniform _
{
	SceneBuffer lights;
};

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput f_albedo_spec;
layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput f_normal;
layout (input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput f_depth;

// layout(set = 1,binding = 0) uniform sampler2D[] shadow_texs;

layout(location = 0) in vec2 screen_pos;

layout (location = 0) out vec4 out_color;


float random(vec4 seed4)
{
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

// float get_shadow_value(vec4 world_pos,vec3 normal)
// {	
// 	vec4 shadow_pos4 = lights.shadow_proj_view[0] * world_pos;

// 	vec3 shadow_pos = shadow_pos4.xyz / shadow_pos4.w;

// 	shadow_pos.xy = shadow_pos.xy * 0.5 + 0.5;

// 	float current_depth = shadow_pos.z;

// 	float bias = max(lights.shadow_bias[0].max * (1.0 - dot(normal, lights.sun_light_dir.xyz)), lights.shadow_bias[0].min);  
// 	bias = lights.shadow_bias[0].min;

// 	vec2 texel_size = 1.0 / textureSize(shadow_texs[0], 0);
// 	float shadow = 0.0;


// 	const int iterations = 8;

// 	for(int i =0;i < iterations;++i)
// 	{
// 		int index = int(random(vec4(mod(world_pos.xy * 117.22,88.5),gl_FragCoord.y,i)) * 16.0) % 16;
// 		// index = i;

// 		float pcf_depth = texture(shadow_texs[0],shadow_pos.xy + lights.poisson_disk[index].xy).r;
// 		shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0; 
// 	}

// 	return shadow / float(iterations);
// }



float linearize_depth(float d,float z_near,float z_far)
{
    return z_near * z_far / (z_far + d * (z_near - z_far));
}

float proj_near = 0.1;
float proj_far = 2000.0;

void main()
{
	const float depth = subpassLoad(f_depth).r;

    if(depth >= 1.0)
	{
		out_color = vec4(0.0,0.0,0.0,1.0);
		return;
	}


	vec3 normal = subpassLoad(f_normal).rgb;

	vec4 albedo_spec = subpassLoad(f_albedo_spec).rgba;

    vec3 ambient = vec3(0.2,0.2,0.2);


	// out_color = vec4(vec2(gl_FragCoord.xy) / screen_size - 0.5,0.0,1.0);
	// out_color = vec4(screen_pos.xy,0.0,1.0);
	// out_color = vec4(vec3(1.0 - depth),1.0);
	// return;

	vec4 world_pos4 = lights.inv_proj_view * vec4( screen_pos.xy ,depth,1.0);

	// vec4 shadow_pos4 = lights.shadow_proj_view[0] * world_pos4;
	// vec3 shadow_pos = shadow_pos4.xyz / shadow_pos4.w;
	// shadow_pos.xy = shadow_pos.xy * 0.5 + 0.5;
	// out_color = vec4(vec3(shadow_pos.xyz),1.0);

	// out_color = vec4(world_pos4.xyz / (world_pos4.w * 200) + 0.5,1.0);
	// return;


	// out_color = vec4(normal.xyz * 2 + 0.5,1.0f);
	out_color = vec4(albedo_spec.xyz,1.0);
}