#include <math.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/vector.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseVector
 * 
 * @details This structure stores the x and y components of a vector in 2D space.
 */
struct EseVector {
    float x;            /**< The x-component of the vector */
    float y;            /**< The y-component of the vector */

    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this vector has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseVector *_ese_vector_make(void);

// Lua metamethods
static EseLuaValue* _ese_vector_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
static EseLuaValue* _ese_vector_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua methods
static EseLuaValue* _ese_vector_lua_set_direction(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_magnitude(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_vector_lua_normalize(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

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
static EseVector *_ese_vector_make() {
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
static EseLuaValue* _ese_vector_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the vector from the first argument
    if (!lua_value_is_vector(argv[0])) {
        return NULL;
    }

    EseVector *vector = lua_value_get_vector(argv[0]);
    if (vector) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this vector, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this vector was referenced from C and should not be freed.
        if (vector->lua_ref == ESE_LUA_NOREF) {
            ese_vector_destroy(vector);
        }
    }

    return NULL;
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
static EseLuaValue* _ese_vector_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_VECTOR_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return lua_value_create_error("result", "index requires 2 arguments");
    }

    if (!lua_value_is_vector(argv[0])) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return lua_value_create_error("result", "first argument must be a vector");
    }
    
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return lua_value_create_error("result", "invalid vector");
    }

    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return lua_value_create_error("result", "second argument must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_VECTOR_INDEX);
        return lua_value_create_error("result", "invalid key");
    }

