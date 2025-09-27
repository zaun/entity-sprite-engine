#include <string.h>
#include "utility/log.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "graphics/sprite.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_sprite.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"

// VTable wrapper functions
static EseEntityComponent* _sprite_vtable_copy(EseEntityComponent* component) {
    return _entity_component_sprite_copy((EseEntityComponentSprite*)component->data);
}

static void _sprite_vtable_destroy(EseEntityComponent* component) {
    _entity_component_sprite_destroy((EseEntityComponentSprite*)component->data);
}

static void _sprite_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    _entity_component_sprite_update((EseEntityComponentSprite*)component->data, entity, delta_time);
}

static void _sprite_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    EntityDrawCallbacks* draw_callbacks = (EntityDrawCallbacks*)callbacks;
    _entity_component_sprite_draw((EseEntityComponentSprite*)component->data, screen_x, screen_y, draw_callbacks->draw_texture, user_data);
}

static bool _sprite_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    // Sprite components don't support function execution
    return false;
}

static void _sprite_vtable_ref(EseEntityComponent* component) {
    EseEntityComponentSprite *sprite = (EseEntityComponentSprite*)component->data;
    log_assert("ENTITY_COMP", sprite, "sprite vtable ref called with NULL");
    if (sprite->base.lua_ref == LUA_NOREF) {
        EseEntityComponentSprite **ud = (EseEntityComponentSprite **)lua_newuserdata(sprite->base.lua->runtime, sizeof(EseEntityComponentSprite *));
        *ud = sprite;
        luaL_getmetatable(sprite->base.lua->runtime, ENTITY_COMPONENT_SPRITE_PROXY_META);
        lua_setmetatable(sprite->base.lua->runtime, -2);
        sprite->base.lua_ref = luaL_ref(sprite->base.lua->runtime, LUA_REGISTRYINDEX);
        sprite->base.lua_ref_count = 1;
    } else {
        sprite->base.lua_ref_count++;
    }
}

