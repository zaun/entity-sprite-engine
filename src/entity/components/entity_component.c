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
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component_text.h"
#include "entity/components/entity_component.h"
#include "utility/profile.h"

void entity_component_lua_init(EseLuaEngine *engine) {
    profile_start(PROFILE_ENTITY_COMPONENT_UPDATE);
    
    _entity_component_collider_init(engine);
    _entity_component_lua_init(engine);
    _entity_component_map_init(engine);
    _entity_component_sprite_init(engine);
    _entity_component_text_init(engine);
    
    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_lua_init");
}

EseEntityComponent *entity_component_copy(EseEntityComponent* component) {
    log_assert("ENTITY_COMP", component, "entity_component_copy called with NULL component");

    profile_start(PROFILE_ENTITY_COMPONENT_COPY);

    EseEntityComponent *result;
    switch (component->type) {
        case ENTITY_COMPONENT_COLLIDER:
            result = _entity_component_collider_copy((EseEntityComponentCollider*)component->data);
            break;
        case ENTITY_COMPONENT_LUA:
            result = _entity_component_lua_copy((EseEntityComponentLua*)component->data);
            break;
        case ENTITY_COMPONENT_MAP:
            result = _entity_component_map_copy((EseEntityComponentMap*)component->data);
            break;
        case ENTITY_COMPONENT_SPRITE:
            result = _entity_component_sprite_copy((EseEntityComponentSprite*)component->data);
            break;
        case ENTITY_COMPONENT_TEXT:
            result = _entity_component_text_copy((EseEntityComponentText*)component->data);
            break;
        default:
            result = NULL;
            break;
    }

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
    
    switch (component->type) {
        case ENTITY_COMPONENT_COLLIDER:
            _entity_component_collider_destroy((EseEntityComponentCollider*)component->data);
            break;
        case ENTITY_COMPONENT_LUA:
            _entity_component_lua_destroy((EseEntityComponentLua*)component->data);
            break;
        case ENTITY_COMPONENT_MAP:
            _entity_component_map_destroy((EseEntityComponentMap*)component->data);
            break;
        case ENTITY_COMPONENT_SPRITE:
            _entity_component_sprite_destroy((EseEntityComponentSprite*)component->data);
            break;
        case ENTITY_COMPONENT_TEXT:
            _entity_component_text_destroy((EseEntityComponentText*)component->data);
            break;
        default:
            log_error("ENTITY", "Can't free unknown component type");
            break;
    }
    
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

    switch (component->type) {
        case ENTITY_COMPONENT_COLLIDER:
            profile_start(PROFILE_ENTITY_COMP_COLLIDER_UPDATE);
            // Update world bounds in case entity position changed
            entity_component_collider_update_world_bounds_only((EseEntityComponentCollider*)component->data);
            profile_stop(PROFILE_ENTITY_COMP_COLLIDER_UPDATE, "entity_component_collider_update");
            break;
        case ENTITY_COMPONENT_LUA:
            profile_start(PROFILE_ENTITY_COMP_LUA_UPDATE);
            _entity_component_lua_update((EseEntityComponentLua*)component->data, entity, delta_time);
            profile_stop(PROFILE_ENTITY_COMP_LUA_UPDATE, "entity_component_lua_update");
            break;
        case ENTITY_COMPONENT_MAP:
            profile_start(PROFILE_ENTITY_COMP_MAP_UPDATE);
            _entity_component_map_update((EseEntityComponentMap*)component->data, entity, delta_time);
            profile_stop(PROFILE_ENTITY_COMP_MAP_UPDATE, "entity_component_map_update");
            break;
        case ENTITY_COMPONENT_SPRITE:
            profile_start(PROFILE_ENTITY_COMP_SPRITE_UPDATE);
            _entity_component_sprite_update((EseEntityComponentSprite*)component->data, entity, delta_time);
            profile_stop(PROFILE_ENTITY_COMP_SPRITE_UPDATE, "entity_component_sprite_update");
            break;
        case ENTITY_COMPONENT_TEXT:
            profile_start(PROFILE_ENTITY_COMP_TEXT_UPDATE);
            _entity_component_text_update((EseEntityComponentText*)component->data, entity, delta_time);
            profile_stop(PROFILE_ENTITY_COMP_TEXT_UPDATE, "entity_component_text_update");
            break;
        default:
            log_debug("ENTITY_COMP", "Unknown TYPE updaging EseEntityComponent %s", component->id->value);
            break;
    }
    
    profile_stop(PROFILE_ENTITY_COMPONENT_UPDATE, "entity_component_update");
}

