#include <string.h>
#include <stdio.h>
#include <math.h>
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "types/types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseArc *_ese_arc_make(void);

// Lua metamethods
static EseLuaValue* _ese_arc_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua constructors
static EseLuaValue* _ese_arc_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// Lua methods
static EseLuaValue* _ese_arc_lua_contains_point(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_intersects_rect(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_get_length(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);
static EseLuaValue* _ese_arc_lua_get_point_at_angle(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseArc instance with default values
 * 
 * Allocates memory for a new EseArc and initializes all fields to safe defaults.
 * The arc starts at origin (0,0) with unit radius, full circle angles, and no Lua state or references.
 * 
 * @return Pointer to the newly created EseArc, or NULL on allocation failure
 */
static EseArc *_ese_arc_make() {
    EseArc *arc = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    arc->x = 0.0f;
    arc->y = 0.0f;
    arc->radius = 1.0f;
    arc->start_angle = 0.0f;
    arc->end_angle = 2.0f * M_PI;

    arc->engine = NULL;
    arc->lua_ref = ESE_LUA_NOREF;
    arc->lua_ref_count = 0;
    return arc;
}

// Lua metamethods
/**
 * @brief Lua garbage collection metamethod for EseArc
 * 
 * Handles cleanup when a Lua proxy table for an EseArc is garbage collected.
 * Only frees the underlying EseArc if it has no C-side references.
 * 
 * @param L Lua state
 * @return Always returns 0 (no values pushed)
 */
static EseLuaValue* _ese_arc_lua_gc(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return NULL;
    }

    // Get the arc from the first argument
    if (!lua_value_is_arc(argv[0])) {
        return NULL;
    }

    EseArc *arc = lua_value_get_arc(argv[0]);
    if (arc) {
        // If lua_ref == ESE_LUA_NOREF, there are no more references to this arc, 
        // so we can free it.
        // If lua_ref != ESE_LUA_NOREF, this arc was referenced from C and should not be freed.
        if (arc->lua_ref == ESE_LUA_NOREF) {
            ese_arc_destroy(arc);
        }
    }

    return NULL;
}

/**
 * @brief Lua __index metamethod for EseArc property access
 * 
 * Provides read access to arc properties (x, y, radius, start_angle, end_angle) from Lua.
 * When a Lua script accesses arc.x, arc.y, etc., this function is called to retrieve the values.
 * Also provides access to methods like contains_point, intersects_rect, get_length, and get_point_at_angle.
 * 
 * @param engine EseLuaEngine pointer
 * @param argc Number of arguments
 * @param argv Array of EseLuaValue arguments
 * @return EseLuaValue* containing the requested property or method
 */
static EseLuaValue* _ese_arc_lua_index(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_ARC_INDEX);
    
    if (argc != 2) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the arc from the first argument (should be arc)
    if (!lua_value_is_arc(argv[0])) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return lua_value_create_nil("result");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return lua_value_create_nil("result");
    }
    
    // Get the key from the second argument (should be string)
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return lua_value_create_nil("result");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    if (!key) {
        profile_cancel(PROFILE_LUA_ARC_INDEX);
        return lua_value_create_nil("result");
    }

    if (strcmp(key, "x") == 0) {
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return lua_value_create_number("result", arc->x);
    } else if (strcmp(key, "y") == 0) {
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return lua_value_create_number("result", arc->y);
    } else if (strcmp(key, "radius") == 0) {
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return lua_value_create_number("result", arc->radius);
    } else if (strcmp(key, "start_angle") == 0) {
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return lua_value_create_number("result", arc->start_angle);
    } else if (strcmp(key, "end_angle") == 0) {
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (getter)");
        return lua_value_create_number("result", arc->end_angle);
    } else if (strcmp(key, "contains_point") == 0) {
        EseLuaValue *arc_value = lua_value_create_arc("arc", arc);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return lua_value_create_cfunc("contains_point", _ese_arc_lua_contains_point, arc_value);
    } else if (strcmp(key, "intersects_rect") == 0) {
        EseLuaValue *arc_value = lua_value_create_arc("arc", arc);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return lua_value_create_cfunc("intersects_rect", _ese_arc_lua_intersects_rect, arc_value);
    } else if (strcmp(key, "get_length") == 0) {
        EseLuaValue *arc_value = lua_value_create_arc("arc", arc);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return lua_value_create_cfunc("get_length", _ese_arc_lua_get_length, arc_value);
    } else if (strcmp(key, "get_point_at_angle") == 0) {
        EseLuaValue *arc_value = lua_value_create_arc("arc", arc);
        profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (method)");
        return lua_value_create_cfunc("get_point_at_angle", _ese_arc_lua_get_point_at_angle, arc_value);
    }
    
    profile_stop(PROFILE_LUA_ARC_INDEX, "ese_arc_lua_index (invalid)");
    return lua_value_create_nil("result");
}

