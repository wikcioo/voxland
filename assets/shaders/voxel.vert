#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in mat4 in_inst_model;
layout(location = 6) in vec3 in_inst_color;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_color;

uniform mat4 u_projection;
uniform mat4 u_view;

void main()
{
    v_position = vec3(in_inst_model * vec4(in_position, 1.0));
    v_normal = mat3(transpose(inverse(in_inst_model))) * in_normal;
    v_color = in_inst_color;
    gl_Position = u_projection * u_view * in_inst_model * vec4(in_position, 1.0);
}