bool entity_component_detect_collision_component(EseEntityComponent *a, EseEntityComponent *b) {
    log_assert("ENTITY_COMP", a, "entity_component_detect_collision_component called with NULL a");
    log_assert("ENTITY_COMP", b, "entity_component_detect_collision_component called with NULL b");

    profile_start(PROFILE_ENTITY_COLLISION_TEST);

    if (a->type != ENTITY_COMPONENT_COLLIDER || b->type != ENTITY_COMPONENT_COLLIDER) {
        profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_component_detect_collision_component");
        return false;
    }

    EseEntityComponentCollider *colliderA = (EseEntityComponentCollider *)a->data;
    EseEntityComponentCollider *colliderB = (EseEntityComponentCollider *)b->data;

    for (size_t i = 0; i < colliderA->rects_count; i++) {
        EseRect *rect_a = rect_copy(colliderA->rects[i]);
        rect_set_x(rect_a, rect_get_x(rect_a) + point_get_x(a->entity->position));
        rect_set_y(rect_a, rect_get_y(rect_a) + point_get_y(a->entity->position));
        for (size_t j = 0; j < colliderB->rects_count; j++) {
            EseRect *rect_b = rect_copy(colliderB->rects[j]);
            rect_set_x(rect_b, rect_get_x(rect_b) + point_get_x(b->entity->position));
            rect_set_y(rect_b, rect_get_y(rect_b) + point_get_y(b->entity->position));
            if (rect_intersects(rect_a, rect_b)) {
                rect_destroy(rect_a);
                rect_destroy(rect_b);
                profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_component_detect_collision_component");
                return true;
            }
            rect_destroy(rect_b);
        }
        rect_destroy(rect_a);
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_TEST, "entity_component_detect_collision_component");
    return false;
}

bool entity_component_detect_collision_rect(EseEntityComponent *component, EseRect *rect) {
    log_assert("ENTITY_COMP", component, "entity_component_detect_collision_rect called with NULL component");
    log_assert("ENTITY_COMP", rect, "entity_component_detect_collision_rect called with NULL rect");

    profile_start(PROFILE_ENTITY_COLLISION_RECT_DETECT);

    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *colliderRect = rect_copy(collider->rects[i]);
        rect_set_x(colliderRect, rect_get_x(colliderRect) + point_get_x(component->entity->position));
        rect_set_y(colliderRect, rect_get_y(colliderRect) + point_get_y(component->entity->position));
        if (rect_intersects(colliderRect, rect)) {
            rect_destroy(colliderRect);
            profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_collision_rect");
            return true;
        }
        rect_destroy(colliderRect);
    }
    
    profile_stop(PROFILE_ENTITY_COLLISION_RECT_DETECT, "entity_component_detect_collision_rect");
    return false;
}

