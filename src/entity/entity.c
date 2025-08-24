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

    // Copy tags
    if (entity->tag_count > 0) {
        copy->tags = memory_manager.malloc(sizeof(char*) * entity->tag_capacity, MMTAG_ENTITY);
        copy->tag_capacity = entity->tag_capacity;
        copy->tag_count = entity->tag_count;
        
        for (size_t i = 0; i < entity->tag_count; ++i) {
            copy->tags[i] = memory_manager.strdup(entity->tags[i], MMTAG_ENTITY);
        }
    } else {
        copy->tags = NULL;
        copy->tag_count = 0;
        copy->tag_capacity = 0;
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

    // Clean up tags
    for (size_t i = 0; i < entity->tag_count; ++i) {
        memory_manager.free(entity->tags[i]);
    }
    memory_manager.free(entity->tags);

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

// Tag management functions

/**
 * @brief Helper function to capitalize a string and truncate to MAX_TAG_LENGTH
 */
static void _normalize_tag(char *dest, const char *src) {
    size_t i = 0;
    while (src[i] && i < MAX_TAG_LENGTH - 1) {
        if (src[i] >= 'a' && src[i] <= 'z') {
            dest[i] = src[i] - 32; // Convert to uppercase
        } else {
            dest[i] = src[i];
        }
        i++;
    }
    dest[i] = '\0';
}

bool entity_add_tag(EseEntity *entity, const char *tag) {
    log_assert("ENTITY", entity, "entity_add_tag called with NULL entity");
    log_assert("ENTITY", tag, "entity_add_tag called with NULL tag");

    // Check if tag already exists
    if (entity_has_tag(entity, tag)) {
        return false;
    }

    // Expand tag array if needed
    if (entity->tag_count == entity->tag_capacity) {
        size_t new_capacity = entity->tag_capacity == 0 ? 4 : entity->tag_capacity * 2;
        char **new_tags = memory_manager.realloc(
            entity->tags, 
            sizeof(char*) * new_capacity, 
            MMTAG_ENTITY
        );
        if (!new_tags) {
            log_error("ENTITY", "entity_add_tag: failed to allocate memory for tags");
            return false;
        }
        entity->tags = new_tags;
        entity->tag_capacity = new_capacity;
    }

    // Allocate and normalize the tag
    char *normalized_tag = memory_manager.malloc(MAX_TAG_LENGTH, MMTAG_ENTITY);
    if (!normalized_tag) {
        log_error("ENTITY", "entity_add_tag: failed to allocate memory for tag string");
        return false;
    }

    _normalize_tag(normalized_tag, tag);
    entity->tags[entity->tag_count++] = normalized_tag;

    return true;
}

bool entity_remove_tag(EseEntity *entity, const char *tag) {
    log_assert("ENTITY", entity, "entity_remove_tag called with NULL entity");
    log_assert("ENTITY", tag, "entity_remove_tag called with NULL tag");

    char normalized_tag[MAX_TAG_LENGTH];
    _normalize_tag(normalized_tag, tag);

    for (size_t i = 0; i < entity->tag_count; i++) {
        if (strcmp(entity->tags[i], normalized_tag) == 0) {
            // Free the tag string
            memory_manager.free(entity->tags[i]);
            
            // Shift remaining tags down
            for (size_t j = i; j < entity->tag_count - 1; j++) {
                entity->tags[j] = entity->tags[j + 1];
            }
            entity->tag_count--;
            return true;
        }
    }

    return false;
}

bool entity_has_tag(EseEntity *entity, const char *tag) {
    log_assert("ENTITY", entity, "entity_has_tag called with NULL entity");
    log_assert("ENTITY", tag, "entity_has_tag called with NULL tag");

    char normalized_tag[MAX_TAG_LENGTH];
    _normalize_tag(normalized_tag, tag);

    for (size_t i = 0; i < entity->tag_count; i++) {
        if (strcmp(entity->tags[i], normalized_tag) == 0) {
            return true;
        }
    }

    return false;
}
