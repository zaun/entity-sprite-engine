/**
 * @file rect.c
 * @brief Implementation of rectangle type with floating-point coordinates and
 * dimensions
 * @details Implements rectangle operations, collision detection, Lua
 * integration, and JSON serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "types/rect.h"
#include "types/rect_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// The actual EseRect struct definition (private to this file)
typedef struct EseRect {
    float x;        /** The x-coordinate of the rectangle's top-left corner */
    float y;        /** The y-coordinate of the rectangle's top-left corner */
    float width;    /** The width of the rectangle */
    float height;   /** The height of the rectangle */
    float rotation; /** The rotation of the rect around the center point in
                       radians */

    lua_State *state;  /** Lua State this EseRect belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this rect has been referenced in C */

    // Watcher system
    EseRectWatcherCallback *watchers; /** Array of watcher callbacks */
    void **watcher_userdata;          /** Array of userdata for each watcher */
    size_t watcher_count;             /** Number of registered watchers */
    size_t watcher_capacity;          /** Capacity of the watcher arrays */
} EseRect;

/**
 * @brief Simple 2D vector structure for mathematical operations.
 *
 * @details This structure represents a 2D vector with x and y components
 *          for use in collision detection and geometric calculations.
 */
typedef struct {
    float x, y;
} Vec2;

/**
 * @brief Oriented bounding box structure for collision detection.
 *
 * @details This structure represents an oriented bounding box using
 *          center point, normalized axes, and half-widths for efficient
 *          collision detection using the Separating Axis Theorem (SAT).
 */
typedef struct {
    Vec2 c;       /** Center point of the bounding box */
    Vec2 axis[2]; /** Normalized local axes in world space */
    float ext[2]; /** Half-widths along each axis */
} OBB;

