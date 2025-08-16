#include <math.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/vector.h"

/**
 * @brief Pushes a EseVector pointer as a Lua userdata object onto the stack.
 * 
 * @details Creates a new Lua table that acts as a proxy for the EseVector object,
 *          storing the C pointer as light userdata in the "__ptr" field and
 *          setting the VectorProxyMeta metatable for property access.
 * 
 * @param vector Pointer to the EseVector object to wrap for Lua access
 * @param is_lua_owned True if LUA will handle freeing
 * 
 * @warning The EseVector object must remain valid for the lifetime of the Lua object
 */
void _vector_lua_register(EseVector *vector, bool is_lua_owned) {
    log_assert("VECTOR", vector, "_vector_lua_register called with NULL vector");
    log_assert("VECTOR", vector->lua_ref == LUA_NOREF, "_vector_lua_register vector is already registered");
    
    lua_newtable(vector->state);
    lua_pushlightuserdata(vector->state, vector);
    lua_setfield(vector->state, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(vector->state, is_lua_owned);
    lua_setfield(vector->state, -2, "__is_lua_owned");

    luaL_getmetatable(vector->state, "VectorProxyMeta");
    lua_setmetatable(vector->state, -2);

    // Store a reference to this proxy table in the Lua registry
    vector->lua_ref = luaL_ref(vector->state, LUA_REGISTRYINDEX);
}

void vector_lua_push(EseVector *vector) {
    log_assert("VECTOR", vector, "vector_lua_push called with NULL vector");
    log_assert("VECTOR", vector->lua_ref != LUA_NOREF, "vector_lua_push vector not registered with lua");

    // Push the proxy table back onto the stack for Lua to receive
    lua_rawgeti(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
}

/**
 * @brief Lua function to create a new EseVector object.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new vector object)
 */
static int _vector_lua_new(lua_State *L) {
    float x = 0.0f;
    float y = 0.0f;

    int n_args = lua_gettop(L);
    if (n_args == 2) {
        if (lua_isnumber(L, 1)) {
            x = (float)lua_tonumber(L, 1);
        } else {
            return luaL_error(L, "x must be a number");
        }
        if (lua_isnumber(L, 2)) {
            y = (float)lua_tonumber(L, 2);
        } else {
            return luaL_error(L, "y must be a number");
        }
    } else if (n_args != 0) {
        return luaL_error(L, "new() takes 0 or 2 arguments");
    }

    EseVector *vector = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_GENERAL);
    vector->x = x;
    vector->y = y;
    vector->state = L;
    vector->lua_ref = LUA_NOREF;
    _vector_lua_register(vector, true);

    vector_lua_push(vector);
    return 1;
}

/**
 * @brief Lua function to create a zero vector.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new vector object)
 */
static int _vector_lua_zero(lua_State *L) {
    EseVector *vector = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_GENERAL);
    vector->x = 0.0f;
    vector->y = 0.0f;
    vector->state = L;
    vector->lua_ref = LUA_NOREF;
    _vector_lua_register(vector, true);

    vector_lua_push(vector);
    return 1;
}

/**
 * @brief Lua method to set vector direction.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _vector_lua_set_direction(lua_State *L) {
    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in set_direction method");
    }
    
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        return luaL_error(L, "set_direction(direction, magnitude) requires string and number");
    }
    
    const char *direction = lua_tostring(L, 1);
    float magnitude = (float)lua_tonumber(L, 2);
    
    vector_set_direction(vector, direction, magnitude);
    return 0;
}

/**
 * @brief Lua method to get vector magnitude.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the magnitude)
 */
static int _vector_lua_magnitude(lua_State *L) {
    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in magnitude method");
    }
    
    lua_pushnumber(L, vector_magnitude(vector));
    return 1;
}

/**
 * @brief Lua method to normalize vector.
 * 
 * @param L Lua state pointer
 * @return Number of return values (always 0)
 */
static int _vector_lua_normalize(lua_State *L) {
    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in normalize method");
    }
    
    vector_normalize(vector);
    return 0;
}

/**
 * @brief Lua __index metamethod for EseVector objects (getter).
 * 
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack
 */
static int _vector_lua_index(lua_State *L) {
    EseVector *vector = vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) return 0;

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, vector->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, vector->y);
        return 1;
    } else if (strcmp(key, "set_direction") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_set_direction, 1);
        return 1;
    } else if (strcmp(key, "magnitude") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_magnitude, 1);
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_normalize, 1);
        return 1;
    }
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseVector objects (setter).
 * 
 * @param L Lua state pointer
 * @return Always returns 0 or throws Lua error for invalid operations
 */
