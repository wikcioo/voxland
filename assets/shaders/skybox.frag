#version 330 core

out vec4 o_color;

in vec3 v_tex_coord;

uniform samplerCube u_skybox;

void main()
{
    o_color = texture(u_skybox, v_tex_coord);
}
