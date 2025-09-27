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
#include "entity/components/entity_component_collider.h"
#include "utility/profile.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/pubsub.h"
#include "scripting/lua_engine.h"

EseEntity *entity_create(EseLuaEngine *engine) {
    log_assert("ENTITY", engine, "entity_create called with NULL engine");

    profile_start(PROFILE_ENTITY_CREATE);
    
    EseEntity *entity = _entity_make(engine);
    log_debug("ENTITY", "create %p", (void*)entity);
    entity_ref(entity); // C-created = C-owned
    
    profile_stop(PROFILE_ENTITY_CREATE, "entity_create");
    profile_count_add("entity_create_count");
    return entity;
}

EseEntity *entity_copy(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_copy called with NULL entity");
    
    profile_start(PROFILE_ENTITY_COPY);
    
    EseEntity *copy = _entity_make(entity->lua);
    entity_ref(copy); // C-created = C-owned

    // Copy all fields
    copy->active = entity->active;
    ese_point_set_x(copy->position, ese_point_get_x(entity->position));
    ese_point_set_y(copy->position, ese_point_get_y(entity->position));
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

    // subscriptions are not copied on purpose; start as NULL lazily
    copy->subscriptions = NULL;

    // Copy default props
    copy->default_props = entity->default_props ? dlist_copy(entity->default_props, (DListCopyFn)lua_value_copy) : NULL;

    // Apply copied default_props to the new entity's Lua __data table
    EseDListIter *iter = dlist_iter_create(copy->default_props);
    void *value_ptr;
    while (dlist_iter_next(iter, &value_ptr)) {
        EseLuaValue *value = (EseLuaValue*)value_ptr;
        _entity_lua_to_data(copy, value);
    }
    dlist_iter_free(iter);

    profile_stop(PROFILE_ENTITY_COPY, "entity_copy");
    profile_count_add("entity_copy_count");
    return copy;
}

static void _entity_cleanup(EseEntity *entity) {
    // Note: assumes Lua registry ref is already cleared or was never set
    ese_uuid_unref(entity->id);
    ese_uuid_destroy(entity->id);
    ese_point_unref(entity->position);
    ese_point_destroy(entity->position);

    hashmap_free(entity->current_collisions);
    hashmap_free(entity->previous_collisions);
    if (entity->collision_bounds) {
        ese_rect_destroy(entity->collision_bounds);
    }
    if (entity->collision_world_bounds) {
        ese_rect_destroy(entity->collision_world_bounds);
    }

    for (size_t i = 0; i < entity->component_count; ++i) {
        entity_component_destroy(entity->components[i]);
    }
    memory_manager.free(entity->components);

    // Clean up tags
    for (size_t i = 0; i < entity->tag_count; ++i) {
        memory_manager.free(entity->tags[i]);
    }
    memory_manager.free(entity->tags);

    // Clean up pub/sub subscriptions
    if (entity->subscriptions) {
        // Get engine to unsubscribe from pub/sub system
        EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(entity->lua->runtime, ENGINE_KEY);
        if (engine) {
            size_t count = array_size(entity->subscriptions);
            for (size_t i = 0; i < count; i++) {
                EseEntitySubscription *sub = array_get(entity->subscriptions, i);
                if (sub) {
                    engine_pubsub_unsub(engine, sub->topic_name, entity, sub->function_name);
                }
            }
        }
        array_destroy(entity->subscriptions);
        entity->subscriptions = NULL;
    }

    if (entity->lua_val_ref) {
        lua_value_free(entity->lua_val_ref);
    }

    if (entity->default_props) {
        dlist_free(entity->default_props);
    }
    log_debug("ENTITY", "free %p", (void*)entity);
    memory_manager.free(entity);
    profile_count_add("entity_destroy_count");
}

