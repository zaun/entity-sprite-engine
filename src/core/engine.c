#include "core/engine.h"
#include "core/collision_resolver.h"
#include "core/console.h"
#include "core/engine_lua.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/pubsub.h"
#include "core/spatial_index.h"
#include "core/system_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_map.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "entity/systems/collider_render_system.h"
#include "entity/systems/map_render_system.h"
#include "entity/systems/shape_render_system.h"
#include "entity/systems/sprite_render_system.h"
#include "entity/systems/sprite_system.h"
#include "entity/systems/text_render_system.h"
#include "graphics/font.h"
#include "graphics/gui/gui.h"
#include "graphics/gui/gui_lua.h"
#include "graphics/render_list.h"
#include "platform/renderer.h"
#include "types/gui_style.h"
#include "types/http.h"
#include "types/input_state.h"
#include "types/input_state_private.h"
#include "types/types.h"
#include "utility/array.h"
#include "utility/double_linked_list.h"
#include "utility/hashmap.h"
#include "utility/job_queue.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include <limits.h>
#include <stdbool.h>
#include <string.h>

EseEngine *engine_create(const char *startup_script) {
    log_init();

    log_debug("ENGINE", "Creating EseEngine.");

    EseEngine *engine = memory_manager.malloc(sizeof(EseEngine), MMTAG_ENGINE);

    engine->renderer = NULL;
    engine->asset_manager = NULL;
    engine->console = console_create();
    engine->pub_sub = ese_pubsub_create();

    engine->draw_list = draw_list_create();
    engine->render_list_a = render_list_create();
    engine->render_list_b = render_list_create();

    engine->entities = dlist_create(NULL);
    engine->del_entities = dlist_create(NULL);

    engine->systems = NULL;
    engine->sys_count = 0;
    engine->sys_cap = 0;

    engine->spatial_index = spatial_index_create();
    engine->collision_resolver = collision_resolver_create();

    engine->lua_engine = lua_engine_create();

    // Create job queue with 4 workers by default (no per-worker init/deinit)
    engine->job_queue = ese_job_queue_create(4, NULL, NULL);

    // Initialize GUI Lua functions after GUI is created
    engine->gui = ese_gui_create(engine->lua_engine);
    ese_gui_lua_init(engine->lua_engine);

    // Add lookups
    lua_engine_add_registry_key(engine->lua_engine->runtime, ENGINE_KEY, engine);
    lua_engine_add_registry_key(engine->lua_engine->runtime, LUA_ENGINE_KEY, engine->lua_engine);

    // Add Entities
    entity_lua_init(engine->lua_engine);
    entity_component_lua_init(engine->lua_engine);

    // Add types
    ese_arc_lua_init(engine->lua_engine);
    ese_camera_lua_init(engine->lua_engine);
    ese_color_lua_init(engine->lua_engine);
    ese_display_lua_init(engine->lua_engine);
    ese_input_state_lua_init(engine->lua_engine);
    ese_map_lua_init(engine->lua_engine);
    ese_map_cell_lua_init(engine->lua_engine);
    ese_point_lua_init(engine->lua_engine);
    ese_poly_line_lua_init(engine->lua_engine);
    ese_ray_lua_init(engine->lua_engine);
    ese_rect_lua_init(engine->lua_engine);
    ese_collision_hit_lua_init(engine->lua_engine);
    ese_tileset_lua_init(engine->lua_engine);
    ese_vector_lua_init(engine->lua_engine);
    ese_uuid_lua_init(engine->lua_engine);
    ese_gui_style_lua_init(engine->lua_engine);
    ese_http_request_lua_init(engine->lua_engine);

    // Add functions
    lua_engine_add_function(engine->lua_engine, "print", _lua_print);
    lua_engine_add_function(engine->lua_engine, "asset_load_script", _lua_asset_load_script);
    lua_engine_add_function(engine->lua_engine, "asset_load_atlas", _lua_asset_load_atlas);
    lua_engine_add_function(engine->lua_engine, "asset_load_shader", _lua_asset_load_shader);
    lua_engine_add_function(engine->lua_engine, "asset_load_map", _lua_asset_load_map);
    lua_engine_add_function(engine->lua_engine, "asset_get_map", _lua_asset_get_map);
    lua_engine_add_function(engine->lua_engine, "set_pipeline", _lua_set_pipeline);
    lua_engine_add_function(engine->lua_engine, "detect_collision", _lua_detect_collision);
    lua_engine_add_function(engine->lua_engine, "scene_clear", _lua_scene_clear);
    lua_engine_add_function(engine->lua_engine, "scene_reset", _lua_scene_reset);

    // Add globals
    engine->input_state = ese_input_state_create(engine->lua_engine);
    ese_input_state_ref(engine->input_state);
    lua_engine_add_global(engine->lua_engine, "InputState", engine->input_state->lua_ref);

    engine->display_state = ese_display_create(engine->lua_engine);
    ese_display_ref(engine->display_state);
    lua_engine_add_global(engine->lua_engine, "Display",
                          ese_display_get_lua_ref(engine->display_state));

    engine->camera_state = ese_camera_create(engine->lua_engine);
    ese_camera_ref(engine->camera_state);
    lua_engine_add_global(engine->lua_engine, "Camera", engine->camera_state->lua_ref);

    // Lock global
    lua_engine_global_lock(engine->lua_engine);

    // Register systems
    engine_register_sprite_system(engine);
    engine_register_sprite_render_system(engine);
    engine_register_shape_render_system(engine);
    engine_register_text_render_system(engine);
    engine_register_collider_render_system(engine);
    engine_register_map_render_system(engine);

    // Load startup script
    if (startup_script) {
        lua_engine_load_script(engine->lua_engine, startup_script, "STARTUP");
        engine->startup_ref = lua_engine_instance_script(engine->lua_engine, startup_script);
    }

    engine->active_render_list = true;
    engine->draw_console = false;

    return engine;
}

