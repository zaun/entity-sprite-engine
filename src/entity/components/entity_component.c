#include "entity/components/entity_component.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component_text.h"
#include "entity/entity_private.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lauxlib.h"
#include <stdbool.h>
#include <string.h>

void entity_component_lua_init(EseLuaEngine *engine) {
    profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);

    _entity_component_collider_init(engine);
    _entity_component_lua_init(engine);
    _entity_component_map_init(engine);
    _entity_component_shape_init(engine);
    _entity_component_sprite_init(engine);
    _entity_component_text_init(engine);

    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_lua_init");
}

EseEntityComponent *entity_component_copy(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_copy called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_COPY);

    EseEntityComponent *result = component->vtable->copy(component);

    profile_stop(PROFILE_ENTITY_COMPONENT_COPY, "entity_component_copy");
    profile_count_add("entity_comp_copy_count");
    return result;
}

void entity_component_destroy(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_destroy called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_DESTROY);

    component->vtable->destroy(component);

    profile_stop(PROFILE_ENTITY_COMPONENT_DESTROY, "entity_component_destroy");
    profile_count_add("entity_comp_destroy_count");
}

void entity_component_push(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_push called with NULL component");
    log_assert("ENTITY_COMP", component->lua_ref != LUA_NOREF,
               "entity_component_push component not registered with lua");

    profile_start(PROFILE_ENTITY_LUA_PROPERTY_ACCESS);

    // Push the userdata/proxy back onto the stack for Lua to receive
    lua_rawgeti(component->lua->runtime, LUA_REGISTRYINDEX, component->lua_ref);

    profile_stop(PROFILE_ENTITY_LUA_PROPERTY_ACCESS, "entity_component_push");
}

void entity_component_update(EseEntityComponent *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "entity_component_update called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);

    if (component->vtable->update) {
        component->vtable->update(component, entity, delta_time);
    }

    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_update");
}

void entity_component_detect_collision_with_component(EseEntityComponent *a, EseEntityComponent *b,
                                                      EseArray *out_hits) {
    log_assert("ENTITY_COMP", a,
               "entity_component_detect_collision_with_component called with NULL a");
    log_assert("ENTITY_COMP", b,
               "entity_component_detect_collision_with_component called with NULL b");
    log_assert("ENTITY_COMP", out_hits,
               "entity_component_detect_collision_with_component called with "
               "NULL out_hits");

    // Both components must be active
    if (!a->active || !b->active) {
        return;
    }

    // Test collider vs collider
    if (a->type == ENTITY_COMPONENT_COLLIDER && b->type == ENTITY_COMPONENT_COLLIDER) {
        profile_count_add("dispatch_collider_vs_collider");
        a->vtable->collides(a, b, out_hits);
        return;
    }

    // Test collider vs map
    if (a->type == ENTITY_COMPONENT_COLLIDER && b->type == ENTITY_COMPONENT_MAP &&
        ((EseEntityComponentCollider *)(a->data))->map_interaction) {
        profile_count_add("dispatch_collider_vs_map");
        b->vtable->collides(b, a, out_hits);
        return;
    }

    // Test map vs collider
    if (a->type == ENTITY_COMPONENT_MAP && b->type == ENTITY_COMPONENT_COLLIDER &&
        ((EseEntityComponentCollider *)(b->data))->map_interaction) {
        profile_count_add("dispatch_map_vs_collider");
        a->vtable->collides(a, b, out_hits);
        return;
    }

    // Nothing to test
    return;
}

bool entity_component_detect_collision_rect(EseEntityComponent *component, EseRect *rect) {
    log_assert("ENTITY_COMP", component,
               "entity_component_detect_collision_rect called with NULL component");
    log_assert("ENTITY_COMP", rect, "entity_component_detect_collision_rect called with NULL rect");

    profile_start(PROFILE_ENTITY_COLLISION_RECT_DETECT);

    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *colliderRect = collider->rects[i];
        EseRect *worldRect = ese_rect_copy(colliderRect);
        ese_rect_set_x(worldRect,
                       ese_rect_get_x(worldRect) + ese_point_get_x(component->entity->position));
        ese_rect_set_y(worldRect,
                       ese_rect_get_y(worldRect) + ese_point_get_y(component->entity->position));
        if (ese_rect_intersects(worldRect, rect)) {
            ese_rect_destroy(worldRect);
            profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_coll_rect");
            return true;
        }
        ese_rect_destroy(worldRect);
    }

    profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_coll_rect");
    return false;
}

bool entity_component_run_function(EseEntityComponent *component, EseEntity *entity,
                                   const char *func_name, int argc, EseLuaValue *argv[]) {
    log_assert("ENTITY_COMP", component,
               "entity_component_run_function called with NULL component");
    log_assert("ENTITY_COMP", entity, "entity_component_run_function called with NULL entity");

    profile_start(PROFILE_ENTITY_LUA_FUNCTION_CALL);

    bool result =
        component->vtable->run_function(component, entity, func_name, argc, (void **)argv);

    profile_stop(PROFILE_ENTITY_LUA_FUNCTION_CALL, "entity_component_run_function");
    return result;
}

void *entity_component_get_data(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_get_type called with NULL component");

    return component->data;
}

EseEntityComponent *entity_component_get(lua_State *L) {
    log_assert("ENTITY_COMP", L, "entity_component_get called with NULL L");

    // Handle userdata - check if it's a collider component
    EseEntityComponentCollider **ud_collider =
        (EseEntityComponentCollider **)luaL_testudata(L, 1, ENTITY_COMPONENT_COLLIDER_PROXY_META);
    if (ud_collider) {
        return &(*ud_collider)->base;
    }

    // Handle userdata - check if it's a lua component
    EseEntityComponentLua **ud_lua =
        (EseEntityComponentLua **)luaL_testudata(L, 1, ENTITY_COMPONENT_LUA_PROXY_META);
    if (ud_lua) {
        return &(*ud_lua)->base;
    }

    // Handle userdata - check if it's a map component
    EseEntityComponentMap **ud_map =
        (EseEntityComponentMap **)luaL_testudata(L, 1, ENTITY_COMPONENT_MAP_PROXY_META);
    if (ud_map) {
        return &(*ud_map)->base;
    }

    // Handle userdata - check if it's a shape component
    EseEntityComponentShape **ud_shape =
        (EseEntityComponentShape **)luaL_testudata(L, 1, ENTITY_COMPONENT_SHAPE_PROXY_META);
    if (ud_shape) {
        return &(*ud_shape)->base;
    }

    // Handle userdata - check if it's a sprite component
    EseEntityComponentSprite **ud_sprite =
        (EseEntityComponentSprite **)luaL_testudata(L, 1, ENTITY_COMPONENT_SPRITE_PROXY_META);
    if (ud_sprite) {
        return &(*ud_sprite)->base;
    }

    // Handle userdata - check if it's a text component
    EseEntityComponentText **ud_text =
        (EseEntityComponentText **)luaL_testudata(L, 1, ENTITY_COMPONENT_TEXT_PROXY_META);
    if (ud_text) {
        return &(*ud_text)->base;
    }

    luaL_argerror(L, 1, "expected a component userdata, got unknown userdata type");
    return NULL;
}
