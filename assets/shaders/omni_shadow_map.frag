#version 330 core

in vec4 o_frag_pos;

uniform vec3 u_light_pos;
uniform float u_far_plane;

void main()
{
    float light_distance = length(o_frag_pos.xyz - u_light_pos) / u_far_plane;
    gl_FragDepth = light_distance;
}