/**
 * @brief Lua __newindex metamethod for EseArc property assignment
 * 
 * Provides write access to arc properties (x, y, radius, start_angle, end_angle) from Lua.
 * When a Lua script assigns to arc.x, arc.y, etc., this function is called to update the values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 0)
 */
static EseLuaValue* _ese_arc_lua_newindex(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_ARC_NEWINDEX);
    
    if (argc != 3) {
        profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
        return lua_value_create_error("result", "arc.x, arc.y, arc.radius, arc.start_angle, arc.end_angle assignment requires exactly 3 arguments");
    }

    // Get the arc from the first argument (self)
    if (!lua_value_is_arc(argv[0])) {
        profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
        return lua_value_create_error("result", "arc property assignment: first argument must be an Arc");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
        return lua_value_create_error("result", "arc property assignment: arc is NULL");
    }
    
    // Get the key
    if (!lua_value_is_string(argv[1])) {
        profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
        return lua_value_create_error("result", "arc property assignment: property name (x, y, radius, start_angle, end_angle) must be a string");
    }
    
    const char *key = lua_value_get_string(argv[1]);
    
    if (strcmp(key, "x") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return lua_value_create_error("result", "arc.x must be a number");
        }
        arc->x = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "y") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return lua_value_create_error("result", "arc.y must be a number");
        }
        arc->y = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "radius") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return lua_value_create_error("result", "arc.radius must be a number");
        }
        arc->radius = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "start_angle") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return lua_value_create_error("result", "arc.start_angle must be a number");
        }
        arc->start_angle = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return NULL;
    } else if (strcmp(key, "end_angle") == 0) {
        if (!lua_value_is_number(argv[2])) {
            profile_cancel(PROFILE_LUA_ARC_NEWINDEX);
            return lua_value_create_error("result", "arc.end_angle must be a number");
        }
        arc->end_angle = (float)lua_value_get_number(argv[2]);
        profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (setter)");
        return NULL;
    }
    
    profile_stop(PROFILE_LUA_ARC_NEWINDEX, "ese_arc_lua_newindex (invalid)");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "unknown or unassignable property '%s'", key);
    return lua_value_create_error("result", error_msg);
}

/**
 * @brief Lua __tostring metamethod for EseArc string representation
 * 
 * Converts an EseArc to a human-readable string for debugging and display.
 * The format includes the memory address and current x,y,radius,start_angle,end_angle values.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1)
 */
static EseLuaValue* _ese_arc_lua_tostring(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_string("result", "Arc: (invalid)");
    }

    if (!lua_value_is_arc(argv[0])) {
        return lua_value_create_string("result", "Arc: (invalid)");
    }

    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        return lua_value_create_string("result", "Arc: (invalid)");
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Arc: %p (x=%.2f, y=%.2f, r=%.2f, start=%.2f, end=%.2f)", 
             (void*)arc, arc->x, arc->y, arc->radius, arc->start_angle, arc->end_angle);
    
    return lua_value_create_string("result", buf);
}

