#include <math.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
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
/**
 * @brief Creates a new EseVector instance with default values
 * 
 * Allocates memory for a new EseVector and initializes all fields to safe defaults.
 * The vector starts at origin (0,0) with no Lua state or references.
 * 
 * @return Pointer to the newly created EseVector, or NULL on allocation failure
 */
static EseVector *_vector_make() {
    EseVector *vector = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_VECTOR);
    vector->x = 0.0f;
    vector->y = 0.0f;
    vector->state = NULL;
    vector->lua_ref = LUA_NOREF;
    vector->lua_ref_count = 0;
    return vector;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseVector
 * 
 * Handles cleanup when a Lua proxy table for an EseVector is garbage collected.
 * Only frees the underlying EseVector if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static int _vector_lua_gc(lua_State *L) {
    // Get from userdata
    EseVector **ud = (EseVector **)luaL_testudata(L, 1, "VectorMeta");
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseVector *vector = *ud;
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

/**
 * @brief Lua __index metamethod for EseVector property access
 * 
 * Provides read access to vector properties (x, y) from Lua. When a Lua script
 * accesses vector.x or vector.y, this function is called to retrieve the values.
 * Also provides access to methods like set_direction, magnitude, and normalize.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (1 for valid properties/methods, 0 for invalid)
 */
static int _vector_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_INDEX);
    EseVector *vector = vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, vector->x);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, vector->y);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "set_direction") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_set_direction, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "magnitude") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_magnitude, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _vector_lua_normalize, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    }
    profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index");
    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseVector property assignment
 * 
 * Provides write access to vector properties (x, y) from Lua. When a Lua script
 * assigns to vector.x or vector.y, this function is called to update the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static int _vector_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEWINDEX);
    EseVector *vector = vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return luaL_error(L, "vector.x must be a number");
        }
        vector->x = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_isnumber(L, 3)) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return luaL_error(L, "vector.y must be a number");
        }
        vector->y = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return 0;
    }
    profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __tostring metamethod for EseVector string representation
 * 
 * Converts an EseVector to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y coordinates.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
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
/**
 * @brief Lua constructor function for creating new EseVector instances
 * 
 * Creates a new EseVector from Lua with specified x,y coordinates.
 * This function is called when Lua code executes `Vector.new(x, y)`.
 * It validates the arguments, creates the underlying EseVector, and returns a proxy
 * table that provides access to the vector's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _vector_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEW);
    float x = 0.0f;
    float y = 0.0f;

    int n_args = lua_gettop(L);
    if (n_args == 2) {
        if (lua_isnumber(L, 1)) {
            x = (float)lua_tonumber(L, 1);
        } else {
            profile_cancel(PROFILE_LUA_VECTOR_NEW);
            return luaL_error(L, "x must be a number");
        }
        if (lua_isnumber(L, 2)) {
            y = (float)lua_tonumber(L, 2);
        } else {
            profile_cancel(PROFILE_LUA_VECTOR_NEW);
            return luaL_error(L, "y must be a number");
        }
    } else if (n_args != 0) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "new() takes 0 or 2 arguments");
    }

    // Create the vector using the standard creation function
    EseVector *vector = _vector_make();
    vector->x = x;
    vector->y = y;
    vector->state = L;
    
    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, "VectorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_VECTOR_NEW, "vector_lua_new");
    return 1;
}

/**
 * @brief Lua constructor function for creating EseVector at origin
 * 
 * Creates a new EseVector at the origin (0,0) from Lua.
 * This function is called when Lua code executes `Vector.zero()`.
 * It's a convenience constructor for creating vectors at the default position.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static int _vector_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_ZERO);
    // Create the vector using the standard creation function
    EseVector *vector = _vector_make();  // We'll set the state manually
    vector->state = L;
    
    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, "VectorMeta");
    lua_setmetatable(L, -2);

    profile_stop(PROFILE_LUA_VECTOR_ZERO, "vector_lua_zero");
    return 1;
}

// Lua methods
/**
 * @brief Lua method for setting vector direction and magnitude
 * 
 * Sets the vector's direction using cardinal direction strings (N, S, E, W)
 * and applies the specified magnitude. Handles diagonal directions automatically.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
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
 * @brief Lua method for calculating vector magnitude
 * 
 * Calculates and returns the magnitude (length) of the vector using the
 * Pythagorean theorem (sqrt(x² + y²)).
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the magnitude value)
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
 * @brief Lua method for normalizing the vector
 * 
 * Normalizes the vector to unit length (magnitude = 1.0) while preserving
 * its direction. If the vector has zero magnitude, no change is made.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
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

    EseVector *copy = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_VECTOR);
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
        vector_unref(vector);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

// Lua integration
void vector_lua_init(EseLuaEngine *engine) {
    log_assert("VECTOR", engine, "vector_lua_init called with NULL engine");

    if (luaL_newmetatable(engine->runtime, "VectorMeta")) {
        log_debug("LUA", "Adding entity VectorMeta to engine");
        lua_pushstring(engine->runtime, "VectorMeta");
        lua_setfield(engine->runtime, -2, "__name");
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
        // Lua-owned: create a new userdata
        EseVector **ud = (EseVector **)lua_newuserdata(vector->state, sizeof(EseVector *));
        *ud = vector;

        // Attach metatable
        luaL_getmetatable(vector->state, "VectorMeta");
        lua_setmetatable(vector->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
    }
}

EseVector *vector_lua_get(lua_State *L, int idx) {
    log_assert("VECTOR", L, "vector_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseVector **ud = (EseVector **)luaL_testudata(L, idx, "VectorMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void vector_ref(EseVector *vector) {
    log_assert("VECTOR", vector, "vector_ref called with NULL vector");
    
    if (vector->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseVector **ud = (EseVector **)lua_newuserdata(vector->state, sizeof(EseVector *));
        *ud = vector;

        // Attach metatable
        luaL_getmetatable(vector->state, "VectorMeta");
        lua_setmetatable(vector->state, -2);

        // Store hard reference to prevent garbage collection
        vector->lua_ref = luaL_ref(vector->state, LUA_REGISTRYINDEX);
        vector->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        vector->lua_ref_count++;
    }

    profile_count_add("vector_ref_count");
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

    profile_count_add("vector_unref_count");
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
