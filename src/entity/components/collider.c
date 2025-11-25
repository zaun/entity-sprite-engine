#include "entity/components/collider.h"
#include "core/asset_manager.h"
#include "core/collision_resolver.h"
#include "vendor/json/cJSON.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "scripting/lua_engine.h"
#include "types/rect.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <math.h>
#include <string.h>

#define COLLIDER_RECT_CAPACITY 5

bool _entity_component_collider_collides_component(EseEntityComponentCollider *colliderA,
                                                   EseEntityComponentCollider *colliderB,
                                                   EseArray *out_hits);

// VTable wrapper functions
static EseEntityComponent *_collider_vtable_copy(EseEntityComponent *component) {
    return entity_component_collider_copy((EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_destroy(EseEntityComponent *component) {
    entity_component_collider_destroy((EseEntityComponentCollider *)component->data);
}

static bool _collider_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                          const char *func_name, int argc, void *argv[]) {
    // Colliders don't support function execution
    return false;
}

static void _collider_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                                EseArray *out_hits) {
    _entity_component_collider_collides_component((EseEntityComponentCollider *)a->data,
                                                  (EseEntityComponentCollider *)b->data, out_hits);
}

static void _collider_vtable_ref(EseEntityComponent *component) {
    entity_component_collider_ref((EseEntityComponentCollider *)component->data);
}

static void _collider_vtable_unref(EseEntityComponent *component) {
    entity_component_collider_unref((EseEntityComponentCollider *)component->data);
}

static cJSON *_collider_vtable_serialize(EseEntityComponent *component) {
    return entity_component_collider_serialize((EseEntityComponentCollider *)component->data);
}

// Static vtable instance for collider components
static const ComponentVTable collider_vtable = {.copy = _collider_vtable_copy,
                                                .destroy = _collider_vtable_destroy,
                                                .run_function = _collider_vtable_run_function,
                                                .collides = _collider_vtable_collides_component,
                                                .ref = _collider_vtable_ref,
                                                .unref = _collider_vtable_unref,
                                                .serialize = _collider_vtable_serialize};

bool _entity_component_collider_collides_component(EseEntityComponentCollider *colliderA,
                                                   EseEntityComponentCollider *colliderB,
                                                   EseArray *out_hits) {
    log_assert("ENTITY_COMP", colliderA,
               "_entity_component_collider_collides_component called with NULL "
               "collider");
    log_assert("ENTITY_COMP", colliderB,
               "_entity_component_collider_collides_component called with NULL "
               "collider");

    profile_start(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES);

    float pos_a_x = ese_point_get_x(colliderA->base.entity->position);
    float pos_a_y = ese_point_get_y(colliderA->base.entity->position);
    float pos_b_x = ese_point_get_x(colliderB->base.entity->position);
    float pos_b_y = ese_point_get_y(colliderB->base.entity->position);

    for (size_t i = 0; i < colliderA->rects_count; i++) {
        EseRect *rect_a = colliderA->rects[i];

        // Create world-space rect for A
        EseRect *world_rect_a = ese_rect_create(colliderA->base.lua);
        ese_rect_set_x(world_rect_a,
                       ese_rect_get_x(rect_a) + ese_point_get_x(colliderA->offset) + pos_a_x);
        ese_rect_set_y(world_rect_a,
                       ese_rect_get_y(rect_a) + ese_point_get_y(colliderA->offset) + pos_a_y);
        ese_rect_set_width(world_rect_a, ese_rect_get_width(rect_a));
        ese_rect_set_height(world_rect_a, ese_rect_get_height(rect_a));
        ese_rect_set_rotation(world_rect_a, ese_rect_get_rotation(rect_a));

        for (size_t j = 0; j < colliderB->rects_count; j++) {
            EseRect *rect_b = colliderB->rects[j];

            // Create world-space rect for B
            EseRect *world_rect_b = ese_rect_create(colliderB->base.lua);
            ese_rect_set_x(world_rect_b,
                           ese_rect_get_x(rect_b) + ese_point_get_x(colliderB->offset) + pos_b_x);
            ese_rect_set_y(world_rect_b,
                           ese_rect_get_y(rect_b) + ese_point_get_y(colliderB->offset) + pos_b_y);
            ese_rect_set_width(world_rect_b, ese_rect_get_width(rect_b));
            ese_rect_set_height(world_rect_b, ese_rect_get_height(rect_b));
            ese_rect_set_rotation(world_rect_b, ese_rect_get_rotation(rect_b));

            // Use proper rotated rectangle intersection test
            if (ese_rect_intersects(world_rect_a, world_rect_b)) {
                profile_count_add("collider_pair_rect_tests_hit");
                EseCollisionHit *hit = ese_collision_hit_create(colliderA->base.entity->lua);
                ese_collision_hit_set_kind(hit, COLLISION_KIND_COLLIDER);
                ese_collision_hit_set_entity(hit, colliderA->base.entity);
                ese_collision_hit_set_target(hit, colliderB->base.entity);
                ese_collision_hit_set_state(hit, COLLISION_STATE_STAY);
                ese_collision_hit_set_rect(hit, colliderB->rects[j]);
                array_push(out_hits, hit);
                ese_rect_destroy(world_rect_b);
                ese_rect_destroy(world_rect_a);
                profile_stop(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES,
                             "entity_comp_collider_collides_comp");
                return true;
            }
            ese_rect_destroy(world_rect_b);
            profile_count_add("collider_pair_rect_tests_miss");
        }

        ese_rect_destroy(world_rect_a);
    }

    profile_stop(PROFILE_ENTITY_COMP_COLLIDER_COLLIDES, "entity_comp_collider_collides_comp");
    return false;
}

EseEntityComponent *entity_component_collider_make(EseLuaEngine *engine) {
    EseEntityComponentCollider *component =
        memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_COLLIDER;
    component->base.vtable = &collider_vtable;

    component->offset = ese_point_create(engine);
    ese_point_ref(component->offset);
    component->rects =
        memory_manager.malloc(sizeof(EseRect *) * COLLIDER_RECT_CAPACITY, MMTAG_ENTITY);
    component->rects_capacity = COLLIDER_RECT_CAPACITY;
    component->rects_count = 0;
    component->draw_debug = false;
    component->map_interaction = false;

    return &component->base;
}

EseEntityComponent *entity_component_collider_create(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "entity_component_collider_create called with NULL engine");

    EseEntityComponent *component = entity_component_collider_make(engine);

    // Register with Lua using ref system
    entity_component_collider_ref((EseEntityComponentCollider *)component->data);

    profile_count_add("entity_comp_collider_create_count");
    return component;
}

