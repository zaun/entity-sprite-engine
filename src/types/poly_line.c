#include "types/poly_line.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/color.h"
#include "types/point.h"
#include "types/poly_line_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The actual EsePolyLine struct definition (private to this file)
typedef struct EsePolyLine {
    EsePolyLineType type;   /** The type of polyline (OPEN, CLOSED, FILLED) */
    float stroke_width;     /** The stroke width */
    EseColor *stroke_color; /** The stroke color */
    EseColor *fill_color;   /** The fill color */

    float *points;         /** Array of point coordinates (x1, y1, x2, y2, ...) */
    size_t point_count;    /** Number of points */
    size_t point_capacity; /** Capacity of the points array (in number of
                              points, not floats) */

    lua_State *state;  /** Lua State this EsePolyLine belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this polyline has been referenced in
                        * C
                        */

    // Watcher system
    EsePolyLineWatcherCallback *watchers; /** Array of watcher callbacks */
    void **watcher_userdata;              /** Array of userdata for each watcher */
    size_t watcher_count;                 /** Number of registered watchers */
    size_t watcher_capacity;              /** Capacity of the watcher arrays */
} EsePolyLine;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers

// Watcher system

// Private static setters for Lua state management
static void _ese_poly_line_set_lua_ref(EsePolyLine *poly_line, int lua_ref);
static void _ese_poly_line_set_lua_ref_count(EsePolyLine *poly_line, int lua_ref_count);
static void _ese_poly_line_set_state(EsePolyLine *poly_line, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EsePolyLine instance with default values
 *
 * Allocates memory for a new EsePolyLine and initializes all fields to safe
 * defaults. The polyline starts as OPEN type with no points, default stroke
 * width, and default colors.
 *
 * @return Pointer to the newly created EsePolyLine, or NULL on allocation
 * failure
 */
EsePolyLine *_ese_poly_line_make() {
    EsePolyLine *poly_line =
        (EsePolyLine *)memory_manager.malloc(sizeof(EsePolyLine), MMTAG_POLY_LINE);
    poly_line->type = POLY_LINE_OPEN;
    poly_line->stroke_width = 1.0f;
    poly_line->stroke_color = NULL;
    poly_line->fill_color = NULL;
    poly_line->points = NULL;
    poly_line->point_count = 0;
    poly_line->point_capacity = 0;
    poly_line->state = NULL;
    poly_line->lua_ref = LUA_NOREF;
    poly_line->lua_ref_count = 0;
    poly_line->watchers = NULL;
    poly_line->watcher_userdata = NULL;
    poly_line->watcher_count = 0;
    poly_line->watcher_capacity = 0;
    return poly_line;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a polyline change
 *
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated polyline and their associated userdata. This is called whenever any
 * property of the polyline is modified.
 *
 * @param poly_line Pointer to the EsePolyLine that has changed
 */
void _ese_poly_line_notify_watchers(EsePolyLine *poly_line) {
    if (!poly_line || poly_line->watcher_count == 0)
        return;

    for (size_t i = 0; i < poly_line->watcher_count; i++) {
        if (poly_line->watchers[i]) {
            poly_line->watchers[i](poly_line, poly_line->watcher_userdata[i]);
        }
    }
}

// Private static setters for Lua state management
static void _ese_poly_line_set_lua_ref(EsePolyLine *poly_line, int lua_ref) {
    poly_line->lua_ref = lua_ref;
}

static void _ese_poly_line_set_lua_ref_count(EsePolyLine *poly_line, int lua_ref_count) {
    poly_line->lua_ref_count = lua_ref_count;
}

static void _ese_poly_line_set_state(EsePolyLine *poly_line, lua_State *state) {
    poly_line->state = state;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EsePolyLine *ese_poly_line_create(EseLuaEngine *engine) {
    log_assert("POLY_LINE", engine, "poly_line_create called with NULL engine");
    EsePolyLine *poly_line = _ese_poly_line_make();
    poly_line->state = engine->runtime;
    return poly_line;
}

EsePolyLine *ese_poly_line_copy(const EsePolyLine *source) {
    log_assert("POLY_LINE", source, "poly_line_copy called with NULL source");

    EsePolyLine *copy = (EsePolyLine *)memory_manager.malloc(sizeof(EsePolyLine), MMTAG_POLY_LINE);
    copy->type = source->type;
    copy->stroke_width = source->stroke_width;
    copy->stroke_color = NULL;
    copy->fill_color = NULL;
    copy->state = source->state;
    copy->lua_ref = LUA_NOREF;
    copy->lua_ref_count = 0;
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;

    // Copy points array
    copy->point_count = source->point_count;
    copy->point_capacity = source->point_capacity;
    if (source->point_count > 0) {
        copy->points =
            memory_manager.malloc(sizeof(float) * source->point_count * 2, MMTAG_POLY_LINE);
        memcpy(copy->points, source->points, sizeof(float) * source->point_count * 2);
    } else {
        copy->points = NULL;
    }

    // Colors: increase ref counts if present
    if (source->stroke_color) {
        copy->stroke_color = source->stroke_color;
        ese_color_ref(copy->stroke_color);
    }
    if (source->fill_color) {
        copy->fill_color = source->fill_color;
        ese_color_ref(copy->fill_color);
    }

    return copy;
}

void ese_poly_line_destroy(EsePolyLine *poly_line) {
    if (!poly_line)
        return;

    if (poly_line->lua_ref == LUA_NOREF) {
        // No Lua references, safe to free immediately

        // Free points array
        if (poly_line->points) {
            memory_manager.free(poly_line->points);
            poly_line->points = NULL;
        }
        poly_line->point_count = 0;
        poly_line->point_capacity = 0;

        if (poly_line->stroke_color) {
            ese_color_unref(poly_line->stroke_color);
            ese_color_destroy(poly_line->stroke_color);
            poly_line->stroke_color = NULL;
        }
        if (poly_line->fill_color) {
            ese_color_unref(poly_line->fill_color);
            ese_color_destroy(poly_line->fill_color);
            poly_line->fill_color = NULL;
        }

        // Free watcher arrays if they exist
        if (poly_line->watchers) {
            memory_manager.free(poly_line->watchers);
            poly_line->watchers = NULL;
        }
        if (poly_line->watcher_userdata) {
            memory_manager.free(poly_line->watcher_userdata);
            poly_line->watcher_userdata = NULL;
        }
        poly_line->watcher_count = 0;
        poly_line->watcher_capacity = 0;

        memory_manager.free(poly_line);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_poly_line_unref(poly_line);
    }
}

// Property access
EsePolyLineType ese_poly_line_get_type(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_type called with NULL poly_line");
    return poly_line->type;
}

float ese_poly_line_get_stroke_width(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_width called with NULL poly_line");
    return poly_line->stroke_width;
}

EseColor *ese_poly_line_get_stroke_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_stroke_color called with NULL poly_line");
    return poly_line->stroke_color;
}

EseColor *ese_poly_line_get_fill_color(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_fill_color called with NULL poly_line");
    return poly_line->fill_color;
}

// Points collection management
bool ese_poly_line_add_point(EsePolyLine *poly_line, EsePoint *point) {
    log_assert("POLY_LINE", poly_line, "poly_line_add_point called with NULL poly_line");
    log_assert("POLY_LINE", point, "poly_line_add_point called with NULL point");

    // Initialize points array if this is the first point
    if (poly_line->point_count == 0) {
        poly_line->point_capacity = 4; // Start with capacity for 4 points
        poly_line->points =
            memory_manager.malloc(sizeof(float) * poly_line->point_capacity * 2, MMTAG_POLY_LINE);
        poly_line->point_count = 0;
    }

    // Expand array if needed
    if (poly_line->point_count >= poly_line->point_capacity) {
        size_t new_capacity = poly_line->point_capacity * 2;
        float *new_points = memory_manager.realloc(
            poly_line->points, sizeof(float) * new_capacity * 2, MMTAG_POLY_LINE);

        if (!new_points)
            return false;

        poly_line->points = new_points;
        poly_line->point_capacity = new_capacity;
    }

    // Add the new point coordinates
    size_t index = poly_line->point_count * 2;
    poly_line->points[index] = ese_point_get_x(point);
    poly_line->points[index + 1] = ese_point_get_y(point);
    poly_line->point_count++;

    _ese_poly_line_notify_watchers(poly_line);
    return true;
}

bool ese_poly_line_remove_point(EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_remove_point called with NULL poly_line");

    if (index >= poly_line->point_count) {
        return false;
    }

    // Shift remaining points (each point is 2 floats)
    for (size_t i = index; i < poly_line->point_count - 1; i++) {
        size_t src_index = (i + 1) * 2;
        size_t dst_index = i * 2;
        poly_line->points[dst_index] = poly_line->points[src_index];
        poly_line->points[dst_index + 1] = poly_line->points[src_index + 1];
    }

    poly_line->point_count--;
    _ese_poly_line_notify_watchers(poly_line);
    return true;
}

EsePoint *ese_poly_line_get_point(const EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point called with NULL poly_line");

    if (index >= poly_line->point_count) {
        return NULL;
    }

    // Create a temporary EsePoint from the stored coordinates
    size_t coord_index = index * 2;
    float x = poly_line->points[coord_index];
    float y = poly_line->points[coord_index + 1];

    EseLuaEngine *engine =
        (EseLuaEngine *)lua_engine_get_registry_key(poly_line->state, LUA_ENGINE_KEY);
    EsePoint *point = ese_point_create(engine);
    ese_point_set_x(point, x);
    ese_point_set_y(point, y);

    return point;
}

size_t ese_poly_line_get_point_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point_count called with NULL poly_line");
    return poly_line->point_count;
}

void ese_poly_line_clear_points(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_clear_points called with NULL poly_line");

    // No need to destroy individual points since we're storing floats directly
    poly_line->point_count = 0;
    _ese_poly_line_notify_watchers(poly_line);
}

const float *ese_poly_line_get_points(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_points called with NULL poly_line");
    return poly_line->points;
}

// Lua-related access
lua_State *ese_poly_line_get_state(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_state called with NULL poly_line");
    return poly_line->state;
}

int ese_poly_line_get_lua_ref(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref called with NULL poly_line");
    return poly_line->lua_ref;
}

int ese_poly_line_get_lua_ref_count(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_lua_ref_count called with NULL poly_line");
    return poly_line->lua_ref_count;
}

float ese_poly_line_get_point_x(const EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point_x called with NULL poly_line");
    if (index >= poly_line->point_count) {
        return 0.0f;
    }
    size_t coord_index = index * 2;
    return poly_line->points ? poly_line->points[coord_index] : 0.0f;
}

float ese_poly_line_get_point_y(const EsePolyLine *poly_line, size_t index) {
    log_assert("POLY_LINE", poly_line, "poly_line_get_point_y called with NULL poly_line");
    if (index >= poly_line->point_count) {
        return 0.0f;
    }
    size_t coord_index = index * 2;
    return poly_line->points ? poly_line->points[coord_index + 1] : 0.0f;
}

void ese_poly_line_set_type(EsePolyLine *poly_line, EsePolyLineType type) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_type called with NULL poly_line");
    poly_line->type = type;
    _ese_poly_line_notify_watchers(poly_line);
}

