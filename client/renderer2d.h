#pragma once

#include <glm/glm.hpp>

#include "shader.h"
#include "texture.h"
#include "common/defines.h"

#define RENDERER_MAX_QUAD_COUNT     10000
#define RENDERER_MAX_VERTEX_COUNT   (RENDERER_MAX_QUAD_COUNT * 4)
#define RENDERER_MAX_INDEX_COUNT    (RENDERER_MAX_QUAD_COUNT * 6)
#define RENDERER_MAX_TEXTURE_COUNT  32

#define QUAD_VERTEX_COUNT 4

typedef enum {
    FA16,
    FA32,
    FA64,
    FA128,
    FA_COUNT
} Font_Atlas_Size;

typedef struct {
    u32 quad_count;
    u32 char_count;
    u32 draw_calls;
} Renderer2D_Stats;

typedef struct {
    glm::vec2 size;
    glm::vec2 bearing;
    glm::vec2 advance;
    f32 xoffset;
} Glyph_Data;

typedef struct {
    u32 texture;
    u32 width;
    u32 height;
    u32 bearing_y;
    Glyph_Data glyphs[128];
} Font_Atlas;

typedef struct {
    glm::vec4 position;
    glm::vec4 color;
    glm::vec2 tex_coords;
    f32 tex_index;
} Quad_Vertex;

typedef struct {
    glm::vec2 world_position;
    glm::vec2 local_position;
    glm::vec4 color;
    f32 thickness;
    f32 fade;
} Circle_Vertex;

typedef struct {
    glm::vec2 position;
    glm::vec4 color;
} Line_Vertex;

typedef struct {
    glm::vec4 tex_coords;
    glm::vec4 color;
    f32 tex_index;
} Text_Vertex;

typedef struct {
    // quad data
    u32 quad_va;
    u32 quad_vb;
    u32 quad_ib;
    u32 quad_index_count;
    u32 white_texture_slot;
    Texture white_texture;
    Quad_Vertex *quad_vertex_buffer_base;
    Quad_Vertex *quad_vertex_buffer_ptr;
    glm::vec4 default_quad_vertex_positions[4];
    glm::vec2 default_quad_vertex_tex_coords[4];
    u32 texture_slots[RENDERER_MAX_TEXTURE_COUNT];
    u32 texture_slot_index;
    Shader quad_shader;

    // circle data
    u32 circle_va;
    u32 circle_vb;
    u32 circle_ib;
    u32 circle_index_count;
    Circle_Vertex *circle_vertex_buffer_base;
    Circle_Vertex *circle_vertex_buffer_ptr;
    Shader circle_shader;

    // line data
    u32 line_va;
    u32 line_vb;
    u32 line_vertex_count;
    Line_Vertex *line_vertex_buffer_base;
    Line_Vertex *line_vertex_buffer_ptr;
    Shader line_shader;

    // text data
    u32 text_va;
    u32 text_vb;
    u32 text_ib;
    u32 text_index_count;
    Font_Atlas font_atlases[FA_COUNT];
    Text_Vertex *text_vertex_buffer_base;
    Text_Vertex *text_vertex_buffer_ptr;
    Shader text_shader;

    Renderer2D_Stats stats;
} Renderer2D;

Renderer2D *renderer2d_create(void);
void renderer2d_destroy(Renderer2D *renderer2d);

void renderer2d_begin_scene(Renderer2D *renderer2d, const glm::mat4 *projection);
void renderer2d_end_scene(Renderer2D *renderer2d);

void renderer2d_reset_stats(Renderer2D *renderer2d);
void renderer2d_clear_screen(Renderer2D *renderer2d, glm::vec4 color);

void renderer2d_draw_quad(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, glm::vec3 color, f32 alpha = 1.0f);
void renderer2d_draw_quad(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, f32 rot, glm::vec3 color, f32 alpha = 1.0f);

void renderer2d_draw_rect(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, glm::vec3 color, f32 alpha = 1.0f);

void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, glm::vec3 color, f32 alpha = 1.0f);
void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, f32 thickness, glm::vec3 color, f32 alpha = 1.0f);
void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, f32 thickness, f32 fade, glm::vec3 color, f32 alpha = 1.0f);

void renderer2d_draw_line(Renderer2D *renderer2d, glm::vec2 p1, glm::vec2 p2, glm::vec3 color, f32 alpha = 1.0f);
void renderer2d_set_line_width(Renderer2D *renderer2d, f32 width);

void renderer2d_draw_text(Renderer2D *renderer2d, const char *text, Font_Atlas_Size fa_size, glm::vec2 position, glm::vec3 color, f32 alpha = 1.0f);

u32 renderer2d_get_font_bearing_y(Renderer2D *renderer2d, Font_Atlas_Size fa);
u32 renderer2d_get_font_height(Renderer2D *renderer2d, Font_Atlas_Size fa);
u32 renderer2d_get_font_width(Renderer2D *renderer2d, Font_Atlas_Size fa);