EseEntityComponent *entity_component_collider_copy(const EseEntityComponentCollider *src) {
    log_assert("ENTITY_COMP", src, "entity_component_collider_copy called with NULL src");

    EseEntityComponentCollider *copy =
        memory_manager.malloc(sizeof(EseEntityComponentCollider), MMTAG_ENTITY);
    copy->base.data = copy;
    copy->base.active = true;
    copy->base.id = ese_uuid_create(src->base.lua);
    copy->base.lua = src->base.lua;
    copy->base.lua_ref = LUA_NOREF;
    copy->base.lua_ref_count = 0;
    copy->base.type = ENTITY_COMPONENT_COLLIDER;
    copy->base.vtable = &collider_vtable;

    copy->offset = ese_point_copy(src->offset);
    ese_point_ref(copy->offset);

    // Copy rects
    copy->rects = memory_manager.malloc(sizeof(EseRect *) * src->rects_capacity, MMTAG_ENTITY);
    copy->rects_capacity = src->rects_capacity;
    copy->rects_count = src->rects_count;
    copy->draw_debug = src->draw_debug;
    copy->map_interaction = src->map_interaction;

    for (size_t i = 0; i < copy->rects_count; ++i) {
        EseRect *src_comp = src->rects[i];
        EseRect *dst_comp = ese_rect_copy(src_comp);
        copy->rects[i] = dst_comp;
    }

    return &copy->base;
}