    if (strcmp(key, "x") == 0) {
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return lua_value_create_number(vector->x);
    } else if (strcmp(key, "y") == 0) {
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return lua_value_create_number(vector->y);
    } else if (strcmp(key, "set_direction") == 0) {
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return lua_value_create_cfunc(_ese_vector_lua_set_direction, lua_value_create_vector(vector));
    } else if (strcmp(key, "magnitude") == 0) {
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return lua_value_create_cfunc(_ese_vector_lua_magnitude, lua_value_create_vector(vector));
    } else if (strcmp(key, "normalize") == 0) {
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return lua_value_create_cfunc(_ese_vector_lua_normalize, lua_value_create_vector(vector));
    }
    profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index");
    return lua_value_create_nil();
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
static EseLuaValue* _ese_vector_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_VECTOR_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return lua_value_create_error("result", "newindex requires 3 arguments");
    }

    if (!lua_value_is_vector(argv[0])) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return lua_value_create_error("result", "first argument must be a vector");
    }
    
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return lua_value_create_error("result", "invalid vector");
    }

    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return lua_value_create_error("result", "second argument must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return lua_value_create_error("result", "invalid key");
    }

    if (strcmp(key, "x") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return lua_value_create_error("result", "vector.x must be a number");
        }
        vector->x = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return lua_value_create_nil();
    } else if (strcmp(key, "y") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return lua_value_create_error("result", "vector.y must be a number");
        }
        vector->y = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return lua_value_create_nil();
    }
    profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
    return lua_value_create_error("result", "unknown or unassignable property '%s'", key);
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
static EseLuaValue* _ese_vector_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "tostring requires 1 argument");
    }
    
    if (!lua_value_is_vector(argv[0])) {
        return lua_value_create_error("result", "first argument must be a vector");
    }
    
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        return lua_value_create_string("Vector: (invalid)");
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Vector: %p (x=%.2f, y=%.2f)", (void*)vector, vector->x, vector->y);
    return lua_value_create_string(buf);
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
static EseLuaValue* _ese_vector_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_VECTOR_NEW);

    if (argc != 2) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return lua_value_create_error("result", "Vector.new(number, number) takes 2 arguments");
    }
    
    if (!lua_value_is_number(argv[0])) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return lua_value_create_error("result", "Vector.new(number, number) arguments must be numbers");
    }
    if (!lua_value_is_number(argv[1])) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);  
        return lua_value_create_error("result", "Vector.new(number, number) arguments must be numbers");
    }

    float x = (float)lua_value_get_number(argv[0]);
    float y = (float)lua_value_get_number(argv[1]);

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();
    vector->x = x;
    vector->y = y;
    vector->lua_ref = ESE_LUA_NOREF;
    vector->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_VECTOR_NEW, "vector_lua_new");
    return lua_value_create_vector(vector);
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
static EseLuaValue* _ese_vector_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_VECTOR_ZERO);

    if (argc != 0) {
        profile_cancel(PROFILE_LUA_VECTOR_ZERO);
        return lua_value_create_error("result", "Vector.zero() takes 0 arguments");
    }

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();
    vector->x = 0.0f;
    vector->y = 0.0f;
    vector->lua_ref = ESE_LUA_NOREF;
    vector->lua_ref_count = 0;

    profile_stop(PROFILE_LUA_VECTOR_ZERO, "vector_lua_zero");
    return lua_value_create_vector(vector);
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
static EseLuaValue* _ese_vector_lua_set_direction(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        return lua_value_create_error("result", "vector:set_direction(string, number) takes 2 arguments");
    }

    if (!lua_value_is_string(argv[0]) || !lua_value_is_number(argv[1])) {
        return lua_value_create_error("result", "vector:set_direction(string, number) takes a string and a number");
    }

    // Get vector from upvalue (this function is called as a method)
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        return lua_value_create_error("result", "Invalid EseVector object in set_direction method");
    }
    
    const char *direction = lua_value_get_string(argv[0]);
    float magnitude = (float)lua_value_get_number(argv[1]);

    ese_vector_set_direction(vector, direction, magnitude);
    return lua_value_create_nil();
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
static EseLuaValue* _ese_vector_lua_magnitude(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "vector:magnitude() takes 0 arguments");
    }

    // Get vector from upvalue (this function is called as a method)
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        return lua_value_create_error("result", "Invalid EseVector object in magnitude method");
    }
    
    return lua_value_create_number(ese_vector_magnitude(vector));
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
static EseLuaValue* _ese_vector_lua_normalize(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "vector:normalize() takes 0 arguments");
    }

    // Get vector from upvalue (this function is called as a method)
    EseVector *vector = lua_value_get_vector(argv[0]);
    if (!vector) {
        return lua_value_create_error("result", "Invalid EseVector object in normalize method");
    }
    
    ese_vector_normalize(vector);
    return lua_value_create_nil();
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseVector *ese_vector_create(EseLuaEngine *engine) {
    EseVector *vector = _ese_vector_make();
    vector->state = engine->runtime;
    return vector;
}

