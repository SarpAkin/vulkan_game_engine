#version 460
// #include "../glsl_shared.hpp"

layout(location = 0) in uint vertex_data;

//[variant[SHADOW_PASS]]
#ifndef SHADOW_PASS
layout (location = 0) out vec2  out_tex_pos;
layout (location = 1) out float out_tex_id;
layout (location = 2) out vec3  out_normal;

#endif

layout (push_constant) uniform PushConstants
{
    mat4 proj_view;
    uint cpos_offset;
}push;

layout(std430,set = 1,binding = 0) readonly buffer DrawBuffer
{
    vec4 chunk_poses[];
} draw_buffer;

vec2 tex_cords[] = {vec2(1.0,0.0),vec2(1.0,1.0),vec2(2.0,1.0)};
vec2 atlas_size = vec2(16.0,1.0);


const uint first_comp [3] = {2,0,0};
const uint second_comp[3] = {1,2,1};

const vec3 normal_table[8] = 
{
    vec3(-1.0,-1.0,-1.0), //0b000
    vec3(-1.0,-1.0, 1.0), //0b001
    vec3(-1.0, 1.0,-1.0), //0b010
    vec3(-1.0, 1.0, 1.0), //0b011
    vec3( 1.0,-1.0,-1.0), //0b100
    vec3( 1.0,-1.0, 1.0), //0b101
    vec3( 1.0, 1.0,-1.0), //0b110
    vec3( 1.0, 1.0, 1.0)  //0b111
};

void main()
{

    vec4 position;

    position.x = float((vertex_data >> 10) & 31);  //0b11111
    position.y = float((vertex_data >> 05) & 31);  //0b11111
    position.z = float((vertex_data >> 00) & 31);  //0b11111
    position.w = 1.0;

    uint corner = (vertex_data >> 15) & 7; // 0b111

    position.x += float((corner >> 2) & 1);
    position.y += float((corner >> 1) & 1);
    position.z += float((corner >> 0) & 1);


#ifndef SHADOW_PASS

    uint plane_bits = (vertex_data >> 18) & 3; //0b11

    vec3 normal = vec3(0.0,0.0,0.0);
    normal[plane_bits] =  normal_table[corner][plane_bits];

    out_normal = normal;

    out_tex_pos = vec2(position[first_comp[plane_bits]],position[second_comp[plane_bits]]);
    out_tex_id = float((vertex_data >> 20) & 31);

#endif


    //a block face ranges in -0.5 to 0.5
    position += vec4(-0.5,-0.5,-0.5,0.0);

    position.xyz += draw_buffer.chunk_poses[push.cpos_offset + gl_DrawID].xyz;

    gl_Position = push.proj_view * position;
    // gl_Position.y = -gl_Position.y;


// #ifndef SHADOW_PASS
//     gl_Position = scene.proj_view * position;
//     vec4 shadow_pos = scene.shadow_proj_view * position;
//     out_world_pos = position.xyz;
//     out_shadow_pos = (shadow_pos.xyz / shadow_pos.w);
// #else
//     gl_Position = scene.shadow_proj_view * position;
// #endif
}