static void _entity_component_collider_cleanup(EseEntityComponentCollider *component) {
    for (size_t i = 0; i < component->rects_count; ++i) {
        // Remove watcher before destroying rect
        ese_rect_remove_watcher(component->rects[i], entity_component_collider_rect_changed,
                                component);
        ese_rect_unref(component->rects[i]);
        ese_rect_destroy(component->rects[i]);
    }
    memory_manager.free(component->rects);

    ese_point_unref(component->offset);
    ese_point_destroy(component->offset);

    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_collider_destroy_count");
}

void entity_component_collider_destroy(EseEntityComponentCollider *component) {
    log_assert("ENTITY_COMP", component, "entity_component_collider_destroy called with NULL src");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_collider_cleanup(component);
        } else {
            // We dont "own" the collider so dont free it
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_collider_cleanup(component);
    }
}

void entity_component_collider_ref(EseEntityComponentCollider *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_collider_ref called with NULL component");

    if (component->base.lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseEntityComponentCollider **ud = (EseEntityComponentCollider **)lua_newuserdata(
            component->base.lua->runtime, sizeof(EseEntityComponentCollider *));
        *ud = component;

        // Attach metatable
        luaL_getmetatable(component->base.lua->runtime, ENTITY_COMPONENT_COLLIDER_PROXY_META);
        lua_setmetatable(component->base.lua->runtime, -2);

        // Store hard reference to prevent garbage collection
        component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
        component->base.lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        component->base.lua_ref_count++;
    }

    profile_count_add("entity_comp_collider_ref_count");
}

void entity_component_collider_unref(EseEntityComponentCollider *component) {
    if (!component)
        return;

    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;

        if (component->base.lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("entity_comp_collider_unref_count");
}

cJSON *entity_component_collider_serialize(const EseEntityComponentCollider *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_collider_serialize called with NULL component");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ENTITY_COMP", "Collider serialize: failed to create JSON object");
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "type", "ENTITY_COMPONENT_COLLIDER") ||
        !cJSON_AddBoolToObject(json, "active", component->base.active) ||
        !cJSON_AddBoolToObject(json, "draw_debug", component->draw_debug) ||
        !cJSON_AddBoolToObject(json, "map_interaction", component->map_interaction)) {
        log_error("ENTITY_COMP", "Collider serialize: failed to add fields");
        cJSON_Delete(json);
        return NULL;
    }

    // Offset
    cJSON *offset = cJSON_CreateObject();
    if (!offset) {
        log_error("ENTITY_COMP", "Collider serialize: failed to create offset object");
        cJSON_Delete(json);
        return NULL;
    }
    if (!cJSON_AddNumberToObject(offset, "x", (double)ese_point_get_x(component->offset)) ||
        !cJSON_AddNumberToObject(offset, "y", (double)ese_point_get_y(component->offset)) ||
        !cJSON_AddItemToObject(json, "offset", offset)) {
        log_error("ENTITY_COMP", "Collider serialize: failed to add offset");
        cJSON_Delete(offset);
        cJSON_Delete(json);
        return NULL;
    }

    // NOTE: rects array is not serialized yet; add later if needed.

    return json;
}

EseEntityComponent *entity_component_collider_deserialize(EseLuaEngine *engine,
                                                          const cJSON *data) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_collider_deserialize called with NULL engine");
    log_assert("ENTITY_COMP", data,
               "entity_component_collider_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ENTITY_COMP", "Collider deserialize: data is not an object");
        return NULL;
    }

    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type_item) ||
        strcmp(type_item->valuestring, "ENTITY_COMPONENT_COLLIDER") != 0) {
        log_error("ENTITY_COMP", "Collider deserialize: invalid or missing type");
        return NULL;
    }

    const cJSON *active_item = cJSON_GetObjectItemCaseSensitive(data, "active");
    const cJSON *draw_item = cJSON_GetObjectItemCaseSensitive(data, "draw_debug");
    const cJSON *map_item = cJSON_GetObjectItemCaseSensitive(data, "map_interaction");
    const cJSON *offset_item = cJSON_GetObjectItemCaseSensitive(data, "offset");
    const cJSON *off_x = offset_item ? cJSON_GetObjectItemCaseSensitive(offset_item, "x") : NULL;
    const cJSON *off_y = offset_item ? cJSON_GetObjectItemCaseSensitive(offset_item, "y") : NULL;

    EseEntityComponent *base = entity_component_collider_create(engine);
    if (!base) {
        log_error("ENTITY_COMP", "Collider deserialize: failed to create component");
        return NULL;
    }

    EseEntityComponentCollider *coll = (EseEntityComponentCollider *)base->data;
    if (cJSON_IsBool(active_item)) {
        coll->base.active = cJSON_IsTrue(active_item);
    }
    if (cJSON_IsBool(draw_item)) {
        coll->draw_debug = cJSON_IsTrue(draw_item);
    }
    if (cJSON_IsBool(map_item)) {
        coll->map_interaction = cJSON_IsTrue(map_item);
    }
    if (off_x && cJSON_IsNumber(off_x) && off_y && cJSON_IsNumber(off_y)) {
        ese_point_set_x(coll->offset, (float)off_x->valuedouble);
        ese_point_set_y(coll->offset, (float)off_y->valuedouble);
    }

    return base;
}

