#include <stdbool.h>
#include <string.h>
#include "entity/entity_private.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"
#include "vendor/lua/src/lauxlib.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_shape.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component_text.h"
#include "entity/components/entity_component.h"
#include "utility/profile.h"

void entity_component_lua_init(EseLuaEngine *engine) {
    profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);
    
    _entity_component_collider_init(engine);
    _entity_component_lua_init(engine);
    _entity_component_ese_map_init(engine);
    _entity_component_shape_init(engine);
    _entity_component_sprite_init(engine);
    _entity_component_text_init(engine);
    
    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_lua_init");
}

EseEntityComponent *entity_component_copy(EseEntityComponent* component) {
    log_assert("ENTITY_COMP", component, "entity_component_copy called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_COPY);

    EseEntityComponent *result = component->vtable->copy(component);

    profile_stop(PROFILE_ENTITY_COMPONENT_COPY, "entity_component_copy");
    profile_count_add("entity_comp_copy_count");
    return result;
}

void entity_component_destroy(EseEntityComponent* component) {
    log_assert("ENTITY_COMP", component, "entity_component_destroy called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_DESTROY);

    if (component->lua_ref != LUA_NOREF) {
        luaL_unref(component->lua->runtime, LUA_REGISTRYINDEX, component->lua_ref);
    }
    
    component->vtable->destroy(component);
    
    profile_stop(PROFILE_ENTITY_COMPONENT_DESTROY, "entity_component_destroy");
    profile_count_add("entity_comp_destroy_count");
}

void entity_component_push(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_push called with NULL component");
    log_assert("ENTITY_COMP", component->lua_ref != LUA_NOREF, "entity_component_push component not registered with lua");

    profile_start(PROFILE_ENTITY_LUA_PROPERTY_ACCESS);

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(component->lua->runtime, LUA_REGISTRYINDEX, component->lua_ref);
    
    profile_stop(PROFILE_ENTITY_LUA_PROPERTY_ACCESS, "entity_component_push");
}

void entity_component_update(EseEntityComponent *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "entity_component_update called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);

    component->vtable->update(component, entity, delta_time);
    
    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_update");
}

bool entity_component_detect_collision_component(EseEntityComponent *a, EseEntityComponent *b) {
    log_assert("ENTITY_COMP", a, "entity_component_detect_collision_component called with NULL a");
    log_assert("ENTITY_COMP", b, "entity_component_detect_collision_component called with NULL b");

    profile_start(PROFILE_ENTITY_COLLISION_TEST);

    if (a->type != ENTITY_COMPONENT_COLLIDER || b->type != ENTITY_COMPONENT_COLLIDER) {
        profile_cancel(PROFILE_ENTITY_COLLISION_TEST);
        return false;
    }

    EseEntityComponentCollider *colliderA = (EseEntityComponentCollider *)a->data;
    EseEntityComponentCollider *colliderB = (EseEntityComponentCollider *)b->data;

    // Get entity positions once
    float pos_a_x = ese_point_get_x(a->entity->position);
    float pos_a_y = ese_point_get_y(a->entity->position);
    float pos_b_x = ese_point_get_x(b->entity->position);
    float pos_b_y = ese_point_get_y(b->entity->position);

    for (size_t i = 0; i < colliderA->rects_count; i++) {
        EseRect *rect_a = colliderA->rects[i];
        float a_x = ese_rect_get_x(rect_a) + pos_a_x;
        float a_y = ese_rect_get_y(rect_a) + pos_a_y;
        float a_w = ese_rect_get_width(rect_a);
        float a_h = ese_rect_get_height(rect_a);
        
        for (size_t j = 0; j < colliderB->rects_count; j++) {
            EseRect *rect_b = colliderB->rects[j];
            float b_x = ese_rect_get_x(rect_b) + pos_b_x;
            float b_y = ese_rect_get_y(rect_b) + pos_b_y;
            float b_w = ese_rect_get_width(rect_b);
            float b_h = ese_rect_get_height(rect_b);
            
            // Direct AABB intersection test without creating rect objects
            if (a_x < b_x + b_w && a_x + a_w > b_x && a_y < b_y + b_h && a_y + a_h > b_y) {
                profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_component_detect_collision");
                return true;
            }
        }
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_component_detect_collision");
    return false;
}

bool entity_component_detect_collision_rect(EseEntityComponent *component, EseRect *rect) {
    log_assert("ENTITY_COMP", component, "entity_component_detect_collision_rect called with NULL component");
    log_assert("ENTITY_COMP", rect, "entity_component_detect_collision_rect called with NULL rect");

    profile_start(PROFILE_ENTITY_COLLISION_RECT_DETECT);

    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *colliderRect = ese_rect_copy(collider->rects[i]);
        ese_rect_set_x(colliderRect, ese_rect_get_x(colliderRect) + ese_point_get_x(component->entity->position));
        ese_rect_set_y(colliderRect, ese_rect_get_y(colliderRect) + ese_point_get_y(component->entity->position));
        if (ese_rect_intersects(colliderRect, rect)) {
            ese_rect_destroy(colliderRect);
            profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_coll_rect");
            return true;
        }
        ese_rect_destroy(colliderRect);
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_coll_rect");
    return false;
}

void entity_component_draw(
    EseEntityComponent *component,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawCallbacks *callbacks,
    void *callback_user_data
) {
    profile_start(PROFILE_ENTITY_DRAW_SECTION);
    
    profile_start(PROFILE_ENTITY_DRAW_SCREEN_POS);
    float entity_x = ese_point_get_x(component->entity->position);
    float entity_y = ese_point_get_y(component->entity->position);

    float view_left   = camera_x - view_width  / 2.0f;
    float view_top    = camera_y - view_height / 2.0f;
    float view_right  = camera_x + view_width  / 2.0f;
    float view_bottom = camera_y + view_height / 2.0f;

    int screen_x = (int)(entity_x - view_left);
    int screen_y = (int)(entity_y - view_top);
    profile_stop(PROFILE_ENTITY_DRAW_SCREEN_POS, "entity_component_draw_screen_pos");

    component->vtable->draw(component, screen_x, screen_y, callbacks, callback_user_data);
    
    profile_stop(PROFILE_ENTITY_DRAW_SECTION, "entity_component_draw");
}



bool entity_component_run_function(
    EseEntityComponent *component,
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv[]
) {
    log_assert("ENTITY_COMP", component, "entity_component_run_function called with NULL component");
    log_assert("ENTITY_COMP", entity, "entity_component_run_function called with NULL entity");

    profile_start(PROFILE_ENTITY_LUA_FUNCTION_CALL);

    bool result = component->vtable->run_function(component, entity, func_name, argc, (void**)argv);
    
    profile_stop(PROFILE_ENTITY_LUA_FUNCTION_CALL, "entity_component_run_function");
    return result;
}

void *entity_component_get_data(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_get_type called with NULL component");
    
    return component->data;
}

EseEntityComponent *entity_component_get(lua_State *L) {
    log_assert("ENTITY_COMP", L, "entity_component_get called with NULL L");

    if (!lua_istable(L, 1)) {
        luaL_argerror(L, 1, "expected a component proxy table, got non-table");
        return NULL;
    }

    if (!lua_getmetatable(L, 1)) {
        luaL_argerror(L, 1, "expected a component proxy table, got table without metatable");
        return NULL;
    }

    lua_getfield(L, -1, "__name");

    const char *metatable_name = NULL;
    if (lua_isstring(L, -1)) {
        metatable_name = lua_tostring(L, -1);
    }

    lua_pop(L, 2);

    if (strcmp(metatable_name, ENTITY_COMPONENT_COLLIDER_PROXY_META) == 0) {
        EseEntityComponentCollider *collider_comp = _entity_component_collider_get(L, 1);
        if (collider_comp == NULL) {
            luaL_error(L, "internal error: Collider metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &collider_comp->base;
    } else if (strcmp(metatable_name, ENTITY_COMPONENT_LUA_PROXY_META) == 0) {
        EseEntityComponentLua *lua_comp = _entity_component_lua_get(L, 1);
        if (lua_comp == NULL) {
            luaL_error(L, "internal error: Lua Base metatable name identified, but _get returned NULL.");
            return  NULL;
        }
        return &lua_comp->base;
    } else if (strcmp(metatable_name, ENTITY_COMPONENT_MAP_PROXY_META) == 0) {
        EseEntityComponentMap *ese_map_comp = _entity_component_ese_map_get(L, 1);
        if (ese_map_comp == NULL) {
            luaL_error(L, "internal error: Map metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &ese_map_comp->base;
    } else if (strcmp(metatable_name, ENTITY_COMPONENT_SHAPE_PROXY_META) == 0) {
        EseEntityComponentShape *shape_comp = _entity_component_shape_get(L, 1);
        if (shape_comp == NULL) {
            luaL_error(L, "internal error: Shape metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &shape_comp->base;
    } else if (strcmp(metatable_name, ENTITY_COMPONENT_SPRITE_PROXY_META) == 0) {
        EseEntityComponentSprite *sprite_comp = _entity_component_sprite_get(L, 1);
        if (sprite_comp == NULL) {
            luaL_error(L, "internal error: Sprite metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &sprite_comp->base;
    } else if (strcmp(metatable_name, ENTITY_COMPONENT_TEXT_PROXY_META) == 0) {
        EseEntityComponentText *text_comp = _entity_component_text_get(L, 1);
        if (text_comp == NULL) {
            luaL_error(L, "internal error: Text metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &text_comp->base;
    } else {
        luaL_error(L, "unknown component type with metatable name: %s", metatable_name);
        return NULL;
    }
}