void ese_poly_line_set_stroke_width(EsePolyLine *poly_line, float stroke_width) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_width called with NULL poly_line");
    poly_line->stroke_width = stroke_width;
    _ese_poly_line_notify_watchers(poly_line);
}

void ese_poly_line_set_stroke_color(EsePolyLine *poly_line, EseColor *stroke_color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_stroke_color called with NULL poly_line");
    poly_line->stroke_color = stroke_color;
    _ese_poly_line_notify_watchers(poly_line);
}

void ese_poly_line_set_fill_color(EsePolyLine *poly_line, EseColor *fill_color) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_fill_color called with NULL poly_line");
    poly_line->fill_color = fill_color;
    _ese_poly_line_notify_watchers(poly_line);
}

void ese_poly_line_set_state(EsePolyLine *poly_line, lua_State *state) {
    log_assert("POLY_LINE", poly_line, "poly_line_set_state called with NULL poly_line");
    poly_line->state = state;
}

// Watcher system
bool ese_poly_line_add_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback,
                               void *userdata) {
    log_assert("POLY_LINE", poly_line, "poly_line_add_watcher called with NULL poly_line");
    log_assert("POLY_LINE", callback, "poly_line_add_watcher called with NULL callback");

    // Initialize watcher arrays if this is the first watcher
    if (poly_line->watcher_count == 0) {
        poly_line->watcher_capacity = 4; // Start with capacity for 4 watchers
        poly_line->watchers = memory_manager.malloc(
            sizeof(EsePolyLineWatcherCallback) * poly_line->watcher_capacity, MMTAG_POLY_LINE);
        poly_line->watcher_userdata =
            memory_manager.malloc(sizeof(void *) * poly_line->watcher_capacity, MMTAG_POLY_LINE);
        poly_line->watcher_count = 0;
    }

    // Expand arrays if needed
    if (poly_line->watcher_count >= poly_line->watcher_capacity) {
        size_t new_capacity = poly_line->watcher_capacity * 2;
        EsePolyLineWatcherCallback *new_watchers = memory_manager.realloc(
            poly_line->watchers, sizeof(EsePolyLineWatcherCallback) * new_capacity,
            MMTAG_POLY_LINE);
        void **new_userdata = memory_manager.realloc(
            poly_line->watcher_userdata, sizeof(void *) * new_capacity, MMTAG_POLY_LINE);

        if (!new_watchers || !new_userdata)
            return false;

        poly_line->watchers = new_watchers;
        poly_line->watcher_userdata = new_userdata;
        poly_line->watcher_capacity = new_capacity;
    }

    // Add the new watcher
    poly_line->watchers[poly_line->watcher_count] = callback;
    poly_line->watcher_userdata[poly_line->watcher_count] = userdata;
    poly_line->watcher_count++;

    return true;
}