void entity_destroy(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_destroy called with NULL entity");

    // Respect Lua registry ref-count; only free when no refs remain
    if (entity->lua_ref != LUA_NOREF && entity->lua_ref_count > 0) {
        entity->lua_ref_count--;
        if (entity->lua_ref_count == 0) {
            luaL_unref(entity->lua->runtime, LUA_REGISTRYINDEX, entity->lua_ref);
            entity->lua_ref = LUA_NOREF;
            _entity_cleanup(entity);
        } else {
            // Still referenced in Lua; do not free now
            profile_stop(PROFILE_ENTITY_DESTROY, "entity_destroy (deferred)");
            profile_count_add("entity_destroy_deferred_count");
            return;
        }
    } else if (entity->lua_ref == LUA_NOREF) {
        _entity_cleanup(entity);
    }
}

void entity_update(EseEntity *entity, float delta_time) {
    log_assert("ENTITY", entity, "entity_update called with NULL entity");

    profile_start(PROFILE_ENTITY_UPDATE_OVERALL);
    
    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active) {
            continue;
        }
        
        profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);
        entity_component_update(entity->components[i], entity, delta_time);
        profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_update");
    }
    
    profile_stop(PROFILE_ENTITY_UPDATE_OVERALL, "entity_update");
}

void entity_set_position(EseEntity *entity, float x, float y) {
    log_assert("ENTITY", entity, "entity_set_position called with NULL entity");
    
    ese_point_set_x(entity->position, x);
    ese_point_set_y(entity->position, y);
    
    // Update collision bounds for all collider components
    for (size_t i = 0; i < entity->component_count; i++) {
        EseEntityComponent *comp = entity->components[i];
        if (comp->active && comp->type == ENTITY_COMPONENT_COLLIDER) {
            entity_component_collider_position_changed((EseEntityComponentCollider *)comp->data);
        }
    }
}

void entity_run_function_with_args(
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv[]
) {
    profile_start(PROFILE_ENTITY_LUA_FUNCTION_CALL);
    
    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active || entity->components[i]->type != ENTITY_COMPONENT_LUA) {
            continue;
        }
        entity_component_run_function(entity->components[i], entity, func_name, argc, argv);
    }
    
    profile_stop(PROFILE_ENTITY_LUA_FUNCTION_CALL, "entity_run_function_with_args");
}

int entity_check_collision_state(EseEntity *entity, EseEntity *test) {
    log_assert("ENTITY", entity, "entity_check_collision_state called with NULL entity");
    log_assert("ENTITY", test, "entity_check_collision_state called with NULL test");

    profile_start(PROFILE_ENTITY_COLLISION_DETECT);

    // Get the key.
    const char* canonical_key = _get_collision_key(entity->id, test->id);

    // Read the state from the PREVIOUS frame - check if this pair was colliding
    // We only need to check one entity's collision state since both should be in sync
    // Consider both previous and current maps so tests that do not swap frames still see STAY/EXIT
    bool was_colliding =
        hashmap_get(entity->previous_collisions, canonical_key) != NULL ||
        hashmap_get(entity->current_collisions, canonical_key) != NULL;
    bool currently_colliding = _entity_test_collision(entity, test);

    // Determine collision state: 0=none, 1=enter, 2=stay, 3=exit
    int result;
    if (currently_colliding && !was_colliding) {
        result = 1; // ENTER
    } else if (currently_colliding && was_colliding) {
        result = 2; // STAY
    } else if (!currently_colliding && was_colliding) {
        result = 3; // EXIT
    } else {
        result = 0; // NONE
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_DETECT, "entity_check_collision_state");
    memory_manager.free((void*)canonical_key);
    return result;
}

void entity_clear_collision_states(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_clear_collision_states called with NULL entity");
    
    // Swap current and previous collision states
    EseHashMap *temp = entity->previous_collisions;
    entity->previous_collisions = entity->current_collisions;
    entity->current_collisions = temp;
    
    // Clear the new current_collisions (which was previous_collisions)
    hashmap_clear(entity->current_collisions);
}

