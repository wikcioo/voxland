#pragma once

typedef u32 entity_id;

typedef struct {
    entity_id id;
    f32 radius;
    f32 angular_velocity;
    glm::vec3 initial_position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
} Light;