bool ese_poly_line_remove_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback,
                                  void *userdata) {
    log_assert("POLY_LINE", poly_line, "poly_line_remove_watcher called with NULL poly_line");
    log_assert("POLY_LINE", callback, "poly_line_remove_watcher called with NULL callback");

    for (size_t i = 0; i < poly_line->watcher_count; i++) {
        if (poly_line->watchers[i] == callback && poly_line->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < poly_line->watcher_count - 1; j++) {
                poly_line->watchers[j] = poly_line->watchers[j + 1];
                poly_line->watcher_userdata[j] = poly_line->watcher_userdata[j + 1];
            }
            poly_line->watcher_count--;
            return true;
        }
    }

    return false;
}

// Lua integration
void ese_poly_line_lua_init(EseLuaEngine *engine) {
    log_assert("POLY_LINE", engine, "poly_line_lua_init called with NULL engine");

    _ese_poly_line_lua_init(engine);
}

void ese_poly_line_lua_push(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_lua_push called with NULL poly_line");

    if (poly_line->lua_ref == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(poly_line->state, sizeof(EsePolyLine *));
        *ud = poly_line;

        // Attach metatable
        luaL_getmetatable(poly_line->state, POLY_LINE_PROXY_META);
        lua_setmetatable(poly_line->state, -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(poly_line->state, LUA_REGISTRYINDEX, poly_line->lua_ref);
    }
}

EsePolyLine *ese_poly_line_lua_get(lua_State *L, int idx) {
    log_assert("POLY_LINE", L, "poly_line_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EsePolyLine **ud = (EsePolyLine **)luaL_testudata(L, idx, POLY_LINE_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

/**
 * @brief Lua static method for creating EsePolyLine from JSON string
 */
static int _ese_poly_line_lua_from_json(lua_State *L) {
    int argc = lua_gettop(L);
    if (argc != 1) {
        return luaL_error(L, "PolyLine.fromJSON(string) takes 1 argument");
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "PolyLine.fromJSON(string) argument must be a string");
    }

    const char *json_str = lua_tostring(L, 1);
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        log_error("POLY_LINE", "PolyLine.fromJSON: failed to parse JSON string: %s",
                  json_str ? json_str : "NULL");
        return luaL_error(L, "PolyLine.fromJSON: invalid JSON string");
    }

    EseLuaEngine *engine = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);
    if (!engine) {
        cJSON_Delete(json);
        return luaL_error(L, "PolyLine.fromJSON: no engine available");
    }

    EsePolyLine *poly_line = ese_poly_line_deserialize(engine, json);
    cJSON_Delete(json);
    if (!poly_line) {
        return luaL_error(L, "PolyLine.fromJSON: failed to deserialize polyline");
    }

    ese_poly_line_lua_push(poly_line);
    return 1;
}

