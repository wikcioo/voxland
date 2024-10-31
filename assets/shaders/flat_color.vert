#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

out vec3 v_position;
out vec3 v_normal;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

void main()
{
    v_position = vec3(u_model * vec4(in_position, 1.0));
    v_normal = mat3(transpose(inverse(u_model))) * in_normal;
    gl_Position = u_projection * u_view * u_model * vec4(in_position, 1.0);
}
