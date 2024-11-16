#version 330 core

out vec4 o_color;

in vec3 v_position;
in vec3 v_normal;
in vec3 v_color;

struct point_light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform point_light u_light;
uniform vec3 u_camera_pos;
uniform samplerCube u_shadow_map;
uniform float u_far_plane;

float calculate_shadow(vec3 frag_pos)
{
    vec3 frag_to_light = frag_pos - u_light.position;
    float closest_depth = texture(u_shadow_map, frag_to_light).r;
    closest_depth *= u_far_plane;
    float current_depth = length(frag_to_light);
    float bias = 0.05;
    float shadow = current_depth - bias > closest_depth ? 1.0 : 0.0;
    return shadow;
}

void main()
{
    vec3 ambient = u_light.ambient * v_color;

    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(u_light.position - v_position);
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = u_light.diffuse * (diff * v_color);

    vec3 view_dir = normalize(u_camera_pos - v_position);
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    vec3 specular = u_light.specular * (spec * v_color);

    float shadow = calculate_shadow(v_position);

    vec3 result = ambient + ((1.0 - shadow) * (diffuse + specular));
    o_color = vec4(result, 1.0);
}
