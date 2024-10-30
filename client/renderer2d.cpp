#include "renderer2d.h"

#include <GL/glew.h>

#include <glm/gtc/matrix_transform.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "common/log.h"
#include "common/asserts.h"

#define FONT_FILEPATH "assets/fonts/IosevkaNerdFontMono-Regular.ttf"

LOCAL FT_Library ft;
LOCAL FT_Face face;

LOCAL void start_batch(Renderer2D *renderer2d);
LOCAL void next_batch(Renderer2D *renderer2d);
LOCAL void flush(Renderer2D *renderer2d);

LOCAL void create_shaders(Renderer2D *renderer2d);
LOCAL void create_font_atlas(FT_Face ft_face, u32 height, Font_Atlas *out_atlas);

Renderer2D *renderer2d_create(void)
{
    Renderer2D *renderer2d = (Renderer2D *) malloc(sizeof(Renderer2D));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    create_shaders(renderer2d);

    // quad
    renderer2d->quad_vertex_buffer_base = (Quad_Vertex *) malloc(RENDERER_MAX_VERTEX_COUNT * sizeof(Quad_Vertex));

    renderer2d->white_texture_slot = 0;
    renderer2d->texture_slot_index = 1;

    glCreateVertexArrays(1, &renderer2d->quad_va);
    glBindVertexArray(renderer2d->quad_va);

    glCreateBuffers(1, &renderer2d->quad_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer2d->quad_vb);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_MAX_VERTEX_COUNT * sizeof(Quad_Vertex), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (const void *) offsetof(Quad_Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (const void *) offsetof(Quad_Vertex, color));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (const void *) offsetof(Quad_Vertex, tex_coords));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (const void *) offsetof(Quad_Vertex, tex_index));

    u32 indices[RENDERER_MAX_INDEX_COUNT];
    u32 offset = 0;
    for (u32 i = 0; i < RENDERER_MAX_INDEX_COUNT; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;

        offset += 4;
    }

    glCreateBuffers(1, &renderer2d->quad_ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer2d->quad_ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    renderer2d->default_quad_vertex_positions[0] = glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f);
    renderer2d->default_quad_vertex_positions[1] = glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f);
    renderer2d->default_quad_vertex_positions[2] = glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f);
    renderer2d->default_quad_vertex_positions[3] = glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f);

    renderer2d->default_quad_vertex_tex_coords[0] = glm::vec2(0.0f, 0.0f);
    renderer2d->default_quad_vertex_tex_coords[1] = glm::vec2(0.0f, 1.0f);
    renderer2d->default_quad_vertex_tex_coords[2] = glm::vec2(1.0f, 1.0f);
    renderer2d->default_quad_vertex_tex_coords[3] = glm::vec2(1.0f, 0.0f);

    Texture_Spec spec = {
        .width = 1,
        .height = 1,
        .generate_mipmaps = false,
        .format = IMAGE_FORMAT_RGBA8
    };

    u32 white_texture_data = 0xFFFFFFFF;
    texture_create_from_spec(spec, &white_texture_data, &renderer2d->white_texture);

    renderer2d->texture_slots[0] = renderer2d->white_texture.id;
    memset(&renderer2d->texture_slots[1], 0, RENDERER_MAX_TEXTURE_COUNT - 1);

    i32 samplers[RENDERER_MAX_TEXTURE_COUNT];
    for (u32 i = 0; i < RENDERER_MAX_TEXTURE_COUNT; i++) {
        samplers[i] = i;
    }

    shader_bind(&renderer2d->quad_shader);
    shader_set_uniform_int_array(&renderer2d->quad_shader, "u_textures", samplers, 32);

    // circle
    renderer2d->circle_vertex_buffer_base = (Circle_Vertex *) malloc(RENDERER_MAX_VERTEX_COUNT * sizeof(Circle_Vertex));

    glCreateVertexArrays(1, &renderer2d->circle_va);
    glBindVertexArray(renderer2d->circle_va);

    glCreateBuffers(1, &renderer2d->circle_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer2d->circle_vb);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_MAX_VERTEX_COUNT * sizeof(Circle_Vertex), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Circle_Vertex), (const void *) offsetof(Circle_Vertex, world_position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Circle_Vertex), (const void *) offsetof(Circle_Vertex, local_position));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Circle_Vertex), (const void *) offsetof(Circle_Vertex, color));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Circle_Vertex), (const void *) offsetof(Circle_Vertex, thickness));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Circle_Vertex), (const void *) offsetof(Circle_Vertex, fade));

    glCreateBuffers(1, &renderer2d->circle_ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer2d->circle_ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // line
    renderer2d->line_vertex_buffer_base = (Line_Vertex *) malloc(RENDERER_MAX_VERTEX_COUNT * sizeof(Line_Vertex));

    glCreateVertexArrays(1, &renderer2d->line_va);
    glBindVertexArray(renderer2d->line_va);

    glCreateBuffers(1, &renderer2d->line_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer2d->line_vb);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_MAX_VERTEX_COUNT * sizeof(Line_Vertex), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Line_Vertex), (const void *) offsetof(Line_Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Line_Vertex), (const void *) offsetof(Line_Vertex, color));

    renderer2d_set_line_width(renderer2d, 2.0f);

    // text
    renderer2d->text_vertex_buffer_base = (Text_Vertex *) malloc(RENDERER_MAX_VERTEX_COUNT * sizeof(Text_Vertex));

    glCreateVertexArrays(1, &renderer2d->text_va);
    glBindVertexArray(renderer2d->text_va);

    glCreateBuffers(1, &renderer2d->text_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer2d->text_vb);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_MAX_VERTEX_COUNT * sizeof(Text_Vertex), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Text_Vertex), (const void *) offsetof(Text_Vertex, tex_coords));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Text_Vertex), (const void *) offsetof(Text_Vertex, color));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Text_Vertex), (const void *) offsetof(Text_Vertex, tex_index));

    glCreateBuffers(1, &renderer2d->text_ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer2d->text_ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    shader_bind(&renderer2d->text_shader);
    shader_set_uniform_int_array(&renderer2d->text_shader, "u_textures", samplers, FA_COUNT);

    if (FT_Init_FreeType(&ft)) {
        LOG_FATAL("failed to initialize freetype library\n");
    }

    if (FT_New_Face(ft, FONT_FILEPATH, 0, &face)) {
        LOG_ERROR("failed to load font at %s\n", FONT_FILEPATH);
    }

    create_font_atlas(face, 16, &renderer2d->font_atlases[FA16]);
    create_font_atlas(face, 32, &renderer2d->font_atlases[FA32]);
    create_font_atlas(face, 64, &renderer2d->font_atlases[FA64]);
    create_font_atlas(face, 128, &renderer2d->font_atlases[FA128]);

    return renderer2d;
}

