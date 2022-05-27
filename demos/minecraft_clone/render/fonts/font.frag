#version 450

layout (location = 0) in vec2 f_uv;
layout (location = 1) in vec4 f_color;

layout (location = 0) out vec4 color;

layout(set = 0, binding = 0) uniform sampler2D tex0;

void main()
{
    // color = vec4(1.0,0.0,0.0,1.0);

    float pixel = texture(tex0,f_uv).x;
    if(pixel == 0) discard;

    color = mix(f_color,vec4(0),pixel);
    
}