// Lua constructors
/**
 * @brief Lua constructor function for creating new EseArc instances
 * 
 * Creates a new EseArc from Lua with specified center, radius, and angle parameters.
 * This function is called when Lua code executes `Arc.new(x, y, radius, start_angle, end_angle)`.
 * It validates the arguments, creates the underlying EseArc, and returns a proxy table
 * that provides access to the arc's properties and methods.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static EseLuaValue* _ese_arc_lua_new(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_ARC_NEW);
    float x = 0.0f, y = 0.0f, radius = 1.0f, start_angle = 0.0f, end_angle = 2.0f * M_PI;

    if (argc == 5) {
        if (!lua_value_is_number(argv[0]) || !lua_value_is_number(argv[1]) || 
            !lua_value_is_number(argv[2]) || !lua_value_is_number(argv[3]) || 
            !lua_value_is_number(argv[4])) {
            profile_cancel(PROFILE_LUA_ARC_NEW);
            return lua_value_create_error("result", "all arguments must be numbers");
        }
        x = (float)lua_value_get_number(argv[0]);
        y = (float)lua_value_get_number(argv[1]);
        radius = (float)lua_value_get_number(argv[2]);
        start_angle = (float)lua_value_get_number(argv[3]);
        end_angle = (float)lua_value_get_number(argv[4]);
    } else if (argc != 0) {
        profile_cancel(PROFILE_LUA_ARC_NEW);
        return lua_value_create_error("result", "new() takes 0 or 5 arguments (x, y, radius, start_angle, end_angle)");
    }

    // Create the arc using the standard creation function
    EseArc *arc = _ese_arc_make();
    arc->engine = engine;
    arc->x = x;
    arc->y = y;
    arc->radius = radius;
    arc->start_angle = start_angle;
    arc->end_angle = end_angle;
    
    // Create userdata using the engine API
    EseArc **ud = (EseArc **)lua_engine_create_userdata(engine, "ArcMeta", sizeof(EseArc *));
    *ud = arc;

    profile_stop(PROFILE_LUA_ARC_NEW, "ese_arc_lua_new");
    return lua_value_create_arc("result", arc);
}

/**
 * @brief Lua constructor function for creating EseArc at origin
 * 
 * Creates a new EseArc at the origin (0,0) with unit radius and full circle angles from Lua.
 * This function is called when Lua code executes `Arc.zero()`.
 * It's a convenience constructor for creating arcs at the default position and parameters.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the proxy table)
 */
static EseLuaValue* _ese_arc_lua_zero(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    profile_start(PROFILE_LUA_ARC_ZERO);
    
    if (argc != 0) {
        profile_cancel(PROFILE_LUA_ARC_ZERO);
        return lua_value_create_error("result", "zero() takes no arguments");
    }
    
    // Create the arc using the standard creation function
    EseArc *arc = _ese_arc_make();  // We'll set the state manually
    
    // Create userdata using the engine API
    EseArc **ud = (EseArc **)lua_engine_create_userdata(engine, "ArcMeta", sizeof(EseArc *));
    *ud = arc;

    profile_stop(PROFILE_LUA_ARC_ZERO, "ese_arc_lua_zero");
    return lua_value_create_arc("result", arc);
}

// Lua methods
/**
 * @brief Lua method for testing if a point is contained within the arc
 * 
 * Tests whether the specified (x,y) point lies within the arc bounds using
 * distance and angle calculations with optional tolerance.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static EseLuaValue* _ese_arc_lua_contains_point(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc < 3 || argc > 4) {
        return lua_value_create_error("result", "arc:contains_point(x, y [, tolerance]) requires 2 or 3 arguments");
    }
    
    // First argument is the arc instance (from upvalue)
    if (!lua_value_is_arc(argv[0])) {
        return lua_value_create_error("result", "first argument must be an Arc");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        return lua_value_create_error("result", "arc is NULL");
    }
    
    if (!lua_value_is_number(argv[1]) || !lua_value_is_number(argv[2])) {
        return lua_value_create_error("result", "arc:contains_point(x, y [, tolerance]) x and y must be numbers");
    }
    
    if (argc == 4 && !lua_value_is_number(argv[3])) {
        return lua_value_create_error("result", "arc:contains_point(x, y [, tolerance]) tolerance must be a number");
    }

    float x = (float)lua_value_get_number(argv[1]);
    float y = (float)lua_value_get_number(argv[2]);
    float tolerance = 0.1f;
    
    if (argc == 4) {
        tolerance = (float)lua_value_get_number(argv[3]);
    }
    
    bool result = ese_arc_contains_point(arc, x, y, tolerance);
    return lua_value_create_bool("result", result);
}

/**
 * @brief Lua method for testing arc-rectangle intersection
 * 
 * Tests whether the arc intersects with a given rectangle using
 * bounding box intersection algorithms.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - boolean result)
 */
static EseLuaValue* _ese_arc_lua_intersects_rect(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        return lua_value_create_error("result", "arc:intersects_rect(rect) requires exactly 1 argument");
    }

    // First argument is the arc instance (from upvalue)
    if (!lua_value_is_arc(argv[0])) {
        return lua_value_create_error("result", "first argument must be an Arc");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        return lua_value_create_error("result", "arc is NULL");
    }
    
    if (!lua_value_is_rect(argv[1])) {
        return lua_value_create_error("result", "arc:intersects_rect(rect) argument must be a Rect object");
    }
    
    EseRect *rect = lua_value_get_rect(argv[1]);
    if (!rect) {
        return lua_value_create_error("result", "rect is NULL");
    }
    
    bool result = ese_arc_intersects_rect(arc, rect);
    return lua_value_create_bool("result", result);
}