void entity_process_collision_callbacks(EseEntity *entity_a, EseEntity *entity_b, int state) {
    log_assert("ENTITY", entity_a, "entity_process_collision_callbacks called with NULL entity_a");
    log_assert("ENTITY", entity_b, "entity_process_collision_callbacks called with NULL entity_b");

    profile_start(PROFILE_ENTITY_COLLISION_CALLBACK);

    // Get the key for updating collision state
    const char* canonical_key = _get_collision_key(entity_a->id, entity_b->id);

    switch (state) {
        case 1: // ENTER
            // Collision Enter
            {
                EseLuaValue *args[] = {entity_b->lua_val_ref};
                entity_run_function_with_args(entity_a, "entity_collision_enter", 1, args);
            }
            {
                EseLuaValue *args[] = {entity_a->lua_val_ref};
                entity_run_function_with_args(entity_b, "entity_collision_enter", 1, args);
            }
            
            // Update collision state
            hashmap_set(entity_a->current_collisions, canonical_key, (void*)1);
            hashmap_set(entity_b->current_collisions, canonical_key, (void*)1);
            
            break;
            
        case 2: // STAY
            // Collision Stay
            {
                EseLuaValue *args[] = {entity_b->lua_val_ref};
                entity_run_function_with_args(entity_a, "entity_collision_stay", 1, args);
            }
            {
                EseLuaValue *args[] = {entity_a->lua_val_ref};
                entity_run_function_with_args(entity_b, "entity_collision_stay", 1, args);
            }

            // Maintain collision in current_collisions to indicate ongoing contact
            hashmap_set(entity_a->current_collisions, canonical_key, (void*)1);
            hashmap_set(entity_b->current_collisions, canonical_key, (void*)1);
            break;
            
        case 3: // EXIT
            // Collision Exit
            {
                EseLuaValue *args[] = {entity_b->lua_val_ref};
                entity_run_function_with_args(entity_a, "entity_collision_exit", 1, args);
            }
            {
                EseLuaValue *args[] = {entity_a->lua_val_ref};
                entity_run_function_with_args(entity_b, "entity_collision_exit", 1, args);
            }
            
            // Update collision state
            hashmap_remove(entity_a->current_collisions, canonical_key);
            hashmap_remove(entity_b->current_collisions, canonical_key);
            break;
            
        case 0: // NONE
        default:
            // No collision, nothing to do
            break;
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_CALLBACK, "entity_process_collision_callbacks");
    memory_manager.free((void*)canonical_key);
}

bool entity_detect_collision_rect(EseEntity *entity, EseRect *rect) {
    log_assert("ENTITY", entity, "entity_detect_collision_rect called with NULL entity");
    log_assert("ENTITY", rect, "entity_detect_collision_rect called with NULL rect");

    profile_start(PROFILE_ENTITY_COLLISION_RECT_DETECT);

    for (size_t i = 0; i < entity->component_count; i++) {
        EseEntityComponent *comp = entity->components[i];
        if (!comp->active || comp->type != ENTITY_COMPONENT_COLLIDER) {
            continue;
        }

        if (entity_component_detect_collision_rect(comp, rect)) {
            profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_detect_collision_rect");
            return true;
        }
    }

    profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_detect_collision_rect");
    return false;
}

