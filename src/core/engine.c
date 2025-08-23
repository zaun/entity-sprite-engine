#include <stdbool.h>
#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/entity.h"
#include "types/types.h"
#include "entity/components/entity_component.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "vendor/lua/src/lauxlib.h"
#include "graphics/render_list.h"
#include "platform/renderer.h"
#include "platform/filesystem.h"
#include "core/engine_lua.h"
#include "core/engine_private.h"
#include "core/engine.h"

 
EseEngine *engine_create(const char *startup_script) {
    log_init();

    log_assert("ENGINE", startup_script, "engine_create called with NULL startup_script");

    log_debug("ENGINE", "Creating EseEngine.");

    EseEngine *engine = memory_manager.malloc(sizeof(EseEngine), MMTAG_ENGINE);

    engine->renderer = NULL;
    engine->asset_manager = NULL;

    engine->draw_list = draw_list_create();
    engine->render_list_a = render_list_create();
    engine->render_list_b = render_list_create();

    engine->entities = dlist_create((DListFreeFn)entity_destroy);
    engine->lua_engine = lua_engine_create();

    // Add lookups
    lua_engine_add_registry_key(engine->lua_engine->runtime, ENGINE_KEY, engine);
    lua_engine_add_registry_key(engine->lua_engine->runtime, LUA_ENGINE_KEY, engine->lua_engine);

    // Add Entities
    entity_lua_init(engine->lua_engine);
    entity_component_lua_init(engine->lua_engine);

    // Add types
    arc_lua_init(engine->lua_engine);
    camera_state_lua_init(engine->lua_engine);
    display_state_lua_init(engine->lua_engine);
    input_state_lua_init(engine->lua_engine);
    map_lua_init(engine->lua_engine);
    mapcell_lua_init(engine->lua_engine);
    point_lua_init(engine->lua_engine);
    ray_lua_init(engine->lua_engine);
    rect_lua_init(engine->lua_engine);
    tileset_lua_init(engine->lua_engine);
    vector_lua_init(engine->lua_engine);
    uuid_lua_init(engine->lua_engine);

    // Add functions
    lua_engine_add_function(engine->lua_engine, "print", _lua_print);
    lua_engine_add_function(engine->lua_engine, "asset_load_script", _lua_asset_load_script);
    lua_engine_add_function(engine->lua_engine, "asset_load_atlas", _lua_asset_load_atlas);
    lua_engine_add_function(engine->lua_engine, "asset_load_shader", _lua_asset_load_shader);
    lua_engine_add_function(engine->lua_engine, "asset_load_map", _lua_asset_load_map);
    lua_engine_add_function(engine->lua_engine, "asset_get_map", _lua_asset_get_map);
    lua_engine_add_function(engine->lua_engine, "set_pipeline", _lua_set_pipeline);
    lua_engine_add_function(engine->lua_engine, "detect_collision", _lua_detect_collision);

    // Add globals
    engine->input_state = input_state_create(engine->lua_engine);
    lua_engine_add_global(engine->lua_engine, "InputState", engine->input_state->lua_ref);

    engine->display_state = display_state_create(engine->lua_engine);
    lua_engine_add_global(engine->lua_engine, "Display", engine->display_state->lua_ref);

    engine->camera_state = camera_state_create(engine->lua_engine);
    lua_engine_add_global(engine->lua_engine, "Camera", engine->camera_state->lua_ref);

    // Lock global
    lua_engine_global_lock(engine->lua_engine);

    lua_engine_load_script(engine->lua_engine, startup_script, "STARTUP");
    engine->startup_ref = lua_engine_instance_script(engine->lua_engine, startup_script);

    engine->active_render_list = true;

    return engine;
}

void engine_destroy(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_destroy called with NULL engine");

    draw_list_free(engine->draw_list);
    render_list_destroy(engine->render_list_a);
    render_list_destroy(engine->render_list_b);

    dlist_free(engine->entities);
    asset_manager_destroy(engine->asset_manager);
    input_state_destroy(engine->input_state);
    display_state_destroy(engine->display_state);
    camera_state_destroy(engine->camera_state);

    lua_engine_instance_remove(engine->lua_engine, engine->startup_ref);
    lua_engine_remove_registry_key(engine->lua_engine->runtime, LUA_ENGINE_KEY);
    lua_engine_destroy(engine->lua_engine);

    memory_manager.free(engine);
}

void engine_add_entity(EseEngine *engine, EseEntity *entity) {
    log_assert("ENGINE", engine, "engine_destroy called with NULL engine");
    log_assert("ENGINE", entity, "engine_destroy called with NULL entity");

    log_debug("ENGINE", "Added entity %s", entity->id->value);
    dlist_append(engine->entities, entity);
}

void engine_start(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_start called with NULL engine");
    log_debug("ENGINE", "start");

    // Update the display state
    int view_width, view_height;
    renderer_get_size(engine->renderer, &view_width, &view_height);

    display_state_set_dimensions(engine->display_state, view_width, view_height);
    display_state_set_viewport(engine->display_state, view_width, view_height);

    lua_engine_instance_run_function(engine->lua_engine, engine->startup_ref, LUA_NOREF, "startup");

    engine->isRunning = true;
}

void engine_set_renderer(EseEngine *engine, EseRenderer *renderer) {
    log_assert("ENGINE", engine, "engine_set_renderer called with NULL engine");

    engine->renderer = renderer;
    if (!renderer) {
        return;
    }

    renderer_set_render_list(engine->renderer, engine->render_list_a);

    if (engine->asset_manager) {
        asset_manager_destroy(engine->asset_manager);
    }
    engine->asset_manager = asset_manager_create(engine->renderer);
}