/**
 * @brief Lua method for calculating arc length
 * 
 * Calculates and returns the arc length using the formula: radius * angle_difference.
 * Handles angle wrapping automatically.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (always 1 - the length value)
 */
static EseLuaValue* _ese_arc_lua_get_length(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 1) {
        return lua_value_create_error("result", "arc:get_length() takes no arguments");
    }

    // First argument is the arc instance (from upvalue)
    if (!lua_value_is_arc(argv[0])) {
        return lua_value_create_error("result", "first argument must be an Arc");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        return lua_value_create_error("result", "arc is NULL");
    }
    
    float length = ese_arc_get_length(arc);
    return lua_value_create_number("result", length);
}

/**
 * @brief Lua method for getting a point along the arc at a specified angle
 * 
 * Calculates the coordinates of a point that lies along the arc at the given angle.
 * Returns success status and x,y coordinates if the angle is within the arc's range.
 * 
 * @param L Lua state
 * @return Number of values pushed onto the stack (3 for success: true, x, y; 1 for failure: false)
 */
static EseLuaValue* _ese_arc_lua_get_point_at_angle(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]) {
    if (argc != 2) {
        return lua_value_create_error("result", "arc:get_point_at_angle(angle) requires exactly 1 argument");
    }

    // First argument is the arc instance (from upvalue)
    if (!lua_value_is_arc(argv[0])) {
        return lua_value_create_error("result", "first argument must be an Arc");
    }
    
    EseArc *arc = lua_value_get_arc(argv[0]);
    if (!arc) {
        return lua_value_create_error("result", "arc is NULL");
    }
    
    if (!lua_value_is_number(argv[1])) {
        return lua_value_create_error("result", "arc:get_point_at_angle(angle) requires a number");
    }
    
    float angle = (float)lua_value_get_number(argv[1]);
    float x, y;
    bool success = ese_arc_get_point_at_angle(arc, angle, &x, &y);
    
    if (success) {
        // Return a table with x, y coordinates
        EseLuaValue *result = lua_value_create_table("result");
        EseLuaValue *x_val = lua_value_create_number("x", x);
        EseLuaValue *y_val = lua_value_create_number("y", y);
        
        lua_value_set_table_prop(result, x_val);
        lua_value_set_table_prop(result, y_val);
        
        lua_value_destroy(x_val);
        lua_value_destroy(y_val);
        
        return result;
    } else {
        return lua_value_create_string("result", "failed");
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseArc *ese_arc_create() {
    EseArc *arc = _ese_arc_make();
    return arc;
}

EseArc *ese_arc_copy(const EseArc *source) {
    if (source == NULL) {
        return NULL;
    }

    EseArc *copy = (EseArc *)memory_manager.malloc(sizeof(EseArc), MMTAG_ARC);
    copy->x = source->x;
    copy->y = source->y;
    copy->radius = source->radius;
    copy->start_angle = source->start_angle;
    copy->end_angle = source->end_angle;

    copy->engine = NULL;
    copy->lua_ref = ESE_LUA_NOREF;
    copy->lua_ref_count = 0;
    return copy;
}

void ese_arc_destroy(EseArc *arc) {
    if (!arc) return;
    
    if (arc->lua_ref == ESE_LUA_NOREF) {
        // No Lua references, safe to free immediately
        memory_manager.free(arc);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_arc_unref(arc);
    }
}

// Lua integration
void ese_arc_lua_init(EseLuaEngine *engine) {
    // Add the metatable using the new API
    lua_engine_add_metatable(engine, "ArcMeta", 
                            _ese_arc_lua_index, 
                            _ese_arc_lua_newindex, 
                            _ese_arc_lua_gc, 
                            _ese_arc_lua_tostring);
    
    // Create global Arc table with constructors
    const char *function_names[] = {"new", "zero"};
    EseLuaCFunction functions[] = {_ese_arc_lua_new, _ese_arc_lua_zero};
    lua_engine_add_globaltable(engine, "Arc", 2, function_names, functions);
}

void ese_arc_lua_push(EseLuaEngine *engine, EseArc *arc) {
    log_assert("ARC", engine, "ese_arc_lua_push called with NULL engine");
    log_assert("ARC", arc, "ese_arc_lua_push called with NULL arc");

    if (arc->engine == NULL) {
        arc->engine = engine;
    }
    log_assert("ARC", arc->engine == engine, "ese_arc_lua_push called with arc that is already referenced to a different engine");


    if (arc->lua_ref == ESE_LUA_NOREF) {
        // Lua-owned: create a new userdata using the engine API
        EseArc **ud = (EseArc **)lua_engine_create_userdata(arc->engine, "ArcMeta", sizeof(EseArc *));
        *ud = arc;
    } else {
        // C-owned: get from registry using the engine API
        lua_engine_get_registry_value(engine, arc->lua_ref);
    }
}

EseArc *ese_arc_lua_get(EseLuaEngine *engine, int idx) {
    log_assert("ARC", engine, "ese_arc_lua_get called with NULL engine");
    log_assert("ARC", arc->engine == engine, "ese_arc_lua_get called with arc that is already referenced to a different engine");

    // Check if the value at idx is userdata
    if (!lua_engine_is_userdata(engine, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseArc **ud = (EseArc **)lua_engine_test_userdata(engine, idx, "ArcMeta");
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void ese_arc_ref(EseLuaEngine *engine, EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_ref called with NULL arc");
    
    if (arc->lua_ref == ESE_LUA_NOREF) {
        // Get the reference from the engine's Lua state
        if (arc->engine == NULL) {
            arc->engine = engine;
        }
        log_assert("ARC", arc->engine == engine, "ese_arc_ref called with arc that is already referenced to a different engine");

        // First time referencing - create userdata and store reference
        EseArc **ud = (EseArc **)lua_engine_create_userdata(engine, "ArcMeta", sizeof(EseArc *));
        *ud = arc;

        arc->lua_ref = lua_engine_get_reference(engine);
        arc->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        arc->lua_ref_count++;
    }

    profile_count_add("ese_arc_ref_count");
}

void ese_arc_unref(EseArc *arc) {
    log_assert("ARC", arc, "ese_arc_unref called with NULL arc");
    log_assert("ARC", arc->engine, "ese_arc_unref called with arc that is not referenced to an engine");
    
    if (arc->lua_ref != ESE_LUA_NOREF && arc->lua_ref_count > 0) {
        arc->lua_ref_count--;
        
        if (arc->lua_ref_count == 0) {
            // No more references - remove from registry
            lua_engine_remove_reference(arc->engine, arc->lua_ref);
            arc->lua_ref = ESE_LUA_NOREF;
        }
    }

    profile_count_add("ese_arc_unref_count");
}

// Mathematical operations
bool ese_arc_contains_point(const EseArc *arc, float x, float y, float tolerance) {
    if (!arc) return false;
    
    // Calculate distance from point to arc center
    float dx = x - arc->x;
    float dy = y - arc->y;
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Check if point is within radius tolerance
    if (fabsf(distance - arc->radius) > tolerance) {
        return false;
    }
    
    // Check if point is within angle range
    float angle = atan2f(dy, dx);
    if (angle < 0) {
        angle += 2.0f * M_PI;
    }
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    return (angle >= start && angle <= end);
}

float ese_arc_get_length(const EseArc *arc) {
    if (!arc) return 0.0f;
    
    float angle_diff = arc->end_angle - arc->start_angle;
    if (angle_diff < 0) {
        angle_diff += 2.0f * M_PI;
    }
    
    return arc->radius * angle_diff;
}

bool ese_arc_get_point_at_angle(const EseArc *arc, float angle, float *out_x, float *out_y) {
    if (!arc || !out_x || !out_y) return false;
    
    // Normalize start and end angles
    float start = arc->start_angle;
    float end = arc->end_angle;
    
    if (end < start) {
        end += 2.0f * M_PI;
    }
    
    // Normalize input angle
    while (angle < start) {
        angle += 2.0f * M_PI;
    }
    
    if (angle > end) {
        return false;
    }
    
    *out_x = arc->x + arc->radius * cosf(angle);
    *out_y = arc->y + arc->radius * sinf(angle);
    
    return true;
}

bool ese_arc_intersects_rect(const EseArc *arc, const EseRect *rect) {
    if (!arc || !rect) return false;
    
    // Simple bounding box check for now
    // This could be enhanced with more sophisticated arc-rectangle intersection
    float ese_arc_left = arc->x - arc->radius;
    float ese_arc_right = arc->x + arc->radius;
    float ese_arc_top = arc->y - arc->radius;
    float ese_arc_bottom = arc->y + arc->radius;
    
    float rect_left = ese_rect_get_x(rect);
    float rect_right = ese_rect_get_x(rect) + ese_rect_get_width(rect);
    float rect_top = ese_rect_get_y(rect);
    float rect_bottom = ese_rect_get_y(rect) + ese_rect_get_height(rect);
    
    return !(ese_arc_right < rect_left || ese_arc_left > rect_right ||
             ese_arc_bottom < rect_top || ese_arc_top > rect_bottom);
}