void entity_component_collider_rects_add(EseEntityComponentCollider *collider, EseRect *rect) {
    log_assert("ENTITY", collider, "entity_component_collider_rects_add called with NULL collider");
    log_assert("ENTITY", rect, "entity_component_collider_rects_add called with NULL rect");

    if (collider->rects_count == collider->rects_capacity) {
        size_t new_capacity = collider->rects_capacity * 2;
        EseRect **new_rects =
            memory_manager.realloc(collider->rects, sizeof(EseRect *) * new_capacity, MMTAG_ENTITY);
        collider->rects = new_rects;
        collider->rects_capacity = new_capacity;
    }

    collider->rects[collider->rects_count++] = rect;
    ese_rect_ref(rect);

    // Register a watcher to automatically update bounds when rect properties
    // change
    ese_rect_add_watcher(rect, entity_component_collider_rect_changed, collider);

    // Update entity's collision bounds after adding rect
    entity_component_collider_update_bounds(collider);
}

void entity_component_collider_rect_changed(EseRect *rect, void *userdata) {
    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)userdata;
    if (collider) {
        entity_component_collider_update_bounds(collider);
    }
}

void entity_component_collider_update_bounds(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider,
               "entity_component_collider_update_bounds called with NULL collider");

    // If component isn't attached to an entity yet, skip bounds update
    if (!collider->base.entity) {
        return;
    }

    if (collider->rects_count == 0) {
        // No rects, clear both collision bounds
        if (collider->base.entity->collision_bounds) {
            ese_rect_destroy(collider->base.entity->collision_bounds);
            collider->base.entity->collision_bounds = NULL;
        }
        if (collider->base.entity->collision_world_bounds) {
            ese_rect_destroy(collider->base.entity->collision_world_bounds);
            collider->base.entity->collision_world_bounds = NULL;
        }
        return;
    }

    // Compute bounds from all rects in this collider (relative to entity)
    float min_x = INFINITY, min_y = INFINITY, max_x = -INFINITY, max_y = -INFINITY;

    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *r = collider->rects[i];
        if (!r)
            continue;

        // Get rect properties
        float rx = ese_rect_get_x(r) + ese_point_get_x(collider->offset);
        float ry = ese_rect_get_y(r) + ese_point_get_y(collider->offset);
        float rw = ese_rect_get_width(r);
        float rh = ese_rect_get_height(r);
        float rotation = ese_rect_get_rotation(r);

        // Calculate the bounding box of the rotated rectangle
        if (fabsf(rotation) < 1e-6f) {
            // No rotation - simple AABB
            min_x = fminf(min_x, rx);
            min_y = fminf(min_y, ry);
            max_x = fmaxf(max_x, rx + rw);
            max_y = fmaxf(max_y, ry + rh);
        } else {
            // Rotated rectangle - calculate all 4 corners and find bounding box
            float cx = rx + rw * 0.5f; // Center X
            float cy = ry + rh * 0.5f; // Center Y
            float half_w = rw * 0.5f;
            float half_h = rh * 0.5f;

            float cos_r = cosf(rotation);
            float sin_r = sinf(rotation);

            // Calculate the 4 corners of the rotated rectangle
            float corners_x[4], corners_y[4];

            // Corner 1: (-half_w, -half_h)
            corners_x[0] = cx + cos_r * (-half_w) - sin_r * (-half_h);
            corners_y[0] = cy + sin_r * (-half_w) + cos_r * (-half_h);

            // Corner 2: (+half_w, -half_h)
            corners_x[1] = cx + cos_r * (half_w)-sin_r * (-half_h);
            corners_y[1] = cy + sin_r * (half_w) + cos_r * (-half_h);

            // Corner 3: (+half_w, +half_h)
            corners_x[2] = cx + cos_r * (half_w)-sin_r * (half_h);
            corners_y[2] = cy + sin_r * (half_w) + cos_r * (half_h);

            // Corner 4: (-half_w, +half_h)
            corners_x[3] = cx + cos_r * (-half_w) - sin_r * (half_h);
            corners_y[3] = cy + sin_r * (-half_w) + cos_r * (half_h);

            // Find the bounding box of all corners
            for (int j = 0; j < 4; j++) {
                min_x = fminf(min_x, corners_x[j]);
                min_y = fminf(min_y, corners_y[j]);
                max_x = fmaxf(max_x, corners_x[j]);
                max_y = fmaxf(max_y, corners_y[j]);
            }
        }
    }

    // Create or update the collision bounds (relative to entity)
    if (!collider->base.entity->collision_bounds) {
        collider->base.entity->collision_bounds = ese_rect_create(collider->base.lua);
        ese_rect_ref(collider->base.entity->collision_bounds);
    }

    EseRect *bounds = collider->base.entity->collision_bounds;
    ese_rect_set_x(bounds, min_x);
    ese_rect_set_y(bounds, min_y);
    ese_rect_set_width(bounds, max_x - min_x);
    ese_rect_set_height(bounds, max_y - min_y);
    ese_rect_set_rotation(bounds, 0.0f); // Collision bounds are axis-aligned

    // Create or update the collision world bounds
    if (!collider->base.entity->collision_world_bounds) {
        collider->base.entity->collision_world_bounds = ese_rect_create(collider->base.lua);
        ese_rect_ref(collider->base.entity->collision_world_bounds);
    }

    EseRect *world_bounds = collider->base.entity->collision_world_bounds;
    ese_rect_set_x(world_bounds, min_x + ese_point_get_x(collider->base.entity->position));
    ese_rect_set_y(world_bounds, min_y + ese_point_get_y(collider->base.entity->position));
    ese_rect_set_width(world_bounds, max_x - min_x);
    ese_rect_set_height(world_bounds, max_y - min_y);
    ese_rect_set_rotation(world_bounds,
                          0.0f); // Collision bounds are axis-aligned
}