void engine_update(EseEngine *engine, float delta_time, const EseInputState *state) {
    log_assert("ENGINE", engine, "engine_update called with NULL engine");
    log_assert("ENGINE", state, "engine_update called with NULL state");

    // Update the display state
    int view_width, view_height;
    renderer_get_size(engine->renderer, &view_width, &view_height);

    display_state_set_dimensions(engine->display_state, view_width, view_height);
    display_state_set_viewport(engine->display_state, view_width, view_height);

    // Update the input state
    memcpy(engine->input_state->keys_down, state->keys_down, sizeof(state->keys_down));
    memcpy(engine->input_state->keys_pressed, state->keys_pressed, sizeof(state->keys_pressed));
    memcpy(engine->input_state->keys_released, state->keys_released, sizeof(state->keys_released));
    memcpy(engine->input_state->mouse_buttons, state->mouse_buttons, sizeof(state->mouse_buttons));

    engine->input_state->mouse_x = state->mouse_x;
    engine->input_state->mouse_y = state->mouse_y;
    engine->input_state->mouse_scroll_dx = state->mouse_scroll_dx;
    engine->input_state->mouse_scroll_dy = state->mouse_scroll_dy;

    // Entity PASS ONE - Update each active entity.
	void* value;
    EseDListIter* entity_iter = dlist_iter_create(engine->entities);
	while (dlist_iter_next(entity_iter, &value)) {
	    EseEntity *entity = (EseEntity*)value;

        // Skip inactive entities
        if (!entity->active) {
            continue;
        }
        entity_update(entity, delta_time);
	}
	dlist_iter_free(entity_iter);

    // Entity PASS TWO - Check for collisions
    void* entity_a_value;
    EseDListIter* iter_a = dlist_iter_create(engine->entities);
    while (dlist_iter_next(iter_a, &entity_a_value)) {
        EseEntity *entity_a = (EseEntity*)entity_a_value;
        // Skip inactive entities
        if (!entity_a->active) {
            continue;
        }

        void* entity_b_value;
        // Create a new iterator starting from the current position to avoid redundant checks
        EseDListIter* iter_b = dlist_iter_create_from(iter_a);
        while (dlist_iter_next(iter_b, &entity_b_value)) {
            EseEntity *entity_b = (EseEntity*)entity_b_value;
            // Skip inactive entities, entities without colliders, and the entity itself
            if (!entity_b->active || entity_a == entity_b) {
                continue;
            }

            // Perform the collision test
            entity_process_collision(entity_a, entity_b);
        }
        dlist_iter_free(iter_b);
    }
    dlist_iter_free(iter_a);

    // Camera's view rectangle (centered)
    float view_left   = engine->camera_state->position->x - view_width  / 2.0f;
    float view_right  = engine->camera_state->position->x + view_width  / 2.0f;
    float view_top    = engine->camera_state->position->y - view_height / 2.0f;
    float view_bottom = engine->camera_state->position->y + view_height / 2.0f;

    // Entity PASS THREE - Create draw calls for each active entity
    // This creates a flat list of draw calls from all entities. The entity_draw() 
    // function is responsible for performing visibility culling based on the 
    // camera position and view dimensions passed to it. Each visible entity 
    // may contribute multiple draw calls to the list.
    draw_list_clear(engine->draw_list);
    entity_iter = dlist_iter_create(engine->entities);
	while (dlist_iter_next(entity_iter, &value)) {
	    EseEntity *entity = (EseEntity*)value;

        entity_draw(
            entity,
            engine->camera_state->position->x,
            engine->camera_state->position->y,
            view_width, view_height,
            _engine_add_texture_to_draw_list,
            _engine_add_rect_to_draw_list,
            engine->draw_list
        );
    }
	dlist_iter_free(entity_iter);

    // Renderer update - Create a batched render list
    // incliding all texture and vertext information
    EseRenderList *render_list = _engine_get_render_list(engine);
    render_list_clear(render_list);
    render_list_set_size(render_list, view_width, view_height);
    render_list_fill(render_list, engine->draw_list);

    // Flip the updated render list to be active
    _engine_render_flip(engine);

    // Forec Lua the GC each frame
    lua_engine_gc(engine->lua_engine);
}

EseEntity **engine_detect_collision_rect(EseEngine *engine, EseRect *rect, int max_count) {
    // allocate array of pointers (+1 for NULL terminator)
    EseEntity **results = memory_manager.malloc(sizeof(EseEntity*) * (max_count + 1), MMTAG_ENGINE);
    if (!results) {
        return NULL; // allocation failed
    }

    int count = 0;
    void *entity_value;
    EseDListIter *iter_a = dlist_iter_create(engine->entities);

    while (dlist_iter_next(iter_a, &entity_value)) {
        EseEntity *entity = (EseEntity*)entity_value;

        // skip inactive
        if (!entity->active) {
            continue;
        }

        if (entity_detect_collision_rect(entity, rect)) {
            results[count++] = entity;

            // stop if we hit max_count
            if (count >= max_count) {
                break;
            }
        }
    }

    dlist_iter_free(iter_a);

    // null terminate
    results[count] = NULL;

    return results;
}

// Asset manager passthorugh functions
EseSprite *engine_get_sprite(EseEngine *engine, const char *sprite_id) {
    return asset_manager_get_sprite(engine->asset_manager, sprite_id);
}
