#version 330 core

layout(location = 0) out vec4 o_color;

in vec4 v_color;
in vec2 v_tex_coords;
in float v_tex_index;

uniform sampler2D u_textures[32];

void main()
{
    vec4 color;
    int index = int(v_tex_index);

    switch (index)
    {
        case 0:  color = texture(u_textures[0],  v_tex_coords); break;
        case 1:  color = texture(u_textures[1],  v_tex_coords); break;
        case 2:  color = texture(u_textures[2],  v_tex_coords); break;
        case 3:  color = texture(u_textures[3],  v_tex_coords); break;
        case 4:  color = texture(u_textures[4],  v_tex_coords); break;
        case 5:  color = texture(u_textures[5],  v_tex_coords); break;
        case 6:  color = texture(u_textures[6],  v_tex_coords); break;
        case 7:  color = texture(u_textures[7],  v_tex_coords); break;
        case 8:  color = texture(u_textures[8],  v_tex_coords); break;
        case 9:  color = texture(u_textures[9],  v_tex_coords); break;
        case 10: color = texture(u_textures[10], v_tex_coords); break;
        case 11: color = texture(u_textures[11], v_tex_coords); break;
        case 12: color = texture(u_textures[12], v_tex_coords); break;
        case 13: color = texture(u_textures[13], v_tex_coords); break;
        case 14: color = texture(u_textures[14], v_tex_coords); break;
        case 15: color = texture(u_textures[15], v_tex_coords); break;
        default: color = texture(u_textures[0],  v_tex_coords); break;
    }

    o_color = color * v_color;
}