void entity_component_collider_rect_updated(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider,
               "entity_component_collider_rect_updated called with NULL collider");

    // Simply call the bounds update function
    entity_component_collider_update_bounds(collider);
}

void entity_component_collider_position_changed(EseEntityComponentCollider *collider) {
    log_assert("ENTITY", collider,
               "entity_component_collider_position_changed called with NULL collider");

    // Update bounds since entity position affects all rect world positions
    entity_component_collider_update_bounds(collider);
}

bool entity_component_collider_get_draw_debug(EseEntityComponentCollider *collider) {
    log_assert("ENTITY_COMP", collider,
               "entity_component_collider_get_draw_debug called with NULL collider");

    return collider->draw_debug;
}

void entity_component_collider_set_draw_debug(EseEntityComponentCollider *collider,
                                              bool draw_debug) {
    log_assert("ENTITY_COMP", collider,
               "entity_component_collider_set_draw_debug called with NULL collider");

    collider->draw_debug = draw_debug;
}

bool entity_component_collider_get_map_interaction(EseEntityComponentCollider *collider) {
    log_assert("ENTITY_COMP", collider,
               "entity_component_collider_get_map_interaction called with NULL "
               "collider");
    return collider->map_interaction;
}

void entity_component_collider_set_map_interaction(EseEntityComponentCollider *collider,
                                                   bool enabled) {
    log_assert("ENTITY_COMP", collider,
               "entity_component_collider_set_map_interaction called with NULL "
               "collider");
    collider->map_interaction = enabled;
}
