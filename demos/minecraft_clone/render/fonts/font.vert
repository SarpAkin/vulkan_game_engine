#version 450


layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec2 f_uv;
layout (location = 1) out vec4 f_color;

layout (push_constant) uniform PushConstants{
    vec2 pos;
    vec2 scale;
    vec4 color;
}push;

void main()
{
    f_uv = uv;
    f_color = push.color;

    gl_Position = vec4(push.pos + pos * push.scale,0,1);
}