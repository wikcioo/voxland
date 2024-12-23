#version 330 core

out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coord;
in float v_tex_index;

uniform sampler2D u_textures[4];

void main()
{
    float alpha;
    int index = int(v_tex_index);

    switch (index)
    {
        case 0:  alpha = texture(u_textures[0], v_tex_coord).r; break;
        case 1:  alpha = texture(u_textures[1], v_tex_coord).r; break;
        case 2:  alpha = texture(u_textures[2], v_tex_coord).r; break;
        case 3:  alpha = texture(u_textures[3], v_tex_coord).r; break;
        default: alpha = texture(u_textures[0], v_tex_coord).r; break;
    }

    o_color = vec4(1.0, 1.0, 1.0, alpha) * v_color;
}