void entity_component_draw(
    EseEntityComponent *component,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawTextureCallback texCallback,
    EntityDrawRectCallback rectCallback,
    void *callback_user_data
) {
    profile_start(PROFILE_ENTITY_DRAW_SECTION);
    
    profile_start(PROFILE_ENTITY_DRAW_SCREEN_POS);
    float entity_x = point_get_x(component->entity->position);
    float entity_y = point_get_y(component->entity->position);

    float view_left   = camera_x - view_width  / 2.0f;
    float view_top    = camera_y - view_height / 2.0f;
    float view_right  = camera_x + view_width  / 2.0f;
    float view_bottom = camera_y + view_height / 2.0f;

    int screen_x = (int)(entity_x - view_left);
    int screen_y = (int)(entity_y - view_top);
    profile_stop(PROFILE_ENTITY_DRAW_SCREEN_POS, "entity_component_draw_screen_pos");

    switch(component->type) {
        case ENTITY_COMPONENT_COLLIDER: {
            profile_start(PROFILE_ENTITY_COMP_COLLIDER_DRAW);
            _entity_component_collider_draw(
                (EseEntityComponentCollider*)component->data,
                screen_x, screen_y, rectCallback, callback_user_data
            );
            profile_stop(PROFILE_ENTITY_COMP_COLLIDER_DRAW, "entity_component_collider_draw");
            break;
        }
        case ENTITY_COMPONENT_MAP: {
            profile_start(PROFILE_ENTITY_COMP_MAP_DRAW);
            _entity_component_map_draw(
                (EseEntityComponentMap*)component->data,
                screen_x, screen_y, texCallback, callback_user_data
            );
            profile_stop(PROFILE_ENTITY_COMP_MAP_DRAW, "entity_component_map_draw");
            break;
        }
        case ENTITY_COMPONENT_SPRITE: {
            profile_start(PROFILE_ENTITY_COMP_SPRITE_DRAW);
            _entity_component_sprite_draw(
                (EseEntityComponentSprite*)component->data,
                screen_x, screen_y, texCallback, callback_user_data
            );
            profile_stop(PROFILE_ENTITY_COMP_SPRITE_DRAW, "entity_component_sprite_draw");
            break;
        }
        case ENTITY_COMPONENT_TEXT: {
            profile_start(PROFILE_ENTITY_COMP_TEXT_DRAW);
            _entity_component_text_draw(
                (EseEntityComponentText*)component->data,
                screen_x, screen_y, texCallback, callback_user_data
            );
            profile_stop(PROFILE_ENTITY_COMP_TEXT_DRAW, "entity_component_text_draw");
            break;
        }
        default:
            break;
    }
    
    profile_stop(PROFILE_ENTITY_DRAW_SECTION, "entity_component_draw");
}



bool entity_component_run_function(
    EseEntityComponent *component,
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv
) {
    log_assert("ENTITY_COMP", component, "entity_component_run_function called with NULL component");
    log_assert("ENTITY_COMP", entity, "entity_component_run_function called with NULL entity");

    profile_start(PROFILE_ENTITY_LUA_FUNCTION_CALL);

    bool result;
    switch (component->type) {
        case ENTITY_COMPONENT_LUA:
            result = entity_component_lua_run((EseEntityComponentLua *)component->data, entity, func_name, argc, argv);
            break;
        default:
            // Other component types don't support function execution
            result = false;
            break;
    }
    
    profile_stop(PROFILE_ENTITY_LUA_FUNCTION_CALL, "entity_component_run_function");
    return result;
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

    if (strcmp(metatable_name, COLLIDER_PROXY_META) == 0) {
        EseEntityComponentCollider *collider_comp = _entity_component_collider_get(L, 1);
        if (collider_comp == NULL) {
            luaL_error(L, "internal error: Collider metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &collider_comp->base;
    } else if (strcmp(metatable_name, "EntityComponentLuaProxyMeta") == 0) {
        EseEntityComponentLua *lua_comp = _entity_component_lua_get(L, 1);
        if (lua_comp == NULL) {
            luaL_error(L, "internal error: Lua Base metatable name identified, but _get returned NULL.");
            return  NULL;
        }
        return &lua_comp->base;
    } else if (strcmp(metatable_name, MAP_PROXY_META) == 0) {
        EseEntityComponentMap *map_comp = _entity_component_map_get(L, 1);
        if (map_comp == NULL) {
            luaL_error(L, "internal error: Map metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &map_comp->base;
    } else if (strcmp(metatable_name, SPRITE_PROXY_META) == 0) {
        EseEntityComponentSprite *sprite_comp = _entity_component_sprite_get(L, 1);
        if (sprite_comp == NULL) {
            luaL_error(L, "internal error: Sprite metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &sprite_comp->base;
    } else if (strcmp(metatable_name, TEXT_PROXY_META) == 0) {
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
