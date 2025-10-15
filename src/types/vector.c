#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/vector.h"
#include "vendor/json/cJSON.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseVector
 * 
 * @details This structure stores the x and y components of a vector in 2D space.
 */
struct EseVector {
    float x;            /** The x-component of the vector */
    float y;            /** The y-component of the vector */

    lua_State *state;   /** Lua State this EseVector belongs to */
    int lua_ref;        /** Lua registry reference to its own proxy table */
    int lua_ref_count;  /** Number of times this vector has been referenced in C */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseVector *_ese_vector_make(void);

// Lua metamethods
static int _ese_vector_lua_gc(lua_State *L);
static int _ese_vector_lua_index(lua_State *L);
static int _ese_vector_lua_newindex(lua_State *L);
static int _ese_vector_lua_tostring(lua_State *L);

// Lua constructors
static int _ese_vector_lua_new(lua_State *L);
static int _ese_vector_lua_zero(lua_State *L);

// Lua methods
static int _ese_vector_lua_set_direction(lua_State *L);
static int _ese_vector_lua_magnitude(lua_State *L);
static int _ese_vector_lua_normalize(lua_State *L);
static int _ese_vector_lua_to_json(lua_State *L);
static int _ese_vector_lua_from_json(lua_State *L);

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
static int _ese_vector_lua_gc(lua_State *L) {
    // Get from userdata
    EseVector **ud = (EseVector **)luaL_testudata(L, 1, VECTOR_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseVector *vector = *ud;
    if (vector) {
        // If lua_ref == LUA_NOREF, there are no more references to this vector, 
        // so we can free it.
        // If lua_ref != LUA_NOREF, this vector was referenced from C and should not be freed.
        if (vector->lua_ref == LUA_NOREF) {
            ese_vector_destroy(vector);
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
static int _ese_vector_lua_index(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_INDEX);
    EseVector *vector = ese_vector_lua_get(L, 1);
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
        lua_pushcclosure(L, _ese_vector_lua_set_direction, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "magnitude") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_magnitude, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "normalize") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_normalize, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (getter)");
        return 1;
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushlightuserdata(L, vector);
        lua_pushcclosure(L, _ese_vector_lua_to_json, 1);
        profile_stop(PROFILE_LUA_VECTOR_INDEX, "vector_lua_index (method)");
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
static int _ese_vector_lua_newindex(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEWINDEX);
    EseVector *vector = ese_vector_lua_get(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!vector || !key) {
        profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
        return 0;
    }

    if (strcmp(key, "x") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            profile_cancel(PROFILE_LUA_VECTOR_NEWINDEX);
            return luaL_error(L, "vector.x must be a number");
        }
        vector->x = (float)lua_tonumber(L, 3);
        profile_stop(PROFILE_LUA_VECTOR_NEWINDEX, "vector_lua_newindex (setter)");
        return 0;
    } else if (strcmp(key, "y") == 0) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
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
static int _ese_vector_lua_tostring(lua_State *L) {
    EseVector *vector = ese_vector_lua_get(L, 1);

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
static int _ese_vector_lua_new(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_NEW);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "Vector.new(number, number) takes 2 arguments");
    }
    
    if (lua_type(L, 1) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);
        return luaL_error(L, "Vector.new(number, number) arguments must be numbers");
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        profile_cancel(PROFILE_LUA_VECTOR_NEW);  
        return luaL_error(L, "Vector.new(number, number) arguments must be numbers");
    }

    float x = (float)lua_tonumber(L, 1);
    float y = (float)lua_tonumber(L, 2);

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();
    vector->x = x;
    vector->y = y;
    vector->state = L;
    
    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, VECTOR_PROXY_META);
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
static int _ese_vector_lua_zero(lua_State *L) {
    profile_start(PROFILE_LUA_VECTOR_ZERO);

    // Get argument count
    int argc = lua_gettop(L);
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_VECTOR_ZERO);
        return luaL_error(L, "Vector.zero() takes 0 arguments");
    }

    // Create the vector using the standard creation function
    EseVector *vector = _ese_vector_make();  // We'll set the state manually
    vector->state = L;
    
    // Create userdata directly
    EseVector **ud = (EseVector **)lua_newuserdata(L, sizeof(EseVector *));
    *ud = vector;

    // Attach metatable
    luaL_getmetatable(L, VECTOR_PROXY_META);
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
static int _ese_vector_lua_set_direction(lua_State *L) {
    // Get argument count
    int n_args = lua_gettop(L);
    if (n_args == 3) {
        if (lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TNUMBER) {
            return luaL_error(L, "vector:magnitude(string, number) takes a string and a number");
        }
    } else {
        return luaL_error(L, "vector:set_direction(string, number) takes 2 arguments");
    }

    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in set_direction method");
    }
    
    const char *direction = lua_tostring(L, 2);
    float magnitude = (float)lua_tonumber(L, 3);

    ese_vector_set_direction(vector, direction, magnitude);
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
static int _ese_vector_lua_magnitude(lua_State *L) {
    // Get argument count
    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "vector:magnitude() takes 0 arguments");
    }

    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in magnitude method");
    }
    
    lua_pushnumber(L, ese_vector_magnitude(vector));
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
static int _ese_vector_lua_normalize(lua_State *L) {
    // Get argument count
    int n_args = lua_gettop(L);
    if (n_args != 1) {
        return luaL_error(L, "vector:normalize() takes 0 arguments");
    }

    EseVector *vector = (EseVector *)lua_touserdata(L, lua_upvalueindex(1));
    if (!vector) {
        return luaL_error(L, "Invalid EseVector object in normalize method");
    }
    
    ese_vector_normalize(vector);
    return 0;
}