void engine_destroy(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_destroy called with NULL engine");

    // Destroy all systems first (they may reference entities)
    if (engine->systems) {
        for (size_t i = 0; i < engine->sys_count; i++) {
            system_manager_destroy(engine->systems[i], engine);
        }
        memory_manager.free(engine->systems);
    }

    ese_gui_destroy(engine->gui);
    draw_list_destroy(engine->draw_list);
    render_list_destroy(engine->render_list_a);
    render_list_destroy(engine->render_list_b);

    // Now free the entirty lists
    void *v;
    while ((v = dlist_pop_front(engine->entities)) != NULL) {
        entity_destroy((EseEntity *)v);
    }
    while ((v = dlist_pop_front(engine->del_entities)) != NULL) {
        entity_destroy((EseEntity *)v);
    }
    dlist_free(engine->entities);
    dlist_free(engine->del_entities);

    if (engine->asset_manager) {
        asset_manager_destroy(engine->asset_manager);
    }
    ese_input_state_destroy(engine->input_state);

    if (engine->pub_sub) {
        ese_pubsub_destroy(engine->pub_sub);
    }
    if (engine->job_queue) {
        ese_job_queue_destroy(engine->job_queue);
    }

    ese_display_destroy(engine->display_state);
    ese_camera_destroy(engine->camera_state);
    console_destroy(engine->console);

    if (engine->collision_resolver) {
        collision_resolver_destroy(engine->collision_resolver);
    }
    if (engine->spatial_index) {
        spatial_index_destroy(engine->spatial_index);
    }

    lua_engine_instance_remove(engine->lua_engine, engine->startup_ref);
    lua_engine_remove_registry_key(engine->lua_engine->runtime, ENGINE_KEY);
    lua_engine_remove_registry_key(engine->lua_engine->runtime, LUA_ENGINE_KEY);
    lua_engine_destroy(engine->lua_engine);

    memory_manager.free(engine);
}

EseGui *engine_get_gui(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_get_gui called with NULL engine");
    return engine->gui;
}

void engine_add_entity(EseEngine *engine, EseEntity *entity) {
    log_assert("ENGINE", engine, "engine_add_entity called with NULL engine");
    log_assert("ENGINE", entity, "engine_add_entity called with NULL entity");

    log_verbose("ENGINE", "Added entity %s", ese_uuid_get_value(entity->id));
    dlist_append(engine->entities, entity);
}