void entity_draw(
    EseEntity *entity,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawCallbacks *callbacks,
    void *callback_user_data
) {
    log_assert("ENTITY", entity, "entity_draw called with NULL entity");
    log_assert("ENTITY", callbacks, "entity_draw called with NULL callbacks");
    log_assert("ENTITY", callbacks->draw_texture, "entity_draw called with NULL draw_texture callback");
    log_assert("ENTITY", callbacks->draw_rect, "entity_draw called with NULL draw_rect callback");
    log_assert("ENTITY", callbacks->draw_polyline, "entity_draw called with NULL draw_polyline callback");

    profile_start(PROFILE_ENTITY_DRAW_OVERALL);

    for (size_t i = 0; i < entity->component_count; i++) {
        if (!entity->components[i]->active) continue;

        profile_start(PROFILE_ENTITY_DRAW_SECTION);
        entity_component_draw(
            entity->components[i],
            camera_x, camera_y, view_width, view_height,
            callbacks, callback_user_data
        );
        profile_stop(PROFILE_ENTITY_DRAW_SECTION, "entity_component_draw");
    }
    
    profile_stop(PROFILE_ENTITY_DRAW_OVERALL, "entity_draw");
}

const char *entity_component_add(EseEntity *entity, EseEntityComponent *comp) {
    log_assert("ENTITY", entity, "entity_component_add called with NULL entity");
    log_assert("ENTITY", comp, "entity_component_add called with NULL comp");

    profile_start(PROFILE_ENTITY_COMPONENT_ADD);

    // Lazily allocate components array on first add
    if (entity->component_capacity == 0) {
        size_t initial_capacity = 10; // matches ENTITY_INITIAL_CAPACITY previously used
        entity->components = memory_manager.malloc(
            sizeof(EseEntityComponent*) * initial_capacity,
            MMTAG_ENTITY
        );
        entity->component_capacity = initial_capacity;
        entity->component_count = 0;
    }

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

    // If this is a collider, initialize bounds now that the entity pointer is set
    if (comp->type == ENTITY_COMPONENT_COLLIDER) {
        entity_component_collider_update_bounds((EseEntityComponentCollider *)comp->data);
    }

    profile_stop(PROFILE_ENTITY_COMPONENT_ADD, "entity_component_add");
    profile_count_add("entity_comp_add_count");
    return ese_uuid_get_value(comp->id);
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

size_t entity_component_count(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_component_count called with NULL entity");
    return entity->component_count;
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
        if (!entity->default_props) {
            entity->default_props = dlist_create((DListFreeFn)lua_value_free);
        }
        dlist_append(entity->default_props, value);
        return true;
    }
    
    return false;
}

int entity_get_lua_ref(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_get_lua_ref called with NULL entity");
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

EseRect *entity_get_collision_bounds(EseEntity *entity, bool to_world_coords) {
    log_assert("ENTITY", entity, "entity_get_collision_bounds called with NULL entity");
    
    // If no collision bounds exist, return NULL
    if (!entity->collision_bounds) {
        return NULL;
    }
    
    if (to_world_coords) {
        // Use the pre-computed world bounds if available
        if (entity->collision_world_bounds) {
            return ese_rect_copy(entity->collision_world_bounds);
        } else {
            // Fallback: create a copy of the bounds in world coordinates
            EseRect *world_bounds = ese_rect_copy(entity->collision_bounds);
            if (world_bounds) {
                ese_rect_set_x(world_bounds, ese_rect_get_x(world_bounds) + ese_point_get_x(entity->position));
                ese_rect_set_y(world_bounds, ese_rect_get_y(world_bounds) + ese_point_get_y(entity->position));
            }
            return world_bounds;
        }
    } else {
        // Always return a copy for consistency - caller must free both cases
        return ese_rect_copy(entity->collision_bounds);
    }
}

void entity_set_visible(EseEntity *entity, bool visible) {
    log_assert("ENTITY", entity, "entity_set_visible called with NULL entity");
    entity->visible = visible;
}

bool entity_get_visible(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_get_visible called with NULL entity");
    return entity->visible;
}

void entity_set_persistent(EseEntity *entity, bool persistent) {
    log_assert("ENTITY", entity, "entity_set_persistent called with NULL entity");
    entity->persistent = persistent;
}

bool entity_get_persistent(EseEntity *entity) {
    log_assert("ENTITY", entity, "entity_get_persistent called with NULL entity");
    return entity->persistent;
}