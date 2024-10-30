#version 330 core

out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;
in float v_tex_index;

uniform sampler2D u_textures[4];

void main()
{
    int index = int(v_tex_index);
    o_color = vec4(1.0, 1.0, 1.0, texture(u_textures[index], v_tex_coord).r) * v_color;
}
