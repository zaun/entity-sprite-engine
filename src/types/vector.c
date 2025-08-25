#include <math.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "types/vector.h"

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseVector *_vector_make(void);

// Lua metamethods
static int _vector_lua_gc(lua_State *L);
static int _vector_lua_index(lua_State *L);
static int _vector_lua_newindex(lua_State *L);
static int _vector_lua_tostring(lua_State *L);

// Lua constructors
static int _vector_lua_new(lua_State *L);
static int _vector_lua_zero(lua_State *L);

// Lua methods
static int _vector_lua_set_direction(lua_State *L);
static int _vector_lua_magnitude(lua_State *L);
static int _vector_lua_normalize(lua_State *L);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
static EseVector *_vector_make() {
    EseVector *vector = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_GENERAL);
    vector->x = 0.0f;
    vector->y = 0.0f;
    vector->state = NULL;
    vector->lua_ref = LUA_NOREF;
    vector->lua_ref_count = 0;
    return vector;
}

// Lua metamethods
static int _vector_lua_gc(lua_State *L) {
    EseVector *vector = vector_lua_get(L, 1);

    if (vector) {
        // If lua_ref == LUA_NOREF, there are no more references to this vector, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this vector was referenced from C and should not be freed.
        if (vector->lua_ref == LUA_NOREF) {
            vector_destroy(vector);
        }
    }

    return 0;
}

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

// Lua constructors
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

    // Create the vector using the standard creation function
    EseVector *vector = _vector_make();
    vector->x = x;
    vector->y = y;
    vector->state = L;
    
    // Create proxy table for Lua-owned vector
    lua_newtable(L);
    lua_pushlightuserdata(L, vector);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "VectorProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

static int _vector_lua_zero(lua_State *L) {
    // Create the vector using the standard creation function
    EseVector *vector = _vector_make();  // We'll set the state manually
    vector->state = L;
    
    // Create proxy table for Lua-owned vector
    lua_newtable(L);
    lua_pushlightuserdata(L, vector);
    lua_setfield(L, -2, "__ptr");

    luaL_getmetatable(L, "VectorProxyMeta");
    lua_setmetatable(L, -2);

    return 1;
}

// Lua methods
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

static int _vector_lua_magnitude(lua_State *L) {
    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in magnitude method");
    }
    
    lua_pushnumber(L, vector_magnitude(vector));
    return 1;
}

static int _vector_lua_normalize(lua_State *L) {
    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in normalize method");
    }
    
    vector_normalize(vector);
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseVector *vector_create(EseLuaEngine *engine) {
    EseVector *vector = _vector_make();
    vector->state = engine->runtime;
    return vector;
}

EseVector *vector_copy(const EseVector *source) {
    if (source == NULL) {
        return NULL;
    }

    EseVector *copy = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_GENERAL);
    copy->x = source->x;
    copy->y = source->y;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void vector_destroy(EseVector *vector) {
    if (!vector) return;
    
    if (vector->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(vector);
    } else {
        // Has Lua references, decrement counter
        if (vector->lua_ref_count > 0) {
            vector->lua_ref_count--;
            
            if (vector->lua_ref_count == 0) {
                // No more C references, unref from Lua registry
                // Let Lua's GC handle the final cleanup
                luaL_unref(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
                vector->lua_ref = LUA_NOREF;
            }
        }
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void vector_lua_init(EseLuaEngine *engine) {
    log_assert("VECTOR", engine, "vector_lua_init called with NULL engine");

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

void vector_lua_push(EseVector *vector) {
    log_assert("VECTOR", vector, "vector_lua_push called with NULL vector");

    if (vector->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new proxy table since we don't store them
        lua_newtable(vector->state);
        lua_pushlightuserdata(vector->state, vector);
        lua_setfield(vector->state, -2, "__ptr");
        
        luaL_getmetatable(vector->state, "VectorProxyMeta");
        lua_setmetatable(vector->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
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

void vector_ref(EseVector *vector) {
    log_assert("VECTOR", vector, "vector_ref called with NULL vector");
    
    if (vector->lua_ref == LUA_NOREF) {
        // First time referencing - create proxy table and store reference
        lua_newtable(vector->state);
        lua_pushlightuserdata(vector->state, vector);
        lua_setfield(vector->state, -2, "__ptr");

        luaL_getmetatable(vector->state, "VectorProxyMeta");
        lua_setmetatable(vector->state, -2);

        // Store hard reference to prevent garbage collection
        vector->lua_ref = luaL_ref(vector->state, LUA_REGISTRYINDEX);
        vector->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        vector->lua_ref_count++;
    }
}

void vector_unref(EseVector *vector) {
    if (!vector) return;
    
    if (vector->lua_ref != LUA_NOREF && vector->lua_ref_count > 0) {
        vector->lua_ref_count--;
        
        if (vector->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
            vector->lua_ref = LUA_NOREF;
        }
    }
}

// Mathematical operations
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