/**
 * @brief Lua instance method for converting EseVector to JSON string
 */
static int _ese_vector_lua_to_json(lua_State *L) {
    EseVector *vector = ese_vector_lua_get(L, 1);
    if (!vector) {
        return luaL_error(L, "Vector:toJSON() called on invalid vector");
    }

    cJSON *json = ese_vector_serialize(vector);
    if (!json) {
        return luaL_error(L, "Vector:toJSON() failed to serialize vector");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "Vector:toJSON() failed to convert to string");
    }

    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}

/**
 * @brief Lua static method for creating EseVector from JSON string
 */
static int _ese_vector_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "Vector.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "Vector.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("VECTOR", "Vector.fromJSON: failed to parse JSON string: %s", json_str ? json_str : "NULL");
        return luaL_error(L, "Vector.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "Vector.fromJSON: no engine available");
    }

    EseVector *vector = ese_vector_deserialize(engine, json);
    cJSON_Delete(json);
    if (!vector) {
        return luaL_error(L, "Vector.fromJSON: failed to deserialize vector");
    }

    ese_vector_lua_push(vector);
    return 1;
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

/**
 * @brief Serializes an EseVector to a cJSON object.
 *
 * Creates a cJSON object representing the vector with type "VECTOR"
 * and x, y components. Only serializes the
 * vector data, not Lua-related fields.
 *
 * @param vector Pointer to the EseVector object to serialize
 * @return cJSON object representing the vector, or NULL on failure
 */
cJSON *ese_vector_serialize(const EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_serialize called with NULL vector");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("VECTOR", "Failed to create cJSON object for vector serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("VECTOR");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("VECTOR", "Failed to add type field to vector serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x component
    cJSON *x = cJSON_CreateNumber((double)vector->x);
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("VECTOR", "Failed to add x field to vector serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y component
    cJSON *y = cJSON_CreateNumber((double)vector->y);
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("VECTOR", "Failed to add y field to vector serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseVector from a cJSON object.
 *
 * Creates a new EseVector from a cJSON object with type "VECTOR"
 * and x, y components. The vector is created
 * with the specified engine and must be explicitly referenced with
 * ese_vector_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for vector creation
 * @param data cJSON object containing vector data
 * @return Pointer to newly created EseVector object, or NULL on failure
 */
EseVector *ese_vector_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("VECTOR", data, "ese_vector_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("VECTOR", "Vector deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "VECTOR") != 0) {
        log_error("VECTOR", "Vector deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x component
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("VECTOR", "Vector deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y component
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("VECTOR", "Vector deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Create new vector
    EseVector *vector = ese_vector_create(engine);
    ese_vector_set_x(vector, (float)x_item->valuedouble);
    ese_vector_set_y(vector, (float)y_item->valuedouble);

    return vector;
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

    // Create metatable
    lua_engine_new_object_meta(engine, VECTOR_PROXY_META, 
        _ese_vector_lua_index, 
        _ese_vector_lua_newindex, 
        _ese_vector_lua_gc, 
        _ese_vector_lua_tostring);
    
    // Create global Vector table with functions
    const char *keys[] = {"new", "zero", "fromJSON"};
    lua_CFunction functions[] = {_ese_vector_lua_new, _ese_vector_lua_zero, _ese_vector_lua_from_json};
    lua_engine_new_object(engine, "Vector", 3, keys, functions);
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

void ese_vector_ref(EseVector *vector) {
    log_assert("VECTOR", vector, "ese_vector_ref called with NULL vector");
    
    if (vector->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseVector **ud = (EseVector **)lua_newuserdata(vector->state, sizeof(EseVector *));
        *ud = vector;

        // Attach metatable
        luaL_getmetatable(vector->state, VECTOR_PROXY_META);
        lua_setmetatable(vector->state, -2);

        // Store hard reference to prevent garbage collection
        vector->lua_ref = luaL_ref(vector->state, LUA_REGISTRYINDEX);
        vector->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        vector->lua_ref_count++;
    }

    profile_count_add("ese_vector_ref_count");
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
