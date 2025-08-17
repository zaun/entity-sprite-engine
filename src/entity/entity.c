#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "entity/components/entity_component_private.h"
#include "types/types.h"
#include "utility/log.h"
#include "core/memory_manager.h"
#include "entity/entity_private.h"
#include "entity/entity_lua.h"
#include "entity/entity.h"

EseEntity *entity_create(EseLuaEngine *engine) {
    log_assert("ENTITY", engine, "entity_create called with NULL engine");

    EseEntity *entity = _entity_make(engine);
    _entity_lua_register(entity, false); // C-created = C-owned
    return entity;
}

EseEntity *entity_copy(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_copy called with NULL entity");
    
    EseEntity *copy = _entity_make(entity->lua);
    _entity_lua_register(copy, false); // C-created = C-owned

    // Copy all fields
    copy->active = entity->active;
    copy->position->x = entity->position->x;
    copy->position->y = entity->position->y;
    copy->draw_order = entity->draw_order;

    // Copy components
    copy->components = memory_manager.malloc(sizeof(EseEntityComponent*) * entity->component_capacity, MMTAG_ENTITY);
    copy->component_capacity = entity->component_capacity;
    copy->component_count = entity->component_count;

    for (size_t i = 0; i < entity->component_count; ++i) {
        EseEntityComponent *src_comp = entity->components[i];
        EseEntityComponent *dst_comp = entity_component_copy(src_comp);        
        copy->components[i] = dst_comp;
    }

    // Copy default props
    copy->default_props = dlist_copy(entity->default_props, (DListCopyFn)lua_value_copy);

    return copy;
}

void entity_destroy(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_destroy called with NULL entity");

    uuid_destroy(entity->id);
    point_destroy(entity->position);

    hashmap_free(entity->current_collisions);

    for (size_t i = 0; i < entity->component_count; ++i) {
        entity_component_destroy(entity->components[i]);
    }
    memory_manager.free(entity->components);

    if (entity->lua_ref != LUA_NOREF) {
        luaL_unref(entity->lua->runtime, LUA_REGISTRYINDEX, entity->lua_ref);
    }
    if (entity->lua_val_ref) {
        lua_value_free(entity->lua_val_ref);
    }

    dlist_free(entity->default_props);

    memory_manager.free(entity);
}

void entity_update(EseEntity *entity, float delta_time) {
    log_assert("ENTITY", entity, "entity_update called with NULL entity");

    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active) {
            continue;
        }
        entity_component_update(entity->components[i], entity, delta_time);
    }
}

void entity_run_function_with_args(
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv
) {
    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active || entity->components[i]->type != ENTITY_COMPONENT_LUA) {
            continue;
        }
        entity_component_run_function_with_args(entity->components[i], func_name, argc, argv);
    }
}

void entity_process_collision(EseEntity *entity, EseEntity *test) {
    log_assert("ENTITY", entity, "entity_process_collision called with NULL entity");
    log_assert("ENTITY", test, "entity_process_collision called with NULL test");

    // Get the key.
    const char* canonical_key = _get_collision_key(entity->id, test->id);

    // Read the state from the PREVIOUS frame for both entities.
    bool was_colliding_a = hashmap_get(entity->current_collisions, canonical_key) != NULL;
    bool was_colliding_b = hashmap_get(test->current_collisions, canonical_key) != NULL;

    bool currently_colliding = _entity_test_collision(entity, test);

    if (currently_colliding && (!was_colliding_a || !was_colliding_b)) {
        // Collision Enter
        entity_run_function_with_args(entity, "entity_collision_enter", 1, test->lua_val_ref);
        entity_run_function_with_args(test, "entity_collision_enter", 1, entity->lua_val_ref);
    } else if (currently_colliding && was_colliding_a && was_colliding_b) {
        // Collision Stay
        entity_run_function_with_args(entity, "entity_collision_stay", 1, test->lua_val_ref);
        entity_run_function_with_args(test, "entity_collision_stay", 1, entity->lua_val_ref);
    } else if (!currently_colliding && (was_colliding_a || was_colliding_b)) {
        // Collision Exit
        entity_run_function_with_args(entity, "entity_collision_exit", 1, test->lua_val_ref);
        entity_run_function_with_args(test, "entity_collision_exit", 1, entity->lua_val_ref);
    }


    if (currently_colliding) {
        hashmap_set(entity->current_collisions, canonical_key, (void*)1);
        hashmap_set(test->current_collisions, canonical_key, (void*)1);
    } else {
        hashmap_remove(entity->current_collisions, canonical_key);
        hashmap_remove(test->current_collisions, canonical_key);
    }
}