void engine_remove_entity(EseEngine *engine, EseEntity *entity) {
    log_assert("ENGINE", engine, "engine_remove_entity called with NULL engine");
    log_assert("ENGINE", entity, "engine_remove_entity called with NULL entity");

    log_verbose("ENGINE", "Removed entity %s", ese_uuid_get_value(entity->id));
    dlist_append(engine->del_entities, entity);
}

void engine_clear_entities(EseEngine *engine, bool include_persistent) {
    log_assert("ENGINE", engine, "engine_clear_entities called with NULL engine");

    // We can't remove from engine->entities while iterating over it,
    // so just add it to the delete list, we'll remvoe from the entites
    // list later
    void *value;
    EseDListIter *iter = dlist_iter_create(engine->entities);
    while (dlist_iter_next(iter, &value)) {
        EseEntity *entity = (EseEntity *)value;
        if (include_persistent || !entity_get_persistent(entity)) {
            dlist_append(engine->del_entities, entity);
        }
    }
    dlist_iter_free(iter);
}

void engine_start(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_start called with NULL engine");

    // Update the display state
    int view_width = 0, view_height = 0;
    if (engine->renderer) {
        renderer_get_size(engine->renderer, &view_width, &view_height);
    }

    ese_display_set_dimensions(engine->display_state, view_width, view_height);
    ese_display_set_viewport(engine->display_state, view_width, view_height);

    // Run startup script using the new function reference system
    if (engine->startup_ref != LUA_NOREF) {
        lua_engine_run_function(engine->lua_engine, engine->startup_ref, LUA_NOREF, "startup", 0,
                                NULL, NULL);
    }

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

    // Add fonts
    asset_manager_create_font_atlas(engine->asset_manager, "console_font_10x20", console_font_10x20,
                                    256, 10, 20);
}

