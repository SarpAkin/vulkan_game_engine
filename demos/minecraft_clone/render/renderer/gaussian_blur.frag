#version 460

layout(set = 0,binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 screen_pos;

layout (location = 0) out float out_color;

layout (push_constant) uniform _
{
    uint horizontal;
};

void main()
{   
    vec2 tex_coords = screen_pos * 0.5 + 0.5;

    float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    vec2 tex_offset = 1.0 / textureSize(tex, 0); // gets size of single texel
    float result = texture(tex, tex_coords).r * weight[0]; // current fragment's contribution
    if(horizontal == 1)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(tex, tex_coords + vec2(tex_offset.x * i, 0.0)).r * weight[i];
            result += texture(tex, tex_coords - vec2(tex_offset.x * i, 0.0)).r * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(tex, tex_coords + vec2(0.0, tex_offset.y * i)).r * weight[i];
            result += texture(tex, tex_coords - vec2(0.0, tex_offset.y * i)).r * weight[i];
        }
    }
    out_color = result;
}