bool entity_detect_collision_rect(EseEntity *entity, EseRect *rect) {
    log_assert("ENTITY", entity, "entity_detect_collision_rect called with NULL entity");
    log_assert("ENTITY", rect, "entity_detect_collision_rect called with NULL rect");

    for (size_t i = 0; i < entity->component_count; i++) {
        EseEntityComponent *comp = entity->components[i];
        if (!comp->active || comp->type != ENTITY_COMPONENT_COLLIDER) {
            continue;
        }

        if (entity_component_detect_collision_rect(comp, rect)) {
            return true;
        }
    }

    return false;
}

void entity_draw(
    EseEntity *entity,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawTextureCallback texCallback,
    EntityDrawRectCallback rectCallback,
    void *callback_user_data
) {
    log_assert("ENTITY", entity, "entity_draw called with NULL entity");
    log_assert("ENTITY", texCallback, "entity_draw called with NULL texCallback");
    log_assert("ENTITY", rectCallback, "entity_draw called with NULL rectCallback");


    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active) continue;

        entity_component_draw(
            entity->components[i],
            camera_x, camera_y, view_width, view_height,
            texCallback, rectCallback, callback_user_data
        );
    }
}

const char *entity_component_add(EseEntity *entity, EseEntityComponent *comp) {
    log_assert("ENTITY", entity, "entity_component_add called with NULL entity");
    log_assert("ENTITY", comp, "entity_component_add called with NULL comp");

    if (entity->component_count == entity->component_capacity) {
        size_t new_capacity = entity->component_capacity * 2;
        EseEntityComponent **new_components = memory_manager.realloc(
            entity->components, 
            sizeof(EseEntityComponent*) * new_capacity, 
            MMTAG_ENTITY
        );
        entity->components = new_components;
        entity->component_capacity = new_capacity;
    }

    entity->components[entity->component_count++] = comp;
    comp->entity = entity;

    return comp->id->value;
}

bool entity_component_remove(EseEntity *entity, const char *id) {
    log_assert("ENTITY", entity, "entity_component_remove called with NULL entity");
    log_assert("ENTITY", id, "entity_component_remove called with NULL id");

    int idx = _entity_component_find_index(entity, id);
    if (idx < 0) {
        log_error("ENTITY", "entity_component_remove: component not found (id=%s)", id);
        return false;
    }

    entity_component_destroy(entity->components[idx]);
    entity->components[idx] = entity->components[entity->component_count - 1];
    entity->components[entity->component_count - 1] = NULL;
    entity->component_count--;

    return true;
}

bool entity_add_prop(EseEntity *entity, EseLuaValue *value) {
    log_assert("ENTITY", entity, "entity_add_prop called with NULL entity");
    log_assert("ENTITY", value, "entity_add_prop called with NULL value");

    if (entity->lua_ref == LUA_NOREF) {
        log_error("ENTITY", "entity_add_prop: entity has no Lua reference");
        return false;
    }

    const char *prop_name = lua_value_get_name(value);
    log_assert("ENTITY", prop_name, "entity_add_prop called with NULL value name");

    if (_entity_lua_to_data(entity, value)) {
        dlist_append(entity->default_props, value);
        return true;
    }
    
    return false;
}

int entity_get_lua_ref(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_component_remove called with NULL entity");
    return entity->lua_ref;
}