static void _sprite_vtable_unref(EseEntityComponent* component) {
    EseEntityComponentSprite *sprite = (EseEntityComponentSprite*)component->data;
    if (!sprite) return;
    if (sprite->base.lua_ref != LUA_NOREF && sprite->base.lua_ref_count > 0) {
        sprite->base.lua_ref_count--;
        if (sprite->base.lua_ref_count == 0) {
            luaL_unref(sprite->base.lua->runtime, LUA_REGISTRYINDEX, sprite->base.lua_ref);
            sprite->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for sprite components
static const ComponentVTable sprite_vtable = {
    .copy = _sprite_vtable_copy,
    .destroy = _sprite_vtable_destroy,
    .update = _sprite_vtable_update,
    .draw = _sprite_vtable_draw,
    .run_function = _sprite_vtable_run_function,
    .ref = _sprite_vtable_ref,
    .unref = _sprite_vtable_unref
};

// (legacy table-proxy registration removed; using userdata + ref counting)

static EseEntityComponent *_entity_component_sprite_make(EseLuaEngine *engine, const char *sprite_name) {
    EseEntityComponentSprite *component = memory_manager.malloc(sizeof(EseEntityComponentSprite), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_SPRITE;
    component->base.vtable = &sprite_vtable;
    
    component->current_frame = 0;
    component->sprite_ellapse_time = 0;

    if (sprite_name != NULL) {
        EseEngine *game_engine = (EseEngine *)lua_engine_get_registry_key(engine->runtime, ENGINE_KEY);
        component->sprite_name = memory_manager.strdup(sprite_name, MMTAG_ENTITY);
        component->sprite = engine_get_sprite(game_engine, component->sprite_name);
        if (component->sprite == NULL) {
            log_debug("ENTITY_COMP", "Sprite '%s' not found", sprite_name);
        } else {
            log_debug("ENTITY_COMP", "Sprite '%s' found, frame count: %d", sprite_name, sprite_get_frame_count(component->sprite));
        }
    } else {
        component->sprite_name = NULL;
        component->sprite = NULL;
    }

    return &component->base;
}

EseEntityComponent *_entity_component_sprite_copy(const EseEntityComponentSprite *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_sprite_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_sprite_make(src->base.lua, src->sprite_name);

    return copy;
}

void _entity_component_ese_sprite_cleanup(EseEntityComponentSprite *component)
{
    memory_manager.free(component->sprite_name);
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_sprite_destroy_count");
}

void _entity_component_sprite_destroy(EseEntityComponentSprite *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_sprite_destroy called with NULL src");

    // Respect Lua registry ref-count; only free when no refs remain
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_ese_sprite_cleanup(component);
        } else {
            // We dont "own" the sprite so dont free it}
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_ese_sprite_cleanup(component);
    }
}


void _entity_component_sprite_update(EseEntityComponentSprite *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "entity_component_lua_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "entity_component_lua_update called with NULL src");

    if (component->sprite == NULL) {
        component->current_frame = 0;
        component->sprite_ellapse_time = 0;
        return;
    }

    component->sprite_ellapse_time += delta_time;
    float speed = sprite_get_speed(component->sprite);

    if (component->sprite_ellapse_time >= speed) {
        component->sprite_ellapse_time = 0;
        int frame_count = sprite_get_frame_count(component->sprite);
        if (frame_count > 0) {
            component->current_frame = (component->current_frame + 1) % frame_count;
        }
    }
}

/**
 * @brief Lua function to create a new EseEntityComponentSprite object.
 * 
 * @details Callable from Lua as EseEntityComponentSprite.new().
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 * 
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_sprite_new(lua_State *L) {
    const char *sprite_name = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1 && lua_isstring(L, 1)) {
        sprite_name = lua_tostring(L, 1);
    } else if (n_args == 1 && !lua_isstring(L, 1)) {
        log_debug("ENTITY_COMP", "Script must be a string, ignored");
    } else if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentLua.new() or EseEntityComponentLua.new(String)");
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_sprite_make(lua, sprite_name);

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentSprite **ud = (EseEntityComponentSprite **)lua_newuserdata(L, sizeof(EseEntityComponentSprite *));
    *ud = (EseEntityComponentSprite *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_SPRITE_PROXY_META);
    lua_setmetatable(L, -2);
    
    return 1;
}

EseEntityComponentSprite *_entity_component_sprite_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseEntityComponentSprite **ud = (EseEntityComponentSprite **)luaL_testudata(L, idx, ENTITY_COMPONENT_SPRITE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentSprite objects (getter).
 * 
 * @details Handles property access for EseEntityComponentSprite objects from Lua. 
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_sprite_index(lua_State *L) {
    EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Return nil for freed components
    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    
    if (!key) return 0;
    
    if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        lua_pushstring(L, ese_uuid_get_value(component->base.id));
        return 1;
    } else if (strcmp(key, "sprite") == 0) {
        lua_pushstring(L, component->sprite_name);
        return 1;
    }
    
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentSprite objects (setter).
 * 
 * @details Handles property assignment for EseEntityComponentSprite objects from Lua.
 * 
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_sprite_newindex(lua_State *L) {
    EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Silently ignore writes to freed components
    if (!component) {
        return 0;
    }
    
    if (!key) return 0;
    
    if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        return 0;
    } else if (strcmp(key, "id") == 0) {
        return luaL_error(L, "id is read-only");
    } else if (strcmp(key, "sprite") == 0) {
        if (!lua_isstring(L, 3) && !lua_isnil(L, 3)) {
            return luaL_error(L, "sprite must be a string or nil");
        }

        component->sprite = NULL;
        if (component->sprite_name != NULL) {
            memory_manager.free(component->sprite_name);
            component->sprite_name = NULL;
        }
        
        if (lua_isstring(L, 3)) {
            EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);

            const char *sprite_name = lua_tostring(L, 3);
            component->current_frame = 0;
            component->sprite_name = memory_manager.strdup(sprite_name, MMTAG_ENTITY);
            component->sprite = engine_get_sprite(engine, component->sprite_name);
            return 0;
        }
        // nil path handled: sprite cleared and name freed above
        return 0;
    }
    
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentSprite objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseEntityComponentSprite's memory was allocated by Lua and should be freed.
 * If false, the EseEntityComponentSprite's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_sprite_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentSprite **ud = (EseEntityComponentSprite **)luaL_testudata(L, 1, ENTITY_COMPONENT_SPRITE_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseEntityComponentSprite *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_sprite_destroy(component);
            *ud = NULL;
        }
    }
    
    return 0;
}

static int _entity_component_sprite_tostring(lua_State *L) {
    EseEntityComponentSprite *component = _entity_component_sprite_get(L, 1);
    
    if (!component) {
        lua_pushstring(L, "EntityComponentSprite: (invalid)");
        return 1;
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "EntityComponentSprite: %p (id=%s active=%s sprite=%p)", 
             (void*)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->sprite);
    lua_pushstring(L, buf);
    
    return 1;
}

void _entity_component_sprite_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_sprite_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EntityComponentSprite metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_SPRITE_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentSpriteProxyMeta to engine");
        lua_pushstring(L, ENTITY_COMPONENT_SPRITE_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_sprite_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_sprite_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_sprite_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_sprite_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
    
    // Create global EntityComponentSprite table with constructor
    lua_getglobal(L, "EntityComponentSprite");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EntityComponentSprite table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_sprite_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentSprite");
    } else {
        lua_pop(L, 1);
    }
}

void _entity_component_sprite_draw(EseEntityComponentSprite *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data) {
    log_assert("ENTITY_COMP", component, "_entity_component_sprite_drawable called with NULL component");

    if (!component->sprite) {
        return;
    }

    const char *texture_id;
    float x1, y1, x2, y2;
    int w, h;
    sprite_get_frame(
        component->sprite, component->current_frame,
        &texture_id, &x1, &y1, &x2, &y2, &w, &h
    );

    texCallback(
        screen_x, screen_y, w, h,
        component->base.entity->draw_order,
        texture_id, x1, y1, x2, y2,
        w, h,
        callback_user_data
    );
}

EseEntityComponent *entity_component_sprite_create(EseLuaEngine *engine, const char *sprite_name) {
    log_assert("ENTITY_COMP", engine, "entity_component_sprite_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_sprite_make(engine, sprite_name);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}