void ese_poly_line_ref(EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "poly_line_ref called with NULL poly_line");

    if (poly_line->lua_ref == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EsePolyLine **ud = (EsePolyLine **)lua_newuserdata(poly_line->state, sizeof(EsePolyLine *));
        *ud = poly_line;

        // Attach metatable
        luaL_getmetatable(poly_line->state, POLY_LINE_PROXY_META);
        lua_setmetatable(poly_line->state, -2);

        // Store hard reference to prevent garbage collection
        poly_line->lua_ref = luaL_ref(poly_line->state, LUA_REGISTRYINDEX);
        poly_line->lua_ref_count = 1;
    } else {
        // Already referenced - just increment count
        poly_line->lua_ref_count++;
    }

    profile_count_add("poly_line_ref_count");
}

void ese_poly_line_unref(EsePolyLine *poly_line) {
    if (!poly_line)
        return;

    if (poly_line->lua_ref != LUA_NOREF && poly_line->lua_ref_count > 0) {
        poly_line->lua_ref_count--;

        if (poly_line->lua_ref_count == 0) {
            // No more references - remove from registry
            luaL_unref(poly_line->state, LUA_REGISTRYINDEX, poly_line->lua_ref);
            poly_line->lua_ref = LUA_NOREF;
        }
    }

    profile_count_add("poly_line_unref_count");
}

