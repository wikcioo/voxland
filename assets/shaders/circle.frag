#version 330 core

layout(location = 0) out vec4 o_color;

in vec2 v_local_position;
in vec4 v_color;
in float v_thickness;
in float v_fade;

void main()
{
    float distance = 1.0 - length(v_local_position);
    float circle = smoothstep(0.0, v_fade, distance);
    circle *= smoothstep(v_thickness + v_fade, v_thickness, distance);

    if (circle == 0.0) {
        discard;
    }

    o_color = v_color;
    o_color.a *= circle;
}