void renderer2d_destroy(Renderer2D *renderer2d)
{
    free(renderer2d->quad_vertex_buffer_base);
    free(renderer2d->circle_vertex_buffer_base);
    free(renderer2d->line_vertex_buffer_base);
    free(renderer2d->text_vertex_buffer_base);

    texture_destroy(&renderer2d->white_texture);

    shader_destroy(&renderer2d->quad_shader);
    glDeleteVertexArrays(1, &renderer2d->quad_va);
    glDeleteBuffers(1, &renderer2d->quad_vb);
    glDeleteBuffers(1, &renderer2d->quad_ib);

    shader_destroy(&renderer2d->circle_shader);
    glDeleteVertexArrays(1, &renderer2d->circle_va);
    glDeleteBuffers(1, &renderer2d->circle_vb);
    glDeleteBuffers(1, &renderer2d->circle_ib);

    shader_destroy(&renderer2d->line_shader);
    glDeleteVertexArrays(1, &renderer2d->line_va);
    glDeleteBuffers(1, &renderer2d->line_vb);

    shader_destroy(&renderer2d->text_shader);
    glDeleteVertexArrays(1, &renderer2d->text_va);
    glDeleteBuffers(1, &renderer2d->text_vb);
    glDeleteBuffers(1, &renderer2d->text_ib);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    free(renderer2d);
}

