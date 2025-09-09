#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "utility/double_linked_list.h"
#include "utility/log.h"
#include "types/types.h"
#include "entity/components/entity_component_private.h"
#include "scripting/lua_value.h"
#include "entity/entity_lua.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "utility/profile.h"

#define ENTITY_INITIAL_CAPACITY 10

EseEntity *_entity_make(EseLuaEngine *engine) {
    profile_start(PROFILE_ENTITY_CREATE);
    
    EseEntity *entity = memory_manager.malloc(sizeof(EseEntity), MMTAG_ENTITY);
    entity->position = ese_point_create(engine);
    ese_point_ref(entity->position);

    entity->id = ese_uuid_create(engine);
    ese_uuid_ref(entity->id);
    entity->active = true;
    entity->visible = true;
    entity->persistent = false;
    entity->destroyed = false;
    ese_point_set_x(entity->position, 0.0f);
    ese_point_set_y(entity->position, 0.0f);
    entity->draw_order = 0;

    // Not storing any values, so no free function needed
    entity->current_collisions = hashmap_create(NULL);
    entity->previous_collisions = hashmap_create(NULL);
    entity->collision_bounds = NULL;
    entity->collision_world_bounds = NULL;

    entity->components = memory_manager.malloc(sizeof(EseEntityComponent*) * ENTITY_INITIAL_CAPACITY, MMTAG_ENTITY);
    entity->component_capacity = ENTITY_INITIAL_CAPACITY;
    entity->component_count = 0;

    entity->lua = engine;
    entity->lua_ref = LUA_NOREF;
    entity->lua_val_ref = lua_value_create_nil("entity self ref");

    entity->default_props = dlist_create((DListFreeFn)lua_value_free);

    // Initialize tag system
    entity->tags = NULL;
    entity->tag_count = 0;
    entity->tag_capacity = 0;

    profile_stop(PROFILE_ENTITY_CREATE, "entity_make");
    profile_count_add("entity_make_count");
    return entity;
}

int _entity_component_find_index(EseEntity *entity, const char *id) {
    log_assert("ENTITY", entity, "_entity_component_find_index called with NULL entity");

    for (size_t i = 0; i < entity->component_count; ++i) {
        EseEntityComponent *comp = entity->components[i];
        if (strcmp(ese_uuid_get_value(comp->id), id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

const char* _get_collision_key(EseUUID *a, EseUUID *b) {
    const char* ida = ese_uuid_get_value(a);
    const char* idb = ese_uuid_get_value(b);
    const char* first = ida;
    const char* second = idb;
    if (strcmp(ida, idb) > 0) { first = idb; second = ida; }
    size_t keylen = strlen(first) + 1 + strlen(second) + 1;
    char *key = memory_manager.malloc(keylen, MMTAG_ENGINE);
    snprintf(key, keylen, "%s|%s", first, second);
    return key; // NOTE: caller (hashmap_set) MUST take ownership and free later
}

bool _entity_test_collision(EseEntity *a, EseEntity *b) {
    log_assert("ENTITY", a, "entity_test_collision called with NULL a");
    log_assert("ENTITY", b, "entity_test_collision called with NULL b");

    profile_start(PROFILE_ENTITY_COLLISION_TEST);

    for (size_t i = 0; i < a->component_count; i++) {
        EseEntityComponent *comp_a = a->components[i];
        if (!comp_a->active || comp_a->type != ENTITY_COMPONENT_COLLIDER) {
            continue;
        }

        for (size_t j = 0; j < b->component_count; j++) {
            EseEntityComponent *comp_b = b->components[j];
            if (!comp_b->active || comp_b->type != ENTITY_COMPONENT_COLLIDER) {
                continue;
            }

            if (entity_component_detect_collision_component(comp_a, comp_b)) {
                profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_test_collision");
                return true;
            }
        }
    }

    profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_test_collision");
    return false;
}
