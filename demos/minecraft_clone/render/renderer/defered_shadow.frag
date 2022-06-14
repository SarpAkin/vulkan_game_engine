#version 460

#include "deferedlight_buffer.hpp"


layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput f_normal;
layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput f_depth;

layout(std140,set = 0,binding = 2) uniform _
{
	SceneBuffer scene;
};

layout(set = 0,binding = 3) uniform sampler2DArray shadow_texs;

layout(location = 0) in vec2 screen_pos;

layout (location = 0) out float out_color;


float random(vec4 seed4)
{
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

uint calc_cascade_layer(float ldepth){
	// return 1;

	for(uint i = 0;i < N_CASCADES;++i)
	{
		if(ldepth < scene.cascade_ends[i]) return i;
	}

	return 0;
}

float get_shadow_value(vec4 world_pos,vec3 normal,float ldepth)
{	
	uint layer = calc_cascade_layer(ldepth);

	// return layer == 0 ? 0.0 : 1.0;


	vec4 shadow_pos4 =  scene.shadow_proj_view[layer] * world_pos;

	vec3 shadow_pos = shadow_pos4.xyz / shadow_pos4.w;

	shadow_pos.xy = shadow_pos.xy * 0.5 + 0.5;

	float current_depth = shadow_pos.z;

	float bias = max( scene.shadow_bias[layer].x, scene.shadow_bias[layer].y * (1.0 - dot(normal,  scene.sun_light_dir.xyz)));  
 

#if POISSON_DISC_SIZE == 0
	return current_depth - bias  > texture(shadow_texs,vec3(shadow_pos.xy,layer)).r ? 1.0 : 0.0;
#else

	float shadow = 0.0;

	const int iterations = 8;

	float mutator = 1.f;

	for(int i =0;i < iterations;++i)
	{
		float rand = random(vec4(mod(world_pos.xy * 117.22 * mutator,88.5).xy,gl_FragCoord.xy * mutator));
		mutator = rand;
		int index = int(rand * POISSON_DISC_SIZE) % POISSON_DISC_SIZE;

		float pcf_depth = texture(shadow_texs,vec3(shadow_pos.xy +  scene.poisson_disc[index].xy,layer)).r;
		shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0; 
	}

	return shadow / float(iterations);
#endif
}



float linearize_depth(float d,float z_near,float z_far)
{
    return z_near * z_far / (z_far + d * (z_near - z_far));
}

void main()
{
	const float depth = subpassLoad(f_depth).r;
	float ldepth = linearize_depth(depth, scene.near_far.x, scene.near_far.y);

    if(depth >= 1.0)
	{
		out_color = 0.0;
		return;
	}

	vec3 normal = subpassLoad(f_normal).rgb;

	vec4 world_pos4 =  scene.inv_proj_view * vec4( screen_pos.xy ,depth,1.0);

	out_color = get_shadow_value(world_pos4,normal,ldepth);
}