void renderer2d_begin_scene(Renderer2D *renderer2d, const glm::mat4 *projection)
{
    glDisable(GL_DEPTH_TEST);

    // TODO: replace with uniform buffer
    shader_bind(&renderer2d->quad_shader);
    shader_set_uniform_mat4(&renderer2d->quad_shader, "u_projection", projection);

    shader_bind(&renderer2d->circle_shader);
    shader_set_uniform_mat4(&renderer2d->circle_shader, "u_projection", projection);

    shader_bind(&renderer2d->line_shader);
    shader_set_uniform_mat4(&renderer2d->line_shader, "u_projection", projection);

    shader_bind(&renderer2d->text_shader);
    shader_set_uniform_mat4(&renderer2d->text_shader, "u_projection", projection);

    start_batch(renderer2d);
}

void renderer2d_end_scene(Renderer2D *renderer2d)
{
    flush(renderer2d);
    glEnable(GL_DEPTH_TEST);
}

void renderer2d_reset_stats(Renderer2D *renderer2d)
{
    memset(&renderer2d->stats, 0, sizeof(Renderer2D_Stats));
}

void renderer2d_clear_screen(Renderer2D *renderer2d, glm::vec4 color)
{
    UNUSED(renderer2d);
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer2d_draw_quad(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, glm::vec3 color, f32 alpha)
{
    renderer2d_draw_quad(renderer2d, position, size, 0.0f, color, alpha);
}

void renderer2d_draw_quad(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, f32 rot, glm::vec3 color, f32 alpha)
{
    if (renderer2d->quad_index_count >= RENDERER_MAX_INDEX_COUNT) {
        next_batch(renderer2d);
    }

    f32 texture_index = 0.0f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(rot), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));

    for (i32 i = 0; i < QUAD_VERTEX_COUNT; i++) {
        renderer2d->quad_vertex_buffer_ptr->position = model * renderer2d->default_quad_vertex_positions[i];
        renderer2d->quad_vertex_buffer_ptr->color = glm::vec4(color.r, color.g, color.b, alpha);
        renderer2d->quad_vertex_buffer_ptr->tex_coords = renderer2d->default_quad_vertex_tex_coords[i];
        renderer2d->quad_vertex_buffer_ptr->tex_index = texture_index;
        renderer2d->quad_vertex_buffer_ptr++;
    }

    renderer2d->quad_index_count += 6;
    renderer2d->stats.quad_count++;
}

void renderer2d_draw_rect(Renderer2D *renderer2d, glm::vec2 position, glm::vec2 size, glm::vec3 color, f32 alpha)
{
    glm::vec2 p0 = glm::vec2(position.x - size.x * 0.5f, position.y + size.y * 0.5f);
    glm::vec2 p1 = glm::vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
    glm::vec2 p2 = glm::vec2(position.x + size.x * 0.5f, position.y - size.y * 0.5f);
    glm::vec2 p3 = glm::vec2(position.x - size.x * 0.5f, position.y - size.y * 0.5f);

    renderer2d_draw_line(renderer2d, p0, p1, color, alpha);
    renderer2d_draw_line(renderer2d, p1, p2, color, alpha);
    renderer2d_draw_line(renderer2d, p2, p3, color, alpha);
    renderer2d_draw_line(renderer2d, p3, p0, color, alpha);
}

void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, glm::vec3 color, f32 alpha)
{
    renderer2d_draw_circle(renderer2d, position, radius, 1.0f, 0.03f, color, alpha);
}

void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, f32 thickness, glm::vec3 color, f32 alpha)
{
    renderer2d_draw_circle(renderer2d, position, radius, thickness, 0.03f, color, alpha);
}