/* mini helpers */
static inline float deg_to_rad(float d) { return d * (M_PI / 180.0f); }
static inline float rad_to_deg(float r) { return r * (180.0f / M_PI); }
static inline float dotf(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseRect *_ese_rect_make(void);
static void _ese_rect_to_obb(const EseRect *r, OBB *out);

// Collision detection
static bool _ese_obb_overlap(const OBB *A, const OBB *B);

// Private static setters for Lua state management
static void _ese_rect_set_lua_ref(EseRect *rect, int lua_ref);
static void _ese_rect_set_lua_ref_count(EseRect *rect, int lua_ref_count);
static void _ese_rect_set_state(EseRect *rect, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseRect instance with default values
 *
 * Allocates memory for a new EseRect and initializes all fields to safe
 * defaults. The rect starts at origin (0,0) with zero dimensions and no
 * rotation.
 *
 * @return Pointer to the newly created EseRect, or NULL on allocation failure
 */
static EseRect *_ese_rect_make() {
    EseRect *rect = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_RECT);
    rect->x = 0.0f;
    rect->y = 0.0f;
    rect->width = 0.0f;
    rect->height = 0.0f;
    rect->rotation = 0.0f;
    rect->state = NULL;
    rect->lua_ref = LUA_NOREF;
    rect->lua_ref_count = 0;
    rect->watchers = NULL;
    rect->watcher_userdata = NULL;
    rect->watcher_count = 0;
    rect->watcher_capacity = 0;
    return rect;
}

// Private static setters for Lua state management
static void _ese_rect_set_lua_ref(EseRect *rect, int lua_ref) { rect->lua_ref = lua_ref; }

static void _ese_rect_set_lua_ref_count(EseRect *rect, int lua_ref_count) {
    rect->lua_ref_count = lua_ref_count;
}

static void _ese_rect_set_state(EseRect *rect, lua_State *state) { rect->state = state; }

/**
 * @brief Converts an EseRect to an oriented bounding box (OBB) for collision
 * detection
 *
 * Transforms the axis-aligned rectangle into an oriented bounding box
 * representation using the Separating Axis Theorem (SAT) for efficient
 * collision detection.
 *
 * @param r Pointer to the source EseRect
 * @param out Pointer to the output OBB structure to be filled
 */
static void _ese_rect_to_obb(const EseRect *r, OBB *out) {
    float cx = r->x + r->width * 0.5f;
    float cy = r->y + r->height * 0.5f;
    float a = r->rotation;
    float ca = cosf(a);
    float sa = sinf(a);

    out->c.x = cx;
    out->c.y = cy;
    out->axis[0].x = ca;
    out->axis[0].y = sa;
    out->axis[1].x = -sa;
    out->axis[1].y = ca;
    out->ext[0] = r->width * 0.5f;
    out->ext[1] = r->height * 0.5f;
}

// Collision detection
/**
 * @brief Tests for overlap between two oriented bounding boxes
 *
 * Implements the Separating Axis Theorem (SAT) to determine if two OBBs
 * overlap. Tests all possible separating axes to find a separation or confirm
 * overlap.
 *
 * @param A Pointer to the first OBB
 * @param B Pointer to the second OBB
 * @return true if the OBBs overlap, false otherwise
 */
static bool _ese_obb_overlap(const OBB *A, const OBB *B) {
    const float EPS = 1e-6f;
    Vec2 d = {B->c.x - A->c.x, B->c.y - A->c.y};
    /* test A's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = A->axis[i];
        float ra = A->ext[i];
        float rb =
            B->ext[0] * fabsf(dotf(B->axis[0], axis)) + B->ext[1] * fabsf(dotf(B->axis[1], axis));
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS)
            return false;
    }

    /* test B's axes */
    for (int i = 0; i < 2; ++i) {
        Vec2 axis = B->axis[i];
        float ra =
            A->ext[0] * fabsf(dotf(A->axis[0], axis)) + A->ext[1] * fabsf(dotf(A->axis[1], axis));
        float rb = B->ext[i];
        float dist = fabsf(dotf(d, axis));
        if (dist > ra + rb + EPS)
            return false;
    }

    return true;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseRect *ese_rect_create(EseLuaEngine *engine) {
    log_assert("RECT", engine, "ese_rect_create called with NULL engine");
    EseRect *rect = _ese_rect_make();
    _ese_rect_set_state(rect, engine->runtime);
    return rect;
}

EseRect *ese_rect_copy(const EseRect *source) {
    log_assert("RECT", source, "ese_rect_copy called with NULL source");

    EseRect *copy = (EseRect *)memory_manager.malloc(sizeof(EseRect), MMTAG_RECT);
    copy->x = source->x;
    copy->y = source->y;
    copy->width = source->width;
    copy->height = source->height;
    copy->rotation = source->rotation;
    _ese_rect_set_state(copy, ese_rect_get_state(source));
    _ese_rect_set_lua_ref(copy, LUA_NOREF);
    _ese_rect_set_lua_ref_count(copy, 0);
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_rect_destroy(EseRect *rect) {
    if (!rect)
        return;

    if (ese_rect_get_lua_ref(rect) == LUA_NOREF) {
        // No Lua references, safe to free immediately

        // Free watcher arrays if they exist
        if (rect->watchers) {
            memory_manager.free(rect->watchers);
            rect->watchers = NULL;
        }
        if (rect->watcher_userdata) {
            memory_manager.free(rect->watcher_userdata);
            rect->watcher_userdata = NULL;
        }
        rect->watcher_count = 0;
        rect->watcher_capacity = 0;

        memory_manager.free(rect);
    } else {
        ese_rect_unref(rect);
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
    }
}

size_t ese_rect_sizeof(void) { return sizeof(EseRect); }

/**
 * @brief Serializes an EseRect to a cJSON object.
 *
 * Creates a cJSON object representing the rect with type "RECT"
 * and x, y, width, height, rotation coordinates. Only serializes the
 * coordinate and dimension data, not Lua-related fields.
 *
 * @param rect Pointer to the EseRect object to serialize
 * @return cJSON object representing the rect, or NULL on failure
 */
cJSON *ese_rect_serialize(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_serialize called with NULL rect");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("RECT", "Failed to create cJSON object for rect serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("RECT");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("RECT", "Failed to add type field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add x coordinate
    cJSON *x = cJSON_CreateNumber((double)ese_rect_get_x(rect));
    if (!x || !cJSON_AddItemToObject(json, "x", x)) {
        log_error("RECT", "Failed to add x field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add y coordinate
    cJSON *y = cJSON_CreateNumber((double)ese_rect_get_y(rect));
    if (!y || !cJSON_AddItemToObject(json, "y", y)) {
        log_error("RECT", "Failed to add y field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add width
    cJSON *width = cJSON_CreateNumber((double)ese_rect_get_width(rect));
    if (!width || !cJSON_AddItemToObject(json, "width", width)) {
        log_error("RECT", "Failed to add width field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add height
    cJSON *height = cJSON_CreateNumber((double)ese_rect_get_height(rect));
    if (!height || !cJSON_AddItemToObject(json, "height", height)) {
        log_error("RECT", "Failed to add height field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add rotation
    cJSON *rotation = cJSON_CreateNumber((double)rad_to_deg(ese_rect_get_rotation(rect)));
    if (!rotation || !cJSON_AddItemToObject(json, "rotation", rotation)) {
        log_error("RECT", "Failed to add rotation field to rect serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseRect from a cJSON object.
 *
 * Creates a new EseRect from a cJSON object with type "RECT"
 * and x, y, width, height, rotation coordinates. The rect is created
 * with the specified engine and must be explicitly referenced with
 * ese_rect_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for rect creation
 * @param data cJSON object containing rect data
 * @return Pointer to newly created EseRect object, or NULL on failure
 */
EseRect *ese_rect_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("RECT", data, "ese_rect_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("RECT", "Rect deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "RECT") != 0) {
        log_error("RECT", "Rect deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get x coordinate
    cJSON *x_item = cJSON_GetObjectItem(data, "x");
    if (!x_item || !cJSON_IsNumber(x_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing x field");
        return NULL;
    }

    // Get y coordinate
    cJSON *y_item = cJSON_GetObjectItem(data, "y");
    if (!y_item || !cJSON_IsNumber(y_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing y field");
        return NULL;
    }

    // Get width
    cJSON *width_item = cJSON_GetObjectItem(data, "width");
    if (!width_item || !cJSON_IsNumber(width_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing width field");
        return NULL;
    }

    // Get height
    cJSON *height_item = cJSON_GetObjectItem(data, "height");
    if (!height_item || !cJSON_IsNumber(height_item)) {
        log_error("RECT", "Rect deserialization failed: invalid or missing height field");
        return NULL;
    }

    // Get rotation (optional field)
    cJSON *rotation_item = cJSON_GetObjectItem(data, "rotation");
    float rotation = 0.0f;
    if (rotation_item && cJSON_IsNumber(rotation_item)) {
        rotation = deg_to_rad((float)rotation_item->valuedouble);
    }

    // Create new rect
    EseRect *rect = ese_rect_create(engine);
    ese_rect_set_x(rect, (float)x_item->valuedouble);
    ese_rect_set_y(rect, (float)y_item->valuedouble);
    ese_rect_set_width(rect, (float)width_item->valuedouble);
    ese_rect_set_height(rect, (float)height_item->valuedouble);
    ese_rect_set_rotation(rect, rotation);

    return rect;
}

// Lua integration
void ese_rect_lua_init(EseLuaEngine *engine) { _ese_rect_lua_init(engine); }

void ese_rect_lua_push(EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_lua_push called with NULL rect");

    if (ese_rect_get_lua_ref(rect) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseRect **ud = (EseRect **)lua_newuserdata(ese_rect_get_state(rect), sizeof(EseRect *));
        *ud = rect;

        // Attach metatable
        luaL_getmetatable(ese_rect_get_state(rect), RECT_PROXY_META);
        lua_setmetatable(ese_rect_get_state(rect), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_rect_get_state(rect), LUA_REGISTRYINDEX, ese_rect_get_lua_ref(rect));
    }
}

EseRect *ese_rect_lua_get(lua_State *L, int idx) {
    log_assert("RECT", L, "ese_rect_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseRect **ud = (EseRect **)luaL_testudata(L, idx, RECT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void ese_rect_ref(EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_ref called with NULL rect");

    if (ese_rect_get_lua_ref(rect) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseRect **ud = (EseRect **)lua_newuserdata(ese_rect_get_state(rect), sizeof(EseRect *));
        *ud = rect;

        // Attach metatable
        luaL_getmetatable(ese_rect_get_state(rect), RECT_PROXY_META);
        lua_setmetatable(ese_rect_get_state(rect), -2);

        // Store hard reference to prevent garbage collection
        int ref = luaL_ref(ese_rect_get_state(rect), LUA_REGISTRYINDEX);
        _ese_rect_set_lua_ref(rect, ref);
        _ese_rect_set_lua_ref_count(rect, 1);
    } else {
        // Already referenced - just increment count
        _ese_rect_set_lua_ref_count(rect, ese_rect_get_lua_ref_count(rect) + 1);
    }

    profile_count_add("ese_rect_ref_count");
}

void ese_rect_unref(EseRect *rect) {
    if (!rect)
        return;

    if (ese_rect_get_lua_ref(rect) != LUA_NOREF && ese_rect_get_lua_ref_count(rect) > 0) {
        int new_count = ese_rect_get_lua_ref_count(rect) - 1;
        _ese_rect_set_lua_ref_count(rect, new_count);

        if (new_count == 0) {
            // No more references - remove from registry
            luaL_unref(ese_rect_get_state(rect), LUA_REGISTRYINDEX, ese_rect_get_lua_ref(rect));
            _ese_rect_set_lua_ref(rect, LUA_NOREF);
        }
    }

    profile_count_add("ese_rect_unref_count");
}

// Mathematical operations
bool ese_rect_contains_point(const EseRect *rect, float x, float y) {
    log_assert("RECT", rect, "ese_rect_contains_point called with NULL rect");

    float rotation = ese_rect_get_rotation(rect);
    float rect_x = ese_rect_get_x(rect);
    float rect_y = ese_rect_get_y(rect);
    float width = ese_rect_get_width(rect);
    float height = ese_rect_get_height(rect);

    /* fast AABB path */
    if (fabsf(rotation) < 1e-6f) {
        return (x >= rect_x && x <= rect_x + width && y >= rect_y && y <= rect_y + height);
    }

    /* transform point to rect-local coordinates by rotating around center by
     * -rotation */
    float cx = rect_x + width * 0.5f;
    float cy = rect_y + height * 0.5f;
    float ca = cosf(rotation);
    float sa = sinf(rotation);

    float dx = x - cx;
    float dy = y - cy;

    float localX = ca * dx + sa * dy;
    float localY = -sa * dx + ca * dy;

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    return (localX >= -halfW && localX <= halfW && localY >= -halfH && localY <= halfH);
}

bool ese_rect_intersects(const EseRect *rect1, const EseRect *rect2) {
    log_assert("RECT", rect1, "ese_rect_intersects called with NULL first rect");
    log_assert("RECT", rect2, "ese_rect_intersects called with NULL second rect");

    float rot1 = ese_rect_get_rotation(rect1);
    float rot2 = ese_rect_get_rotation(rect2);

    /* fast AABB path if both rotations are effectively zero */
    if (fabsf(rot1) < 1e-6f && fabsf(rot2) < 1e-6f) {
        float x1 = ese_rect_get_x(rect1);
        float y1 = ese_rect_get_y(rect1);
        float w1 = ese_rect_get_width(rect1);
        float h1 = ese_rect_get_height(rect1);
        float x2 = ese_rect_get_x(rect2);
        float y2 = ese_rect_get_y(rect2);
        float w2 = ese_rect_get_width(rect2);
        float h2 = ese_rect_get_height(rect2);

        return !(x1 > x2 + w2 || x2 > x1 + w1 || y1 > y2 + h2 || y2 > y1 + h1);
    }

    OBB a, b;
    _ese_rect_to_obb(rect1, &a);
    _ese_rect_to_obb(rect2, &b);

    return _ese_obb_overlap(&a, &b);
}

float ese_rect_area(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_area called with NULL rect");
    return ese_rect_get_width(rect) * ese_rect_get_height(rect);
}

// Property access
void ese_rect_set_rotation(EseRect *rect, float radians) {
    log_assert("RECT", rect, "ese_rect_set_rotation called with NULL rect");
    rect->rotation = radians;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_rotation(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_rotation called with NULL rect");
    return rect->rotation;
}

void ese_rect_set_x(EseRect *rect, float x) {
    log_assert("RECT", rect, "ese_rect_set_x called with NULL rect");
    rect->x = x;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_x(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_x called with NULL rect");
    return rect->x;
}

void ese_rect_set_y(EseRect *rect, float y) {
    log_assert("RECT", rect, "ese_rect_set_y called with NULL rect");
    rect->y = y;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_y(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_y called with NULL rect");
    return rect->y;
}

void ese_rect_set_width(EseRect *rect, float width) {
    log_assert("RECT", rect, "ese_rect_set_width called with NULL rect");
    rect->width = width;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_width(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_width called with NULL rect");
    return rect->width;
}

void ese_rect_set_height(EseRect *rect, float height) {
    log_assert("RECT", rect, "ese_rect_set_height called with NULL rect");
    rect->height = height;
    _ese_rect_notify_watchers(rect);
}

float ese_rect_get_height(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_height called with NULL rect");
    return rect->height;
}

// Lua-related access
lua_State *ese_rect_get_state(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_state called with NULL rect");
    return rect->state;
}

int ese_rect_get_lua_ref(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_lua_ref called with NULL rect");
    return rect->lua_ref;
}

int ese_rect_get_lua_ref_count(const EseRect *rect) {
    log_assert("RECT", rect, "ese_rect_get_lua_ref_count called with NULL rect");
    return rect->lua_ref_count;
}

// Watcher system
bool ese_rect_add_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "ese_rect_add_watcher called with NULL rect");
    log_assert("RECT", callback, "ese_rect_add_watcher called with NULL callback");

    // Initialize watcher arrays if this is the first watcher
    if (rect->watcher_count == 0) {
        rect->watcher_capacity = 4; // Start with capacity for 4 watchers
        rect->watchers = memory_manager.malloc(
            sizeof(EseRectWatcherCallback) * rect->watcher_capacity, MMTAG_RECT);
        rect->watcher_userdata =
            memory_manager.malloc(sizeof(void *) * rect->watcher_capacity, MMTAG_RECT);
        rect->watcher_count = 0;
    }

    // Expand arrays if needed
    if (rect->watcher_count >= rect->watcher_capacity) {
        size_t new_capacity = rect->watcher_capacity * 2;
        EseRectWatcherCallback *new_watchers = memory_manager.realloc(
            rect->watchers, sizeof(EseRectWatcherCallback) * new_capacity, MMTAG_RECT);
        void **new_userdata = memory_manager.realloc(rect->watcher_userdata,
                                                     sizeof(void *) * new_capacity, MMTAG_RECT);

        if (!new_watchers || !new_userdata)
            return false;

        rect->watchers = new_watchers;
        rect->watcher_userdata = new_userdata;
        rect->watcher_capacity = new_capacity;
    }

    // Add the new watcher
    rect->watchers[rect->watcher_count] = callback;
    rect->watcher_userdata[rect->watcher_count] = userdata;
    rect->watcher_count++;

    return true;
}

bool ese_rect_remove_watcher(EseRect *rect, EseRectWatcherCallback callback, void *userdata) {
    log_assert("RECT", rect, "ese_rect_remove_watcher called with NULL rect");
    log_assert("RECT", callback, "ese_rect_remove_watcher called with NULL callback");

    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i] == callback && rect->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < rect->watcher_count - 1; j++) {
                rect->watchers[j] = rect->watchers[j + 1];
                rect->watcher_userdata[j] = rect->watcher_userdata[j + 1];
            }
            rect->watcher_count--;
            return true;
        }
    }

    return false;
}

// Helper function to notify all watchers
/**
 * @brief Notifies all registered watchers of a rect change
 *
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated rect and their associated userdata. This is called whenever any
 * rect property (x, y, width, height, rotation) is modified.
 *
 * @param rect Pointer to the EseRect that has changed
 */
void _ese_rect_notify_watchers(EseRect *rect) {
    if (!rect || rect->watcher_count == 0)
        return;

    for (size_t i = 0; i < rect->watcher_count; i++) {
        if (rect->watchers[i]) {
            rect->watchers[i](rect, rect->watcher_userdata[i]);
        }
    }
}

// Lua integration
