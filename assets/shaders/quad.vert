#version 330 core

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coords;
layout(location = 3) in float in_tex_index;

out vec4 v_color;
out vec2 v_tex_coords;
out float v_tex_index;

uniform mat4 u_projection;

void main()
{
    v_color = in_color;
    v_tex_coords = in_tex_coords;
    v_tex_index = in_tex_index;
    gl_Position = u_projection * vec4(in_position);
}