void renderer2d_draw_circle(Renderer2D *renderer2d, glm::vec2 position, f32 radius, f32 thickness, f32 fade, glm::vec3 color, f32 alpha)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f, radius * 2.0f, 1.0f));

    for (i32 i = 0; i < QUAD_VERTEX_COUNT; i++) {
        renderer2d->circle_vertex_buffer_ptr->world_position = glm::vec2(model * renderer2d->default_quad_vertex_positions[i]);
        renderer2d->circle_vertex_buffer_ptr->local_position = glm::vec2(renderer2d->default_quad_vertex_positions[i] * 2.0f);
        renderer2d->circle_vertex_buffer_ptr->color = glm::vec4(color, alpha);
        renderer2d->circle_vertex_buffer_ptr->thickness = thickness;
        renderer2d->circle_vertex_buffer_ptr->fade = fade;
        renderer2d->circle_vertex_buffer_ptr++;
    }

    renderer2d->circle_index_count += 6;
    renderer2d->stats.quad_count++;
}

void renderer2d_draw_line(Renderer2D *renderer2d, glm::vec2 p1, glm::vec2 p2, glm::vec3 color, f32 alpha)
{
    glm::vec4 c = glm::vec4(color, alpha);

    renderer2d->line_vertex_buffer_ptr->position = p1;
    renderer2d->line_vertex_buffer_ptr->color = c;
    renderer2d->line_vertex_buffer_ptr++;

    renderer2d->line_vertex_buffer_ptr->position = p2;
    renderer2d->line_vertex_buffer_ptr->color = c;
    renderer2d->line_vertex_buffer_ptr++;

    renderer2d->line_vertex_count += 2;
}

void renderer2d_set_line_width(Renderer2D *renderer2d, f32 width)
{
    UNUSED(renderer2d);
    glLineWidth(width);
}

void renderer2d_draw_text(Renderer2D *renderer2d, const char *text, Font_Atlas_Size fa_size, glm::vec2 position, glm::vec3 color, f32 alpha)
{
    ASSERT(text);
    ASSERT(fa_size < FA_COUNT);

    f32 start_x = position.x;

    PERSIST f32 scale = 1.0f;
    Font_Atlas *fa = &renderer2d->font_atlases[fa_size];
    for (const char *c = text; *c; c++) {
        Glyph_Data g = fa->glyphs[(i32)*c];

        if (*c == ' ') {
            position.x += g.advance.x * scale;
        } else if (*c == '\n') {
            position.x = start_x;
            position.y -= (f32) fa->height * scale;
        } else if (*c == '\t') {
            // Tab is 4 spaces
            position.x += 4 * fa->glyphs[32].advance.x * scale;
        } else {
            f32 x =  position.x + g.bearing.x * scale;
            f32 y = -position.y - g.bearing.y * scale;
            f32 w = g.size.x * scale;
            f32 h = g.size.y * scale;

            if (!w || !h) {
                continue;
            }

            f32 ox = g.xoffset;
            f32 sx = g.size.x;
            f32 sy = g.size.y;

            glm::vec4 text_vertex_positions[4] = {
                glm::vec4(x, -y, ox, 0),
                glm::vec4(x, -y-h, ox, sy / (f32) fa->height),
                glm::vec4(x+w, -y-h, ox+sx / (f32) fa->width, sy / (f32) fa->height),
                glm::vec4(x+w, -y, ox+sx / (f32) fa->width, 0)
            };

            for (i32 i = 0; i < QUAD_VERTEX_COUNT; i++) {
                renderer2d->text_vertex_buffer_ptr->tex_coords = text_vertex_positions[i];
                renderer2d->text_vertex_buffer_ptr->color = glm::vec4(color, alpha),
                renderer2d->text_vertex_buffer_ptr->tex_index = (f32) fa_size;
                renderer2d->text_vertex_buffer_ptr++;
            }

            position.x += g.advance.x * scale;
            position.y += g.advance.y * scale;

            renderer2d->text_index_count += 6;
            renderer2d->stats.quad_count++;
            renderer2d->stats.char_count++;
        }
    }
}

