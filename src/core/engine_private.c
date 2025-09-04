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
    return strcmp(entity->id->value, (const char*)user_data) == 0;
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

void _engine_delete_entity(EseEngine *engine, EseEntity *entity) {
    log_assert("ENGINE", engine, "_engine_delete_entity called with NULL engine");
    log_assert("ENGINE", entity, "_engine_delete_entity called with NULL entity");

    dlist_remove_by_value(engine->entities, entity);
    dlist_append(engine->del_entities, entity);
}