static int _vector_lua_newindex(lua_State *L) {
    EseVector *vector = vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) return 0;

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "vector.x must be a number");
        }
        vector->x = (float)lua_tonumber(L, 3);
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            return luaL_error(L, "vector.y must be a number");
        }
        vector->y = (float)lua_tonumber(L, 3);
        return 0;
    }
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseVector objects.
 * 
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _vector_lua_gc(lua_State *L) {
    EseVector *vector = vector_lua_get(L, 1);

    if (vector) {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned) {
            vector_destroy(vector);
            log_debug("LUA_GC", "Vector object (Lua-owned) garbage collected and C memory freed.");
        } else {
            log_debug("LUA_GC", "Vector object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _vector_lua_tostring(lua_State *L) {
    EseVector *vector = vector_lua_get(L, 1);

    if (!vector) {
        lua_pushstring(L, "Vector: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Vector: %p (x=%.2f, y=%.2f)", (void*)vector, vector->x, vector->y);
    lua_pushstring(L, buf);

    return 1;
}

void vector_lua_init(EseLuaEngine *engine) {
    if (luaL_newmetatable(engine->runtime, "VectorProxyMeta")) {
        log_debug("LUA", "Adding entity VectorProxyMeta to engine");
        lua_pushcfunction(engine->runtime, _vector_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _vector_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _vector_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _vector_lua_tostring);
        lua_setfield(engine->runtime, -2, "__tostring");
        lua_pushstring(engine->runtime, "locked");
        lua_setfield(engine->runtime, -2, "__metatable");
    }
    lua_pop(engine->runtime, 1);
    
    // Create global EseVector table with constructor
    lua_getglobal(engine->runtime, "Vector");
    if (lua_isnil(engine->runtime, -1)) {
        lua_pop(engine->runtime, 1);
        log_debug("LUA", "Creating global EseVector table");
        lua_newtable(engine->runtime);
        lua_pushcfunction(engine->runtime, _vector_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _vector_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Vector");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

EseVector *vector_create(EseLuaEngine *engine) {
    EseVector *vector = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_GENERAL);
    vector->x = 0.0f;
    vector->y = 0.0f;
    vector->state = engine->runtime;
    vector->lua_ref = LUA_NOREF;
    _vector_lua_register(vector, false);
    return vector;
}

void vector_destroy(EseVector *vector) {
    if (vector) {
        if (vector->lua_ref != LUA_NOREF) {
            luaL_unref(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
        }
        memory_manager.free(vector);
    }
}

EseVector *vector_lua_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }
    
    if (!lua_getmetatable(L, idx)) {
        return NULL;
    }
    
    luaL_getmetatable(L, "VectorProxyMeta");
    
    if (!lua_rawequal(L, -1, -2)) {
        lua_pop(L, 2);
        return NULL;
    }
    
    lua_pop(L, 2);
    
    lua_getfield(L, idx, "__ptr");
    
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }
    
    void *vector = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    return (EseVector *)vector;
}

void vector_set_direction(EseVector *vector, const char *direction, float magnitude) {
    if (!vector || !direction) return;

    float dx = 0.0f, dy = 0.0f;
    
    // Parse direction string
    size_t len = strlen(direction);
    for (size_t i = 0; i < len; i++) {
        switch (direction[i]) {
            case 'n': case 'N': dy += 1.0f; break;
            case 's': case 'S': dy -= 1.0f; break;
            case 'e': case 'E': dx += 1.0f; break;
            case 'w': case 'W': dx -= 1.0f; break;
        }
    }
    
    // Normalize for diagonal directions
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 0.0f) {
        dx /= length;
        dy /= length;
    }
    
    // Apply magnitude
    vector->x = dx * magnitude;
    vector->y = dy * magnitude;
}

float vector_magnitude(const EseVector *vector) {
    if (!vector) return 0.0f;
    return sqrtf(vector->x * vector->x + vector->y * vector->y);
}

void vector_normalize(EseVector *vector) {
    if (!vector) return;
    
    float magnitude = vector_magnitude(vector);
    if (magnitude > 0.0f) {
        vector->x /= magnitude;
        vector->y /= magnitude;
    }
}