void engine_update(EseEngine *engine, float delta_time, const EseInputState *state) {
    log_assert("ENGINE", engine, "engine_update called with NULL engine");
    log_assert("ENGINE", state, "engine_update called with NULL state");

    // Start overall timing
    profile_start(PROFILE_ENG_UPDATE_OVERALL);

    // Update the input state
    if (engine->renderer) {
        profile_start(PROFILE_ENG_UPDATE_SECTION);
        int view_width, view_height;
        renderer_get_size(engine->renderer, &view_width, &view_height);

        ese_display_set_dimensions(engine->display_state, view_width, view_height);
        ese_display_set_viewport(engine->display_state, view_width, view_height);
    }

    // Update the input state
    memcpy(engine->input_state->keys_down, state->keys_down, sizeof(state->keys_down));
    memcpy(engine->input_state->keys_pressed, state->keys_pressed, sizeof(state->keys_pressed));
    memcpy(engine->input_state->keys_released, state->keys_released, sizeof(state->keys_released));
    memcpy(engine->input_state->mouse_down, state->mouse_down, sizeof(state->mouse_down));
    memcpy(engine->input_state->mouse_clicked, state->mouse_clicked, sizeof(state->mouse_clicked));
    memcpy(engine->input_state->mouse_released, state->mouse_released,
           sizeof(state->mouse_released));

    engine->input_state->mouse_x = state->mouse_x;
    engine->input_state->mouse_y = state->mouse_y;
    engine->input_state->mouse_scroll_dx = state->mouse_scroll_dx;
    engine->input_state->mouse_scroll_dy = state->mouse_scroll_dy;
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_input_state");

    // hardcodes engine inputs
    if ((engine->input_state->keys_down[InputKey_LCMD] ||
         engine->input_state->keys_down[InputKey_RCMD]) &&
        (engine->input_state->keys_down[InputKey_LALT] ||
         engine->input_state->keys_down[InputKey_RALT]) &&
        engine->input_state->keys_pressed[InputKey_C]) {
        log_debug("ENGINE", "Toggle console");
        engine->draw_console = !engine->draw_console;
        return;
    }

    // Run ECS Systems in phases
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    engine_run_phase(engine, SYS_PHASE_EARLY, delta_time,
                     true); // Parallel systems before Lua
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_systems_early");

    // Entity PASS ONE - Update each active entity.
    profile_start(PROFILE_ENG_UPDATE_SECTION);

    int entity_count = 0;

    void *value;
    EseDListIter *entity_iter = dlist_iter_create(engine->entities);
    while (dlist_iter_next(entity_iter, &value)) {
        EseEntity *entity = (EseEntity *)value;

        // Skip inactive entities
        if (!entity->active) {
            continue;
        }

        entity_count++;

        // Update components
        for (size_t i = 0; i < entity->component_count; i++) {
            if (!entity->components[i]->active) {
                continue;
            }

            entity_component_update(entity->components[i], entity, delta_time);
        }
    }
    dlist_iter_free(entity_iter);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_entity_update");

    // Run LUA phase systems (single-threaded for Lua scripts)
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    engine_run_phase(engine, SYS_PHASE_LUA, delta_time, false);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_systems_lua");

    // Entity PASS TWO - Check for collisions
    profile_start(PROFILE_ENG_UPDATE_SECTION);

    // Clear collision states for all entities at the beginning of each frame
    // This swaps current and previous collision states
    void *clear_value;
    EseDListIter *clear_iter = dlist_iter_create(engine->entities);
    while (dlist_iter_next(clear_iter, &clear_value)) {
        EseEntity *entity = (EseEntity *)clear_value;
        if (!entity->active)
            continue;
    }
    dlist_iter_free(clear_iter);

    // Entity PASS TWO Step 1: Collect spatial pairs using spatial index
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    spatial_index_clear(engine->spatial_index);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_collision_spatial_clear");

    // Insert active entities
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    void *entity_value;
    EseDListIter *insert_iter = dlist_iter_create(engine->entities);
    while (dlist_iter_next(insert_iter, &entity_value)) {
        EseEntity *entity = (EseEntity *)entity_value;
        if (!entity->active)
            continue;
        spatial_index_insert(engine->spatial_index, entity);
    }
    dlist_iter_free(insert_iter);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_collision_spatial_insert");

    // Get spatial pairs from the spatial index (array is cleared internally)
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    EseArray *spatial_pairs = spatial_index_get_pairs(engine->spatial_index);
    size_t spatial_pair_count = array_size(spatial_pairs);
    for (size_t spi = 0; spi < spatial_pair_count; spi++) {
        profile_count_add("eng_collision_spatial_pairs_count");
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_collision_spatial_get_pairs");

    // Resolve pairs into detailed collision hits
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    EseArray *collision_hits =
        collision_resolver_solve(engine->collision_resolver, spatial_pairs, engine->lua_engine);
    size_t resolved_hit_count = array_size(collision_hits);
    for (size_t rhi = 0; rhi < resolved_hit_count; rhi++) {
        profile_count_add("eng_collision_hits_count");
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_collision_resolver_solve");
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_collision_detect");

    // Entity PASS TWO Step 2: Process collision callbacks for all pairs
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    size_t hit_count = array_size(collision_hits);
    for (size_t i = 0; i < hit_count; i++) {
        EseCollisionHit *hit = (EseCollisionHit *)array_get(collision_hits, i);
        if (!hit)
            continue;
        entity_process_collision_callbacks(hit);
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_collision_callback");

    // Entity PASS THREE - Create draw calls for each active entity (non-sprite
    // components) This creates a flat list of draw calls from all entities. The
    // entity_draw() function is responsible for performing visibility culling
    // based on the camera position and view dimensions passed to it. Each
    // visible entity may contribute multiple draw calls to the list.
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    draw_list_clear(engine->draw_list);
    entity_iter = dlist_iter_create(engine->entities);
    while (dlist_iter_next(entity_iter, &value)) {
        EseEntity *entity = (EseEntity *)value;

        if (!entity->active || !entity->visible) {
            continue;
        }

        EntityDrawCallbacks callbacks = {.draw_texture = _engine_add_texture_to_draw_list,
                                         .draw_rect = _engine_add_rect_to_draw_list,
                                         .draw_polyline = _engine_add_polyline_to_draw_list};

        entity_draw(entity, ese_point_get_x(engine->camera_state->position),
                    ese_point_get_y(engine->camera_state->position),
                    ese_display_get_viewport_width(engine->display_state),
                    ese_display_get_viewport_height(engine->display_state), &callbacks,
                    engine->draw_list);
    }
    dlist_iter_free(entity_iter);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_entity_draw");

    // Run LATE phase systems (parallel after Lua, before render)
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    engine_run_phase(engine, SYS_PHASE_LATE, delta_time, true);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_systems_late");

    // draw the gui
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    ese_gui_input(engine->gui, engine->input_state);
    ese_gui_process(engine->gui, engine->draw_list);
    ese_gui_cleanup(engine->gui);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_gui_draw");

    // Draw the console
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    if (engine->draw_console) {
        EntityDrawCallbacks console_callbacks = {.draw_texture = _engine_add_texture_to_draw_list,
                                                 .draw_rect = _engine_add_rect_to_draw_list,
                                                 .draw_polyline =
                                                     _engine_add_polyline_to_draw_list};

        console_draw(engine->console, engine->asset_manager,
                     ese_display_get_viewport_width(engine->display_state),
                     ese_display_get_viewport_height(engine->display_state), &console_callbacks,
                     engine->draw_list);
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_console_draw");

    // Renderer update - Create a batched render list
    // incliding all texture and vertext information
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    EseRenderList *render_list = _engine_get_render_list(engine);
    render_list_clear(render_list);
    render_list_set_size(render_list, ese_display_get_viewport_width(engine->display_state),
                         ese_display_get_viewport_height(engine->display_state));
    render_list_fill(render_list, engine->draw_list);

    // Flip the updated render list to be active
    if (engine->renderer) {
        _engine_render_flip(engine);
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_renderer");

    // Process completed async job callbacks on the main thread
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    if (engine->job_queue) {
        ese_job_queue_process(engine->job_queue);
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_job_queue_poll");

    profile_start(PROFILE_ENG_UPDATE_SECTION);
    lua_gc(engine->lua_engine->runtime, LUA_GCCOLLECT, 0);
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_lua_gc");

    // Delete entities
    profile_start(PROFILE_ENG_UPDATE_SECTION);
    void *v;
    while ((v = dlist_pop_front(engine->del_entities)) != NULL) {
        EseEntity *entity = (EseEntity *)v;
        // remvoe from entities list here
        dlist_remove_by_value(engine->entities, entity);
        entity_destroy(entity);
    }
    profile_stop(PROFILE_ENG_UPDATE_SECTION, "eng_update_del_entities");

    // Overall update time
    profile_stop(PROFILE_ENG_UPDATE_OVERALL, "eng_update_overall");
}

EseJobQueue *engine_get_job_queue(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_get_job_queue called with NULL engine");
    return engine->job_queue;
}

EseCamera *engine_get_camera(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_get_camera called with NULL engine");
    return engine->camera_state;
}

EseDisplay *engine_get_display(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_get_display called with NULL engine");
    return engine->display_state;
}

EseDrawList *engine_get_draw_list(EseEngine *engine) {
    log_assert("ENGINE", engine, "engine_get_draw_list called with NULL engine");
    return engine->draw_list;
}

EseEntity **engine_detect_collision_rect(EseEngine *engine, EseRect *rect, int max_count) {
    // allocate array of pointers (+1 for NULL terminator)
    EseEntity **results =
        memory_manager.malloc(sizeof(EseEntity *) * (max_count + 1), MMTAG_ENGINE);
    if (!results) {
        return NULL; // allocation failed
    }

    int count = 0;
    void *entity_value;
    EseDListIter *iter_a = dlist_iter_create(engine->entities);

    while (dlist_iter_next(iter_a, &entity_value)) {
        EseEntity *entity = (EseEntity *)entity_value;

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

// Tag system functions

/**
 * @brief Helper function to capitalize a string and truncate to MAX_TAG_LENGTH
 */
static void _normalize_tag(char *dest, const char *src) {
    size_t i = 0;
    while (src[i] && i < 16 - 1) { // MAX_TAG_LENGTH is 16
        if (src[i] >= 'a' && src[i] <= 'z') {
            dest[i] = src[i] - 32; // Convert to uppercase
        } else {
            dest[i] = src[i];
        }
        i++;
    }
    dest[i] = '\0';
}

EseEntity **engine_find_by_tag(EseEngine *engine, const char *tag, int max_count) {
    log_assert("ENGINE", engine, "engine_find_by_tag called with NULL engine");
    log_assert("ENGINE", tag, "engine_find_by_tag called with NULL tag");

    if (max_count == 0) {
        return NULL;
    }

    char normalized_tag[16]; // MAX_TAG_LENGTH
    _normalize_tag(normalized_tag, tag);

    // Allocate result array (max_count + 1 for NULL terminator)
    EseEntity **result = memory_manager.malloc(sizeof(EseEntity *) * (max_count + 1), MMTAG_ENGINE);
    if (!result) {
        log_error("ENGINE", "engine_find_by_tag: failed to allocate result array");
        return NULL;
    }

    int found_count = 0;
    void *entity_value;
    EseDListIter *iter = dlist_iter_create(engine->entities);

    while (dlist_iter_next(iter, &entity_value) && found_count < max_count) {
        EseEntity *entity = (EseEntity *)entity_value;
        if (entity && entity->active && entity_has_tag(entity, normalized_tag)) {
            result[found_count++] = entity;
        }
    }

    dlist_iter_free(iter);

    if (found_count == 0) {
        // No results; free and return NULL per API preference
        memory_manager.free(result);
        return NULL;
    }

    // NULL-terminate the array
    result[found_count] = NULL;

    return result;
}

void engine_add_to_console(EseEngine *engine, EseConsoleLineType type, const char *prefix,
                           const char *message) {
    log_assert("ENGINE", engine, "engine_add_to_console called with NULL engine");
    log_assert("ENGINE", prefix, "engine_add_to_console called with NULL prefix");
    log_assert("ENGINE", message, "engine_add_to_console called with NULL message");

    console_add_line(engine->console, type, prefix, message);
}

void engine_show_console(EseEngine *engine, bool show) {
    log_assert("ENGINE", engine, "engine_show_console called with NULL engine");

    engine->draw_console = show;
}

EseEntity *engine_find_by_id(EseEngine *engine, const char *uuid_string) {
    log_assert("ENGINE", engine, "engine_find_by_id called with NULL engine");
    log_assert("ENGINE", uuid_string, "engine_find_by_id called with NULL uuid_string");

    void *entity_value;
    EseDListIter *iter = dlist_iter_create(engine->entities);

    while (dlist_iter_next(iter, &entity_value)) {
        EseEntity *entity = (EseEntity *)entity_value;
        if (entity && entity->active && strcmp(ese_uuid_get_value(entity->id), uuid_string) == 0) {
            dlist_iter_free(iter);
            return entity;
        }
    }

    dlist_iter_free(iter);
    return NULL;
}

int engine_get_entity_count(EseEngine *engine) { return dlist_size(engine->entities); }

// Pub/Sub passthrough functions
void engine_pubsub_pub(EseEngine *engine, const char *name, const EseLuaValue *data) {
    log_assert("ENGINE", engine, "engine_pubsub_pub called with NULL engine");
    log_assert("ENGINE", name, "engine_pubsub_pub called with NULL name");
    log_assert("ENGINE", data, "engine_pubsub_pub called with NULL data");

    ese_pubsub_pub(engine->pub_sub, name, data);
}

void engine_pubsub_sub(EseEngine *engine, const char *name, EseEntity *entity,
                       const char *function_name) {
    log_assert("ENGINE", engine, "engine_pubsub_sub called with NULL engine");
    log_assert("ENGINE", name, "engine_pubsub_sub called with NULL name");
    log_assert("ENGINE", entity, "engine_pubsub_sub called with NULL entity");
    log_assert("ENGINE", function_name, "engine_pubsub_sub called with NULL function_name");

    ese_pubsub_sub(engine->pub_sub, name, entity, function_name);
}

void engine_pubsub_unsub(EseEngine *engine, const char *name, EseEntity *entity,
                         const char *function_name) {
    log_assert("ENGINE", engine, "engine_pubsub_unsub called with NULL engine");
    log_assert("ENGINE", name, "engine_pubsub_unsub called with NULL name");
    log_assert("ENGINE", entity, "engine_pubsub_unsub called with NULL entity");
    log_assert("ENGINE", function_name, "engine_pubsub_unsub called with NULL function_name");

    ese_pubsub_unsub(engine->pub_sub, name, entity, function_name);
}
