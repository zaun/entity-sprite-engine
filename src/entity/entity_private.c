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

#define ENTITY_INITIAL_CAPACITY 10

EseEntity *_entity_make(EseLuaEngine *engine) {
    EseEntity *entity = memory_manager.malloc(sizeof(EseEntity), MMTAG_ENTITY);
    entity->position = point_create(engine);

    entity->id = uuid_create(engine);
    entity->active = true;
    entity->position->x = 0.0f;
    entity->position->y = 0.0f;
    entity->draw_order = 0;

    entity->current_collisions = hashmap_create();

    entity->components = memory_manager.malloc(sizeof(EseEntityComponent*) * ENTITY_INITIAL_CAPACITY, MMTAG_ENTITY);
    entity->component_capacity = ENTITY_INITIAL_CAPACITY;
    entity->component_count = 0;

    entity->lua = engine;
    entity->lua_ref = LUA_NOREF;

    entity->default_props = dlist_create((DListFreeFn)lua_value_free);

    return entity;
}

int _entity_component_find_index(EseEntity *entity, const char *id) {
    log_assert("ENTITY", entity, "_entity_component_find_index called with NULL entity");

    for (size_t i = 0; i < entity->component_count; ++i) {
        EseEntityComponent *comp = entity->components[i];
        if (strcmp(comp->id->value, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

const char* _get_collision_key(EseUUID* uuid1, EseUUID* uuid2) {
    // A static buffer to hold the string key.
    // This makes the function thread-unsafe, but given the context of a game loop,
    // this is likely a safe assumption.
    static char key_str[40]; // A buffer large enough for two UUIDs' hash strings.

    // Ensure consistent key by sorting UUIDs based on their value.
    if (strcmp(uuid1->value, uuid2->value) > 0) {
        EseUUID* temp = uuid1;
        uuid1 = uuid2;
        uuid2 = temp;
    }
    
    // Get the individual hashes.
    uint64_t h1 = uuid_hash(uuid1);
    uint64_t h2 = uuid_hash(uuid2);
    
    // A simple, robust way to combine two hashes for a single key.
    // This is more robust than a simple XOR.
    uint64_t combined_hash = h1 + (h2 << 6) + (h2 >> 2);

    // Format the combined hash into a string.
    // %llu is for unsigned long long, which is what uint64_t is.
    snprintf(key_str, sizeof(key_str), "%llu", combined_hash);

    return key_str;
}

bool _entity_test_collision(EseEntity *a, EseEntity *b) {
    log_assert("ENTITY", a, "entity_test_collision called with NULL a");
    log_assert("ENTITY", b, "entity_test_collision called with NULL b");

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
                return true;
            }
        }
    }

    return false;
}