size_t ese_poly_line_sizeof(void) { return sizeof(EsePolyLine); }

/**
 * @brief Serializes an EsePolyLine to a cJSON object.
 *
 * Creates a cJSON object representing the polyline with type "POLY_LINE"
 * and all properties including points, colors, and styling. Only serializes the
 * geometric and styling data, not Lua-related fields.
 *
 * @param poly_line Pointer to the EsePolyLine object to serialize
 * @return cJSON object representing the polyline, or NULL on failure
 */
cJSON *ese_poly_line_serialize(const EsePolyLine *poly_line) {
    log_assert("POLY_LINE", poly_line, "ese_poly_line_serialize called with NULL poly_line");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("POLY_LINE", "Failed to create cJSON object for poly_line serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("POLY_LINE");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("POLY_LINE", "Failed to add type field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add polyline type
    const char *type_str;
    switch (poly_line->type) {
    case POLY_LINE_OPEN:
        type_str = "OPEN";
        break;
    case POLY_LINE_CLOSED:
        type_str = "CLOSED";
        break;
    case POLY_LINE_FILLED:
        type_str = "FILLED";
        break;
    default:
        type_str = "UNKNOWN";
        break;
    }
    cJSON *poly_type = cJSON_CreateString(type_str);
    if (!poly_type || !cJSON_AddItemToObject(json, "poly_type", poly_type)) {
        log_error("POLY_LINE", "Failed to add poly_type field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add stroke_width
    cJSON *stroke_width = cJSON_CreateNumber((double)poly_line->stroke_width);
    if (!stroke_width || !cJSON_AddItemToObject(json, "stroke_width", stroke_width)) {
        log_error("POLY_LINE", "Failed to add stroke_width field to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Serialize stroke_color if present
    if (poly_line->stroke_color) {
        cJSON *stroke_color_json = ese_color_serialize(poly_line->stroke_color);
        if (!stroke_color_json || !cJSON_AddItemToObject(json, "stroke_color", stroke_color_json)) {
            log_error("POLY_LINE", "Failed to add stroke_color field to poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }
    }

    // Serialize fill_color if present
    if (poly_line->fill_color) {
        cJSON *fill_color_json = ese_color_serialize(poly_line->fill_color);
        if (!fill_color_json || !cJSON_AddItemToObject(json, "fill_color", fill_color_json)) {
            log_error("POLY_LINE", "Failed to add fill_color field to poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }
    }

    // Add points array
    cJSON *points_array = cJSON_CreateArray();
    if (!points_array || !cJSON_AddItemToObject(json, "points", points_array)) {
        log_error("POLY_LINE", "Failed to add points array to poly_line serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add each point as an array of [x, y] coordinates
    for (size_t i = 0; i < poly_line->point_count; i++) {
        cJSON *point_array = cJSON_CreateArray();
        if (!point_array) {
            log_error("POLY_LINE", "Failed to create point array for poly_line serialization");
            cJSON_Delete(json);
            return NULL;
        }

        cJSON *x = cJSON_CreateNumber((double)poly_line->points[i * 2]);
        cJSON *y = cJSON_CreateNumber((double)poly_line->points[i * 2 + 1]);

        if (!x || !y || !cJSON_AddItemToArray(point_array, x) ||
            !cJSON_AddItemToArray(point_array, y)) {
            log_error("POLY_LINE", "Failed to add point coordinates to poly_line serialization");
            cJSON_Delete(point_array);
            cJSON_Delete(json);
            return NULL;
        }

        if (!cJSON_AddItemToArray(points_array, point_array)) {
            log_error("POLY_LINE", "Failed to add point to points array in "
                                   "poly_line serialization");
            cJSON_Delete(point_array);
            cJSON_Delete(json);
            return NULL;
        }
    }

    return json;
}

/**
 * @brief Deserializes an EsePolyLine from a cJSON object.
 *
 * Creates a new EsePolyLine from a cJSON object with type "POLY_LINE"
 * and all properties including points, colors, and styling. The polyline is
 * created with the specified engine and must be explicitly referenced with
 * ese_poly_line_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for polyline creation
 * @param data cJSON object containing polyline data
 * @return Pointer to newly created EsePolyLine object, or NULL on failure
 */
EsePolyLine *ese_poly_line_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("POLY_LINE", data, "ese_poly_line_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) ||
        strcmp(type_item->valuestring, "POLY_LINE") != 0) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get poly_type field
    cJSON *poly_type_item = cJSON_GetObjectItem(data, "poly_type");
    if (!poly_type_item || !cJSON_IsString(poly_type_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or "
                               "missing poly_type field");
        return NULL;
    }

    EsePolyLineType poly_type;
    if (strcmp(poly_type_item->valuestring, "OPEN") == 0) {
        poly_type = POLY_LINE_OPEN;
    } else if (strcmp(poly_type_item->valuestring, "CLOSED") == 0) {
        poly_type = POLY_LINE_CLOSED;
    } else if (strcmp(poly_type_item->valuestring, "FILLED") == 0) {
        poly_type = POLY_LINE_FILLED;
    } else {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid poly_type value");
        return NULL;
    }

    // Get stroke_width field
    cJSON *stroke_width_item = cJSON_GetObjectItem(data, "stroke_width");
    if (!stroke_width_item || !cJSON_IsNumber(stroke_width_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or "
                               "missing stroke_width field");
        return NULL;
    }

    // Get points array
    cJSON *points_item = cJSON_GetObjectItem(data, "points");
    if (!points_item || !cJSON_IsArray(points_item)) {
        log_error("POLY_LINE", "PolyLine deserialization failed: invalid or missing points array");
        return NULL;
    }

    size_t point_count = cJSON_GetArraySize(points_item);
    if (point_count == 0) {
        log_error("POLY_LINE", "PolyLine deserialization failed: empty points array");
        return NULL;
    }

    // Create new polyline
    EsePolyLine *poly_line = ese_poly_line_create(engine);

    // Set polyline type and stroke width
    poly_line->type = poly_type;
    poly_line->stroke_width = (float)stroke_width_item->valuedouble;

    // Initialize points array
    poly_line->point_capacity = point_count;
    poly_line->point_count = 0;
    poly_line->points =
        memory_manager.malloc(sizeof(float) * poly_line->point_capacity * 2, MMTAG_POLY_LINE);

    // Add points
    for (size_t i = 0; i < point_count; i++) {
        cJSON *point_item = cJSON_GetArrayItem(points_item, i);
        if (!point_item || !cJSON_IsArray(point_item) || cJSON_GetArraySize(point_item) != 2) {
            log_error("POLY_LINE", "PolyLine deserialization failed: invalid point format");
            ese_poly_line_destroy(poly_line);
            return NULL;
        }

        cJSON *x_item = cJSON_GetArrayItem(point_item, 0);
        cJSON *y_item = cJSON_GetArrayItem(point_item, 1);

        if (!x_item || !cJSON_IsNumber(x_item) || !y_item || !cJSON_IsNumber(y_item)) {
            log_error("POLY_LINE", "PolyLine deserialization failed: invalid point coordinates");
            ese_poly_line_destroy(poly_line);
            return NULL;
        }

        // Add point coordinates directly (ese_poly_line_add_point just extracts
        // x,y)
        size_t index = poly_line->point_count * 2;
        poly_line->points[index] = (float)x_item->valuedouble;
        poly_line->points[index + 1] = (float)y_item->valuedouble;
        poly_line->point_count++;
    }

    // Deserialize stroke_color if present
    cJSON *stroke_color_item = cJSON_GetObjectItem(data, "stroke_color");
    if (stroke_color_item && cJSON_IsObject(stroke_color_item)) {
        EseColor *stroke_color = ese_color_deserialize(engine, stroke_color_item);
        if (stroke_color) {
            ese_poly_line_set_stroke_color(poly_line, stroke_color);
            // Don't destroy - polyline takes ownership
        }
    }

    // Deserialize fill_color if present
    cJSON *fill_color_item = cJSON_GetObjectItem(data, "fill_color");
    if (fill_color_item && cJSON_IsObject(fill_color_item)) {
        EseColor *fill_color = ese_color_deserialize(engine, fill_color_item);
        if (fill_color) {
            ese_poly_line_set_fill_color(poly_line, fill_color);
            // Don't destroy - polyline takes ownership
        }
    }

    return poly_line;
}