u32 renderer2d_get_font_bearing_y(Renderer2D *renderer2d, Font_Atlas_Size fa)
{
    return renderer2d->font_atlases[fa].bearing_y;
}

u32 renderer2d_get_font_height(Renderer2D *renderer2d, Font_Atlas_Size fa)
{
    return renderer2d->font_atlases[fa].height;
}

u32 renderer2d_get_font_width(Renderer2D *renderer2d, Font_Atlas_Size fa)
{
    // NOTE: Works only for monospaced fonts
    return (u32) renderer2d->font_atlases[fa].glyphs[32].advance.x;
}

LOCAL void start_batch(Renderer2D *renderer2d)
{
    renderer2d->quad_index_count = 0;
    renderer2d->quad_vertex_buffer_ptr = renderer2d->quad_vertex_buffer_base;

    renderer2d->circle_index_count = 0;
    renderer2d->circle_vertex_buffer_ptr = renderer2d->circle_vertex_buffer_base;

    renderer2d->line_vertex_count = 0;
    renderer2d->line_vertex_buffer_ptr = renderer2d->line_vertex_buffer_base;

    renderer2d->text_index_count = 0;
    renderer2d->text_vertex_buffer_ptr = renderer2d->text_vertex_buffer_base;

    renderer2d->texture_slot_index = 1;
}

LOCAL void next_batch(Renderer2D *renderer2d)
{
    flush(renderer2d);
    start_batch(renderer2d);
}

LOCAL void flush(Renderer2D *renderer2d)
{
    if (renderer2d->quad_index_count > 0) {
        u32 size = (u32) ((u8 *) renderer2d->quad_vertex_buffer_ptr - (u8 *) renderer2d->quad_vertex_buffer_base);
        glBindBuffer(GL_ARRAY_BUFFER, renderer2d->quad_vb);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, (const void *) renderer2d->quad_vertex_buffer_base);

        for (u32 i = 0; i < renderer2d->texture_slot_index; i++) {
            glBindTextureUnit(i, renderer2d->texture_slots[i]);
        }

        shader_bind(&renderer2d->quad_shader);
        glBindVertexArray(renderer2d->quad_va);
        glDrawElements(GL_TRIANGLES, renderer2d->quad_index_count, GL_UNSIGNED_INT, NULL);

        renderer2d->stats.draw_calls++;
    }

    if (renderer2d->circle_index_count > 0) {
        u32 size = (u32) ((u8 *) renderer2d->circle_vertex_buffer_ptr - (u8 *) renderer2d->circle_vertex_buffer_base);
        glBindBuffer(GL_ARRAY_BUFFER, renderer2d->circle_vb);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, (const void *) renderer2d->circle_vertex_buffer_base);

        shader_bind(&renderer2d->circle_shader);
        glBindVertexArray(renderer2d->circle_va);
        glDrawElements(GL_TRIANGLES, renderer2d->circle_index_count, GL_UNSIGNED_INT, NULL);

        renderer2d->stats.draw_calls++;
    }

    if (renderer2d->line_vertex_count > 0) {
        u32 size = (u32) ((u8 *) renderer2d->line_vertex_buffer_ptr - (u8 *) renderer2d->line_vertex_buffer_base);
        glBindBuffer(GL_ARRAY_BUFFER, renderer2d->line_vb);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, (const void *) renderer2d->line_vertex_buffer_base);

        shader_bind(&renderer2d->line_shader);
        glBindVertexArray(renderer2d->line_va);
        glDrawArrays(GL_LINES, 0, renderer2d->line_vertex_count);

        renderer2d->stats.draw_calls++;
    }

    if (renderer2d->text_index_count > 0) {
        u32 size = (u32) ((u8 *) renderer2d->text_vertex_buffer_ptr - (u8 *) renderer2d->text_vertex_buffer_base);
        glBindBuffer(GL_ARRAY_BUFFER, renderer2d->text_vb);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, (const void *) renderer2d->text_vertex_buffer_base);

        for (u32 i = 0; i < FA_COUNT; i++) {
            glBindTextureUnit(i, renderer2d->font_atlases[i].texture);
        }

        shader_bind(&renderer2d->text_shader);
        glBindVertexArray(renderer2d->text_va);
        glDrawElements(GL_TRIANGLES, renderer2d->text_index_count, GL_UNSIGNED_INT, NULL);

        renderer2d->stats.draw_calls++;
    }
}

