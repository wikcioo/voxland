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

    vec3 result = ambient + diffuse + specular;
    o_color = vec4(result, 1.0);
}
