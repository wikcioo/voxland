#version 330 core

layout(location = 0) in vec4 in_coord;
layout(location = 1) in vec4 in_color;
layout(location = 2) in float in_tex_index;

out vec4 v_color;
out vec2 v_tex_coord;
out float v_tex_index;

uniform mat4 u_projection;

void main()
{
    v_color = in_color;
    v_tex_coord = vec2(in_coord.z, in_coord.w);
    v_tex_index = in_tex_index;
    gl_Position = u_projection * vec4(in_coord.xy, 0.0, 1.0);
}