LOCAL void create_shaders(Renderer2D *renderer2d)
{
    Shader_Create_Info quad_shader_create_info = {
        .vertex_filepath = "assets/shaders/quad.vert",
        .fragment_filepath = "assets/shaders/quad.frag"
    };

    if (!shader_create(&quad_shader_create_info, &renderer2d->quad_shader)) {
        LOG_ERROR("failed to create quad shader\n");
    }

    Shader_Create_Info circle_shader_create_info = {
        .vertex_filepath = "assets/shaders/circle.vert",
        .fragment_filepath = "assets/shaders/circle.frag"
    };

    if (!shader_create(&circle_shader_create_info, &renderer2d->circle_shader)) {
        LOG_ERROR("failed to create circle shader\n");
    }

    Shader_Create_Info line_shader_create_info = {
        .vertex_filepath = "assets/shaders/line.vert",
        .fragment_filepath = "assets/shaders/line.frag"
    };

    if (!shader_create(&line_shader_create_info, &renderer2d->line_shader)) {
        LOG_ERROR("failed to create line shader\n");
    }

    Shader_Create_Info text_shader_create_info = {
        .vertex_filepath = "assets/shaders/text.vert",
        .fragment_filepath = "assets/shaders/text.frag"
    };

    if (!shader_create(&text_shader_create_info, &renderer2d->text_shader)) {
        LOG_ERROR("failed to create text shader\n");
    }
}

LOCAL void create_font_atlas(FT_Face ft_face, u32 height, Font_Atlas *out_atlas)
{
    ASSERT(out_atlas);

    if (FT_Set_Pixel_Sizes(ft_face, 0, height) != 0) {
        LOG_ERROR("failed to set new pixel size\n");
        return;
    }

    memset(out_atlas, 0, sizeof(Font_Atlas));

    i32 w = 0, h = 0, by = 0;
    FT_GlyphSlot g = ft_face->glyph;
    for (i32 i = 32; i < 127; i++) {
        if (FT_Load_Char(ft_face, i, FT_LOAD_RENDER) != 0) {
            LOG_ERROR("failed to character `%c`\n", i);
            continue;
        }

        w += g->bitmap.width;
        h = glm::max(h, (i32) g->bitmap.rows);
        by = glm::max(by, (i32) g->bitmap_top);
    }

    out_atlas->width = w;
    out_atlas->height = h;
    out_atlas->bearing_y = by;

    // Glyphs are in a 1-byte greyscale format, so disable the default 4-byte alignment restrictions
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &out_atlas->texture);
    glBindTexture(GL_TEXTURE_2D, out_atlas->texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
    glGenerateMipmap(GL_TEXTURE_2D);

    float borderColor[] = {1.0f, 0.0f, 0.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    i32 x = 0;
    for (i32 i = 32; i < 127; i++) {
        if (FT_Load_Char(ft_face, i, FT_LOAD_RENDER)) {
            continue;
        }

        out_atlas->glyphs[i].size    = glm::vec2(g->bitmap.width, g->bitmap.rows);
        out_atlas->glyphs[i].bearing = glm::vec2(g->bitmap_left, g->bitmap_top);
        out_atlas->glyphs[i].advance = glm::vec2(g->advance.x >> 6, g->advance.y >> 6);
        out_atlas->glyphs[i].xoffset = (f32) x / (f32) w;

        glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, g->bitmap.width, g->bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        x += g->bitmap.width;
    }
}