EseVector *ese_vector_copy(const EseVector *source) {
    log_assert("VECTOR", source, "ese_vector_copy called with NULL source");

    EseVector *copy = (EseVector *)memory_manager.malloc(sizeof(EseVector), MMTAG_VECTOR);
    copy->x = source->x;
    copy->y = source->y;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_vector_destroy(EseVector *vector) {
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

size_t ese_vector_sizeof(void) {
    return sizeof(EseVector);
}

// Property access
void ese_vector_set_x(EseVector *vector, float x) {
    log_assert("VECTOR", vector, "ese_vector_set_x called with NULL vector");
    vector->x = x;
}

float ese_vector_get_x(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_get_x called with NULL vector");
    return vector->x;
}

void ese_vector_set_y(EseVector *vector, float y) {
    log_assert("VECTOR", vector, "ese_vector_set_y called with NULL vector");
    vector->y = y;
}

float ese_vector_get_y(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_get_y called with NULL vector");
    return vector->y;
}

// Lua-related access
lua_State *ese_vector_get_state(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_get_state called with NULL vector");
    return vector->state;
}

int ese_vector_get_lua_ref(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_get_lua_ref called with NULL vector");
    return vector->lua_ref;
}

int ese_vector_get_lua_ref_count(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_get_lua_ref_count called with NULL vector");
    return vector->lua_ref_count;
}

// Lua integration
void ese_vector_lua_init(EseLuaEngine *engine) {
    log_assert("VECTOR", engine, "ese_vector_lua_init called with NULL engine");

    if (luaL_newmetatable(engine->runtime, VECTOR_PROXY_META)) {
        log_debug("LUA", "Adding entity VectorProxyMeta to engine");
        lua_pushstring(engine->runtime, VECTOR_PROXY_META);
        lua_setfield(engine->runtime, -2, "__name");
        lua_pushcfunction(engine->runtime, _ese_vector_lua_index);
        lua_setfield(engine->runtime, -2, "__index");
        lua_pushcfunction(engine->runtime, _ese_vector_lua_newindex);
        lua_setfield(engine->runtime, -2, "__newindex");
        lua_pushcfunction(engine->runtime, _ese_vector_lua_gc);
        lua_setfield(engine->runtime, -2, "__gc");
        lua_pushcfunction(engine->runtime, _ese_vector_lua_tostring);
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
        lua_pushcfunction(engine->runtime, _ese_vector_lua_new);
        lua_setfield(engine->runtime, -2, "new");
        lua_pushcfunction(engine->runtime, _ese_vector_lua_zero);
        lua_setfield(engine->runtime, -2, "zero");
        lua_setglobal(engine->runtime, "Vector");
    } else {
        lua_pop(engine->runtime, 1);
    }
}

void ese_vector_lua_push(EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_lua_push called with NULL vector");

    if (vector->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseVector **ud = (EseVector **)lua_newuserdata(vector->state, sizeof(EseVector *));
        *ud = vector;

        // Attach metatable
        luaL_getmetatable(vector->state, VECTOR_PROXY_META);
        lua_setmetatable(vector->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(vector->state, LUA_REGISTRYINDEX, vector->lua_ref);
    }
}

EseVector *ese_vector_lua_get(lua_State *L, int idx) {
    log_assert("VECTOR", L, "ese_vector_lua_get called with NULL Lua state");
    
    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseVector **ud = (EseVector **)luaL_testudata(L, idx, VECTOR_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_vector_ref(EseVector *vector, EseLuaEngine *engine) {
    log_assert("VECTOR", vector, "ese_vector_ref called with NULL vector");
    log_assert("VECTOR", engine, "ese_vector_ref called with NULL engine");
    
    if (vector->lua_ref == ESE_LUA_NOREF) {
        // First time referencing - create userdata and store reference
        vector->lua_ref = lua_engine_get_reference(engine, lua_engine_create_userdata(engine, vector, "VectorMeta"));
        vector->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        vector->lua_ref_count++;
    }

    profile_count_add("ese_vector_ref_count");
}

void vector_unref(EseVector *vector) {
    if (!vector) return;
    
    if (vector->lua_ref != ESE_LUA_NOREF && vector->lua_ref_count > 0) {
        vector->lua_ref_count--;
        
        if (vector->lua_ref_count == 0) {
            // No more references - remove from registry
            vector->lua_ref = ESE_LUA_NOREF;
        }
    }

    profile_count_add("vector_unref_count");
}

// Mathematical operations
void ese_vector_set_direction(EseVector *vector, const char *direction, float magnitude) {
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
            default: return;
        }
    }
    
    // Normalize for diagonal directions
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 0.0f) {
        dx /= length;
        dy /= length;
        // Apply magnitude
        vector->x = dx * magnitude;
        vector->y = dy * magnitude;
    } else {
        // Invalid direction - set to zero
        vector->x = 0.0f;
        vector->y = 0.0f;
    }
}

float ese_vector_magnitude(const EseVector *vector) {
    if (!vector) return 0.0f;
    return sqrtf(vector->x * vector->x + vector->y * vector->y);
}

void ese_vector_normalize(EseVector *vector) {
    if (!vector) return;
    
    float magnitude = ese_vector_magnitude(vector);
    if (magnitude > 0.0f) {
        vector->x /= magnitude;
        vector->y /= magnitude;
    }
}
