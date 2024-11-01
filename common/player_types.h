#pragma once

#include <glm/glm.hpp>

#include "defines.h"
#include "uthash/uthash.h"

#define PLAYER_USERNAME_MAX_LEN 32
#define PLAYER_PASSWORD_MAX_LEN 32

typedef u32 player_id;

typedef struct {
#if defined(SERVER)
    i32 socket;
#endif
    player_id id; // The id is generated by the server.
    char username[PLAYER_USERNAME_MAX_LEN + 1];
    glm::vec3 color;
    glm::vec3 position;
    UT_hash_handle hh;
} Player;
