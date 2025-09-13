#include <string.h>
#include "utility/log.h"
#include "entity/entity_private.h"
#include "core/engine_private.h"
#include "core/engine.h"
#include "platform/renderer.h"
#include "graphics/draw_list.h"
#include "graphics/render_list.h"

void _engine_render_list_clear(EseEngine *engine) {
    log_assert("ENGINE", engine, "_engine_render_list_clear called with NULL engine");

    if (!engine->active_render_list) {
        render_list_clear(engine->render_list_a);
    } else {
        render_list_clear(engine->render_list_b);
    }
}

void _engine_add_texture_to_draw_list(
    float screen_x, float screen_y, float screen_w, float screen_h, int z_index,
    const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
    int width, int height,
    void *user_data
) {
    log_assert("ENGINE", user_data, "_engine_add_texture_to_draw_list called with NULL user_data");

    EseDrawList *draw_list = (EseDrawList*)user_data;
    EseDrawListObject *obj = draw_list_request_object(draw_list);
    draw_list_object_set_texture(obj, texture_id, texture_x1, texture_y1, texture_x2, texture_y2);
    draw_list_object_set_bounds(obj, screen_x, screen_y, screen_w, screen_h);
    draw_list_object_set_z_index(obj, z_index);
}

void _engine_add_rect_to_draw_list(
    float screen_x, float screen_y, int z_index,
    int width, int height, float rotation, bool filled,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a,
    void *user_data
) {
    log_assert("ENGINE", user_data, "_engine_add_rect_to_draw_list called with NULL user_data");

    EseDrawList *draw_list = (EseDrawList*)user_data;

    EseDrawListObject *obj = draw_list_request_object(draw_list);
    draw_list_object_set_rect_color(obj, r, g, b, a, filled);
    draw_list_object_set_bounds(obj, screen_x, screen_y, width, height);
    draw_list_object_set_rotation(obj, rotation);
    draw_list_object_set_z_index(obj, z_index);
}

void _engine_add_polyline_to_draw_list(
    float screen_x, float screen_y, int z_index,
    const float* points, size_t point_count, float stroke_width,
    unsigned char fill_r, unsigned char fill_g, unsigned char fill_b, unsigned char fill_a,
    unsigned char stroke_r, unsigned char stroke_g, unsigned char stroke_b, unsigned char stroke_a,
    void *user_data
) {
    log_assert("ENGINE", user_data, "_engine_add_polyline_to_draw_list called with NULL user_data");
    log_assert("ENGINE", points, "_engine_add_polyline_to_draw_list called with NULL points");
    log_assert("ENGINE", point_count > 0, "_engine_add_polyline_to_draw_list called with point_count <= 0");

    EseDrawList *draw_list = (EseDrawList*)user_data;

    EseDrawListObject *obj = draw_list_request_object(draw_list);
    draw_list_object_set_polyline(obj, points, point_count, stroke_width);
    draw_list_object_set_polyline_color(obj, fill_r, fill_g, fill_b, fill_a);
    draw_list_object_set_polyline_stroke_color(obj, stroke_r, stroke_g, stroke_b, stroke_a);
    draw_list_object_set_bounds(obj, screen_x, screen_y, 0, 0); // Polylines don't use width/height
    draw_list_object_set_z_index(obj, z_index);
}

bool _engine_render_flip(EseEngine *engine) {
    log_assert("ENGINE", engine, "_engine_render_flip called with NULL engine");

    engine->active_render_list = !engine->active_render_list;
    if (engine->active_render_list) {
        renderer_set_render_list(engine->renderer, engine->render_list_a);
    } else {
        renderer_set_render_list(engine->renderer, engine->render_list_b);
    }
    return true;
}

EseRenderList *_engine_get_render_list(EseEngine *engine) {
    log_assert("ENGINE", engine, "_engine_get_render_list called with NULL engine");

    if (!engine->active_render_list) {
        return engine->render_list_a;
    } else {
        return engine->render_list_b;
    }
}

int _engine_entity_find(void* data, void *user_data) {
    log_assert("ENGINE", data, "_engine_entity_find called with NULL data");
    log_assert("ENGINE", user_data, "_engine_entity_find called with NULL user_data");

    EseEntity *entity = (EseEntity*) data;
    return strcmp(ese_uuid_get_value(entity->id), (const char*)user_data) == 0;
}

EseEntity *_engine_new_entity(EseEngine *engine, const char *id) {
    log_assert("ENGINE", engine, "_engine_new_entity called with NULL engine");

    EseEntity *entity = entity_create(engine->lua_engine);
    dlist_append(engine->entities, (void*)entity);
    // entity_add_to_lua_engine(entity, engine->lua_engine);
    return entity;
}

EseEntity *_engine_find_entity(EseEngine *engine, const char *id) {
    log_assert("ENGINE", engine, "_engine_find_entity called with NULL engine");
    log_assert("ENGINE", id, "_engine_find_entity called with NULL id");

    void* value = dlist_find(engine->entities, _engine_entity_find, (void*)id);
    if (!value) {
        return NULL;
    }

    return (EseEntity*)value;
}