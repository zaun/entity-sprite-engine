#include <stdbool.h>
#include <string.h>
#include "entity/entity_private.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"
#include "vendor/lua/src/lauxlib.h"
#include "entity/components/entity_component_collider.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component.h"

void entity_component_lua_init(EseLuaEngine *engine) {
    _entity_component_collider_init(engine);
    _entity_component_lua_init(engine);
    _entity_component_sprite_init(engine);
}

EseEntityComponent *entity_component_copy(EseEntityComponent* component) {
    log_assert("ENTITY_COMP", component, "entity_component_copy called with NULL component");

    switch (component->type) {
        case ENTITY_COMPONENT_COLLIDER:
            return _entity_component_collider_copy((EseEntityComponentCollider*)component->data);
            break;
        case ENTITY_COMPONENT_LUA:
            return _entity_component_lua_copy((EseEntityComponentLua*)component->data);
            break;
        case ENTITY_COMPONENT_SPRITE:
            return _entity_component_sprite_copy((EseEntityComponentSprite*)component->data);
            break;
        default:
            return NULL;
            break;
    }

}

void entity_component_destroy(EseEntityComponent* component) {
    log_assert("ENTITY_COMP", component, "entity_component_destroy called with NULL component");

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
        case ENTITY_COMPONENT_SPRITE:
            _entity_component_sprite_destroy((EseEntityComponentSprite*)component->data);
            break;
        default:
            log_error("ENTITY", "Can't free unknown component type");
            break;
    }
}

void entity_component_push(EseEntityComponent *component) {
    log_assert("ENTITY_COMP", component, "entity_component_push called with NULL component");
    log_assert("ENTITY_COMP", component->lua_ref != LUA_NOREF, "entity_component_push component not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(component->lua->runtime, LUA_REGISTRYINDEX, component->lua_ref);
}

void entity_component_update(EseEntityComponent *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "entity_component_update called with NULL component");

    switch (component->type) {
        case ENTITY_COMPONENT_COLLIDER:
            _entity_component_collider_update((EseEntityComponentCollider*)component->data, entity, delta_time);
            break;
        case ENTITY_COMPONENT_LUA:
            _entity_component_lua_update((EseEntityComponentLua*)component->data, entity, delta_time);
            break;
        case ENTITY_COMPONENT_SPRITE:
            _entity_component_sprite_update((EseEntityComponentSprite*)component->data, entity, delta_time);
            break;
        default:
            log_debug("ENTITY_COMP", "Unknown TYPE updaging EseEntityComponent %s", component->id->value);
            break;
    }
}

bool entity_component_test_collision(EseEntityComponent *a, EseEntityComponent *b) {
    log_assert("ENTITY_COMP", a, "entity_component_test_collision called with NULL a");
    log_assert("ENTITY_COMP", b, "entity_component_test_collision called with NULL b");

    if (a->type != ENTITY_COMPONENT_COLLIDER || b->type != ENTITY_COMPONENT_COLLIDER) {
        return false;
    }

    EseEntityComponentCollider *colliderA = (EseEntityComponentCollider *)a->data;
    EseEntityComponentCollider *colliderB = (EseEntityComponentCollider *)b->data;

    for (size_t i = 0; i < colliderA->rects_count; i++) {
        EseRect *rect_a = rect_copy(colliderA->rects[i], true);
        rect_a->x += a->entity->position->x;
        rect_a->y += a->entity->position->y;
        for (size_t j = 0; j < colliderB->rects_count; j++) {
            EseRect *rect_b = rect_copy(colliderB->rects[j], true);
            rect_b->x += b->entity->position->x;
            rect_b->y += b->entity->position->y;
            if (rect_intersects(rect_a, rect_b)) {
                rect_destroy(rect_a);
                rect_destroy(rect_b);
                return true;
            }
            rect_destroy(rect_b);
        }
        rect_destroy(rect_a);
    }
    return false;
}

bool entity_component_test_rect(EseEntityComponent *component, EseRect *rect) {
    log_assert("ENTITY_COMP", component, "entity_component_test_rect called with NULL component");
    log_assert("ENTITY_COMP", rect, "entity_component_test_rect called with NULL rect");

    EseEntityComponentCollider *collider = (EseEntityComponentCollider *)component->data;
    for (size_t i = 0; i < collider->rects_count; i++) {
        EseRect *colliderRect = rect_copy(collider->rects[i], true);
        if (rect_intersects(colliderRect, rect)) {
            return true;
        }
    }
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
    float entity_x = component->entity->position->x;
    float entity_y = component->entity->position->y;

    float view_left   = camera_x - view_width  / 2.0f;
    float view_top    = camera_y - view_height / 2.0f;
    float view_right  = camera_x + view_width  / 2.0f;
    float view_bottom = camera_y + view_height / 2.0f;

    int screen_x = (int)(entity_x - view_left);
    int screen_y = (int)(entity_y - view_top);

    switch(component->type) {
        case ENTITY_COMPONENT_SPRITE: {
            _entity_component_sprite_draw(
                (EseEntityComponentSprite*)component->data,
                screen_x, screen_y, texCallback, callback_user_data
            );
            break;
        }
        case ENTITY_COMPONENT_COLLIDER: {
            _entity_component_collider_draw(
                (EseEntityComponentCollider*)component->data,
                screen_x, screen_y, rectCallback, callback_user_data
            );
            break;
        }
        default:
            break;
    }

}

void entity_component_run_function_with_args(
    EseEntityComponent *component,
    const char *func_name,
    int argc,
    EseLuaValue *argv
) {
    log_assert("ENTITY_COMP", component, "entity_component_run_function_with_args called with NULL component");

    if (component->type != ENTITY_COMPONENT_LUA) {
        return;
    }

    EseEntityComponentLua *comp = (EseEntityComponentLua *)component;

    lua_engine_instance_run_function_with_args(
        comp->engine,
        comp->instance_ref,
        comp->base.entity->lua_ref,
        func_name, argc, argv
    );
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

    if (strcmp(metatable_name, "EntityComponentColliderProxyMeta") == 0) {
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
    } else if (strcmp(metatable_name, "EntityComponentSpriteProxyMeta") == 0) {
        EseEntityComponentSprite *sprite_comp = _entity_component_sprite_get(L, 1);
        if (sprite_comp == NULL) {
            luaL_error(L, "internal error: Sprite metatable name identified, but _get returned NULL.");
            return NULL;
        }
        return &sprite_comp->base;
    } else {
        luaL_error(L, "unknown component type with metatable name: %s", metatable_name);
        return NULL;
    }
}
