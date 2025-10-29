#include "types/color.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "types/color_lua.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The actual EseColor struct definition (private to this file)
typedef struct EseColor {
    float r; /** The red component of the color (0.0-1.0) */
    float g; /** The green component of the color (0.0-1.0) */
    float b; /** The blue component of the color (0.0-1.0) */
    float a; /** The alpha component of the color (0.0-1.0) */

    lua_State *state;  /** Lua State this EseColor belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this color has been referenced in C
                        */

    // Watcher system
    EseColorWatcherCallback *watchers; /** Array of watcher callbacks */
    void **watcher_userdata;           /** Array of userdata for each watcher */
    size_t watcher_count;              /** Number of registered watchers */
    size_t watcher_capacity;           /** Capacity of the watcher arrays */
} EseColor;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// Core helpers
static EseColor *_ese_color_make(void);

// Watcher system
static void _ese_color_notify_watchers(EseColor *color);

// Private setters
static void _ese_color_set_lua_ref(EseColor *color, int lua_ref);
static void _ese_color_set_lua_ref_count(EseColor *color, int lua_ref_count);
static void _ese_color_set_state(EseColor *color, lua_State *state);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// Core helpers
/**
 * @brief Creates a new EseColor instance with default values
 *
 * Allocates memory for a new EseColor and initializes all fields to safe
 * defaults. The color starts at black (0,0,0,1) with no Lua state or watchers.
 *
 * @return Pointer to the newly created EseColor, or NULL on allocation failure
 */
static EseColor *_ese_color_make() {
    EseColor *color = (EseColor *)memory_manager.malloc(sizeof(EseColor), MMTAG_COLOR);
    color->r = 0.0f;
    color->g = 0.0f;
    color->b = 0.0f;
    color->a = 1.0f;
    _ese_color_set_state(color, NULL);
    _ese_color_set_lua_ref(color, LUA_NOREF);
    _ese_color_set_lua_ref_count(color, 0);
    color->watchers = NULL;
    color->watcher_userdata = NULL;
    color->watcher_count = 0;
    color->watcher_capacity = 0;
    return color;
}

/**
 * @brief Sets the Lua registry reference for the color (private)
 *
 * @param color Pointer to the EseColor object
 * @param lua_ref The new Lua registry reference value
 */
static void _ese_color_set_lua_ref(EseColor *color, int lua_ref) {
    log_assert("COLOR", color != NULL, "_ese_color_set_lua_ref: color cannot be NULL");
    color->lua_ref = lua_ref;
}

/**
 * @brief Sets the Lua reference count for the color (private)
 *
 * @param color Pointer to the EseColor object
 * @param lua_ref_count The new Lua reference count value
 */
static void _ese_color_set_lua_ref_count(EseColor *color, int lua_ref_count) {
    log_assert("COLOR", color != NULL, "_ese_color_set_lua_ref_count: color cannot be NULL");
    color->lua_ref_count = lua_ref_count;
}

/**
 * @brief Sets the Lua state associated with the color (private)
 *
 * @param color Pointer to the EseColor object
 * @param state The new Lua state value
 */
static void _ese_color_set_state(EseColor *color, lua_State *state) {
    log_assert("COLOR", color != NULL, "_ese_color_set_state: color cannot be NULL");
    color->state = state;
}

// Watcher system
/**
 * @brief Notifies all registered watchers of a color change
 *
 * Iterates through all registered watcher callbacks and invokes them with the
 * updated color and their associated userdata. This is called whenever the
 * color's r, g, b, or a components are modified.
 *
 * @param color Pointer to the EseColor that has changed
 */
static void _ese_color_notify_watchers(EseColor *color) {
    if (!color || color->watcher_count == 0)
        return;

    for (size_t i = 0; i < color->watcher_count; i++) {
        if (color->watchers[i]) {
            color->watchers[i](color, color->watcher_userdata[i]);
        }
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseColor *ese_color_create(EseLuaEngine *engine) {
    log_assert("COLOR", engine, "ese_color_create called with NULL engine");
    EseColor *color = _ese_color_make();
    _ese_color_set_state(color, engine->runtime);
    return color;
}

EseColor *ese_color_copy(const EseColor *source) {
    log_assert("COLOR", source, "ese_color_copy called with NULL source");

    EseColor *copy = (EseColor *)memory_manager.malloc(sizeof(EseColor), MMTAG_COLOR);
    copy->r = ese_color_get_r(source);
    copy->g = ese_color_get_g(source);
    copy->b = ese_color_get_b(source);
    copy->a = ese_color_get_a(source);
    _ese_color_set_state(copy, ese_color_get_state(source));
    _ese_color_set_lua_ref(copy, LUA_NOREF);
    _ese_color_set_lua_ref_count(copy, 0);
    copy->watchers = NULL;
    copy->watcher_userdata = NULL;
    copy->watcher_count = 0;
    copy->watcher_capacity = 0;
    return copy;
}

void ese_color_destroy(EseColor *color) {
    if (!color)
        return;

    if (ese_color_get_lua_ref(color) == LUA_NOREF) {
        // No Lua references, safe to free immediately

        // Free watcher arrays if they exist
        if (color->watchers) {
            memory_manager.free(color->watchers);
            color->watchers = NULL;
        }
        if (color->watcher_userdata) {
            memory_manager.free(color->watcher_userdata);
            color->watcher_userdata = NULL;
        }
        color->watcher_count = 0;
        color->watcher_capacity = 0;

        memory_manager.free(color);
    } else {
        // Don't free memory here - let Lua GC handle it
        // As the script may still have a reference to it.
        ese_color_unref(color);
    }
}

// Property access
void ese_color_set_r(EseColor *color, float r) {
    log_assert("COLOR", color, "ese_color_set_r called with NULL color");
    color->r = r;
    _ese_color_notify_watchers(color);
}

float ese_color_get_r(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_r called with NULL color");
    return color->r;
}

void ese_color_set_g(EseColor *color, float g) {
    log_assert("COLOR", color, "ese_color_set_g called with NULL color");
    color->g = g;
    _ese_color_notify_watchers(color);
}

float ese_color_get_g(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_g called with NULL color");
    return color->g;
}

void ese_color_set_b(EseColor *color, float b) {
    log_assert("COLOR", color, "ese_color_set_b called with NULL color");
    color->b = b;
    _ese_color_notify_watchers(color);
}

float ese_color_get_b(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_b called with NULL color");
    return color->b;
}

void ese_color_set_a(EseColor *color, float a) {
    log_assert("COLOR", color, "ese_color_set_a called with NULL color");
    color->a = a;
    _ese_color_notify_watchers(color);
}

float ese_color_get_a(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_a called with NULL color");
    return color->a;
}

// Lua-related access
lua_State *ese_color_get_state(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_state called with NULL color");
    return color->state;
}

int ese_color_get_lua_ref(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_lua_ref called with NULL color");
    return color->lua_ref;
}

int ese_color_get_lua_ref_count(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_get_lua_ref_count called with NULL color");
    return color->lua_ref_count;
}

// Watcher system
bool ese_color_add_watcher(EseColor *color, EseColorWatcherCallback callback, void *userdata) {
    log_assert("COLOR", color, "ese_color_add_watcher called with NULL color");
    log_assert("COLOR", callback, "ese_color_add_watcher called with NULL callback");

    // Initialize watcher arrays if this is the first watcher
    if (color->watcher_count == 0) {
        color->watcher_capacity = 4; // Start with capacity for 4 watchers
        color->watchers = memory_manager.malloc(
            sizeof(EseColorWatcherCallback) * color->watcher_capacity, MMTAG_COLOR);
        color->watcher_userdata =
            memory_manager.malloc(sizeof(void *) * color->watcher_capacity, MMTAG_COLOR);
        color->watcher_count = 0;
    }

    // Expand arrays if needed
    if (color->watcher_count >= color->watcher_capacity) {
        size_t new_capacity = color->watcher_capacity * 2;
        EseColorWatcherCallback *new_watchers = memory_manager.realloc(
            color->watchers, sizeof(EseColorWatcherCallback) * new_capacity, MMTAG_COLOR);
        void **new_userdata = memory_manager.realloc(color->watcher_userdata,
                                                     sizeof(void *) * new_capacity, MMTAG_COLOR);

        if (!new_watchers || !new_userdata)
            return false;

        color->watchers = new_watchers;
        color->watcher_userdata = new_userdata;
        color->watcher_capacity = new_capacity;
    }

    // Add the new watcher
    color->watchers[color->watcher_count] = callback;
    color->watcher_userdata[color->watcher_count] = userdata;
    color->watcher_count++;

    return true;
}

bool ese_color_remove_watcher(EseColor *color, EseColorWatcherCallback callback, void *userdata) {
    log_assert("COLOR", color, "ese_color_remove_watcher called with NULL color");
    log_assert("COLOR", callback, "ese_color_remove_watcher called with NULL callback");

    for (size_t i = 0; i < color->watcher_count; i++) {
        if (color->watchers[i] == callback && color->watcher_userdata[i] == userdata) {
            // Remove this watcher by shifting remaining ones
            for (size_t j = i; j < color->watcher_count - 1; j++) {
                color->watchers[j] = color->watchers[j + 1];
                color->watcher_userdata[j] = color->watcher_userdata[j + 1];
            }
            color->watcher_count--;
            return true;
        }
    }

    return false;
}

// Lua integration
void ese_color_lua_init(EseLuaEngine *engine) {
    log_assert("COLOR", engine, "ese_color_lua_init called with NULL engine");

    _ese_color_lua_init(engine);
}

void ese_color_lua_push(EseColor *color) {
    log_assert("COLOR", color, "ese_color_lua_push called with NULL color");

    if (ese_color_get_lua_ref(color) == LUA_NOREF) {
        // Lua-owned: create a new userdata
        EseColor **ud =
            (EseColor **)lua_newuserdata(ese_color_get_state(color), sizeof(EseColor *));
        *ud = color;

        // Attach metatable
        luaL_getmetatable(ese_color_get_state(color), COLOR_META);
        lua_setmetatable(ese_color_get_state(color), -2);
    } else {
        // C-owned: get from registry
        lua_rawgeti(ese_color_get_state(color), LUA_REGISTRYINDEX, ese_color_get_lua_ref(color));
    }
}

EseColor *ese_color_lua_get(lua_State *L, int idx) {
    log_assert("COLOR", L, "ese_color_lua_get called with NULL Lua state");

    // Check if the value at idx is userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }

    // Get the userdata and check metatable
    EseColor **ud = (EseColor **)luaL_testudata(L, idx, COLOR_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void ese_color_ref(EseColor *color) {
    log_assert("COLOR", color, "ese_color_ref called with NULL color");

    if (ese_color_get_lua_ref(color) == LUA_NOREF) {
        // First time referencing - create userdata and store reference
        EseColor **ud =
            (EseColor **)lua_newuserdata(ese_color_get_state(color), sizeof(EseColor *));
        *ud = color;

        // Attach metatable
        luaL_getmetatable(ese_color_get_state(color), COLOR_META);
        lua_setmetatable(ese_color_get_state(color), -2);

        // Store hard reference to prevent garbage collection
        int ref = luaL_ref(ese_color_get_state(color), LUA_REGISTRYINDEX);
        _ese_color_set_lua_ref(color, ref);
        _ese_color_set_lua_ref_count(color, 1);
    } else {
        // Already referenced - just increment count
        _ese_color_set_lua_ref_count(color, ese_color_get_lua_ref_count(color) + 1);
    }

    profile_count_add("ese_color_ref_count");
}

void ese_color_unref(EseColor *color) {
    if (!color)
        return;

    if (ese_color_get_lua_ref(color) != LUA_NOREF && ese_color_get_lua_ref_count(color) > 0) {
        _ese_color_set_lua_ref_count(color, ese_color_get_lua_ref_count(color) - 1);

        if (ese_color_get_lua_ref_count(color) == 0) {
            // No more references - remove from registry
            luaL_unref(ese_color_get_state(color), LUA_REGISTRYINDEX, ese_color_get_lua_ref(color));
            _ese_color_set_lua_ref(color, LUA_NOREF);
        }
    }

    profile_count_add("ese_color_unref_count");
}

// Utility functions
bool ese_color_set_hex(EseColor *color, const char *hex_string) {
    log_assert("COLOR", color, "ese_color_set_hex called with NULL color");
    log_assert("COLOR", hex_string, "ese_color_set_hex called with NULL hex_string");

    if (!hex_string || hex_string[0] != '#') {
        return false;
    }

    size_t len = strlen(hex_string);
    if (len < 4 || len > 9) { // #RGB to #RRGGBBAA
        return false;
    }

    unsigned int r, g, b, a = 255; // Default alpha to 255

    if (len == 4) { // #RGB
        if (sscanf(hex_string, "#%1x%1x%1x", &r, &g, &b) != 3) {
            return false;
        }
        r = (r << 4) | r; // Expand to 8-bit
        g = (g << 4) | g;
        b = (b << 4) | b;
    } else if (len == 5) { // #RGBA
        if (sscanf(hex_string, "#%1x%1x%1x%1x", &r, &g, &b, &a) != 4) {
            return false;
        }
        r = (r << 4) | r; // Expand to 8-bit
        g = (g << 4) | g;
        b = (b << 4) | b;
        a = (a << 4) | a;
    } else if (len == 7) { // #RRGGBB
        if (sscanf(hex_string, "#%2x%2x%2x", &r, &g, &b) != 3) {
            return false;
        }
    } else if (len == 9) { // #RRGGBBAA
        if (sscanf(hex_string, "#%2x%2x%2x%2x", &r, &g, &b, &a) != 4) {
            return false;
        }
    } else {
        return false;
    }

    // Convert to normalized floats
    color->r = (float)r / 255.0f;
    color->g = (float)g / 255.0f;
    color->b = (float)b / 255.0f;
    color->a = (float)a / 255.0f;

    _ese_color_notify_watchers(color);
    return true;
}

void ese_color_set_byte(EseColor *color, unsigned char r, unsigned char g, unsigned char b,
                        unsigned char a) {
    log_assert("COLOR", color, "ese_color_set_byte called with NULL color");

    color->r = (float)r / 255.0f;
    color->g = (float)g / 255.0f;
    color->b = (float)b / 255.0f;
    color->a = (float)a / 255.0f;

    _ese_color_notify_watchers(color);
}

void ese_color_get_byte(const EseColor *color, unsigned char *r, unsigned char *g, unsigned char *b,
                        unsigned char *a) {
    log_assert("COLOR", color, "ese_color_get_byte called with NULL color");
    log_assert("COLOR", r, "ese_color_get_byte called with NULL r pointer");
    log_assert("COLOR", g, "ese_color_get_byte called with NULL g pointer");
    log_assert("COLOR", b, "ese_color_get_byte called with NULL b pointer");
    log_assert("COLOR", a, "ese_color_get_byte called with NULL a pointer");

    *r = (unsigned char)(color->r * 255.0f + 0.5f);
    *g = (unsigned char)(color->g * 255.0f + 0.5f);
    *b = (unsigned char)(color->b * 255.0f + 0.5f);
    *a = (unsigned char)(color->a * 255.0f + 0.5f);
}

size_t ese_color_sizeof(void) { return sizeof(EseColor); }

/**
 * @brief Serializes an EseColor to a cJSON object.
 *
 * Creates a cJSON object representing the color with type "COLOR"
 * and r, g, b, a components. Only serializes the
 * color data, not Lua-related fields.
 *
 * @param color Pointer to the EseColor object to serialize
 * @return cJSON object representing the color, or NULL on failure
 */
cJSON *ese_color_serialize(const EseColor *color) {
    log_assert("COLOR", color, "ese_color_serialize called with NULL color");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("COLOR", "Failed to create cJSON object for color serialization");
        return NULL;
    }

    // Add type field
    cJSON *type = cJSON_CreateString("COLOR");
    if (!type || !cJSON_AddItemToObject(json, "type", type)) {
        log_error("COLOR", "Failed to add type field to color serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add red component
    cJSON *r = cJSON_CreateNumber((double)color->r);
    if (!r || !cJSON_AddItemToObject(json, "r", r)) {
        log_error("COLOR", "Failed to add r field to color serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add green component
    cJSON *g = cJSON_CreateNumber((double)color->g);
    if (!g || !cJSON_AddItemToObject(json, "g", g)) {
        log_error("COLOR", "Failed to add g field to color serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add blue component
    cJSON *b = cJSON_CreateNumber((double)color->b);
    if (!b || !cJSON_AddItemToObject(json, "b", b)) {
        log_error("COLOR", "Failed to add b field to color serialization");
        cJSON_Delete(json);
        return NULL;
    }

    // Add alpha component
    cJSON *a = cJSON_CreateNumber((double)color->a);
    if (!a || !cJSON_AddItemToObject(json, "a", a)) {
        log_error("COLOR", "Failed to add a field to color serialization");
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Deserializes an EseColor from a cJSON object.
 *
 * Creates a new EseColor from a cJSON object with type "COLOR"
 * and r, g, b, a components. The color is created
 * with the specified engine and must be explicitly referenced with
 * ese_color_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for color creation
 * @param data cJSON object containing color data
 * @return Pointer to newly created EseColor object, or NULL on failure
 */
EseColor *ese_color_deserialize(EseLuaEngine *engine, const cJSON *data) {
    log_assert("COLOR", data, "ese_color_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("COLOR", "Color deserialization failed: data is not a JSON object");
        return NULL;
    }

    // Check type field
    cJSON *type_item = cJSON_GetObjectItem(data, "type");
    if (!type_item || !cJSON_IsString(type_item) || strcmp(type_item->valuestring, "COLOR") != 0) {
        log_error("COLOR", "Color deserialization failed: invalid or missing type field");
        return NULL;
    }

    // Get red component
    cJSON *r_item = cJSON_GetObjectItem(data, "r");
    if (!r_item || !cJSON_IsNumber(r_item)) {
        log_error("COLOR", "Color deserialization failed: invalid or missing r field");
        return NULL;
    }

    // Get green component
    cJSON *g_item = cJSON_GetObjectItem(data, "g");
    if (!g_item || !cJSON_IsNumber(g_item)) {
        log_error("COLOR", "Color deserialization failed: invalid or missing g field");
        return NULL;
    }

    // Get blue component
    cJSON *b_item = cJSON_GetObjectItem(data, "b");
    if (!b_item || !cJSON_IsNumber(b_item)) {
        log_error("COLOR", "Color deserialization failed: invalid or missing b field");
        return NULL;
    }

    // Get alpha component
    cJSON *a_item = cJSON_GetObjectItem(data, "a");
    if (!a_item || !cJSON_IsNumber(a_item)) {
        log_error("COLOR", "Color deserialization failed: invalid or missing a field");
        return NULL;
    }

    // Create new color
    EseColor *color = ese_color_create(engine);
    ese_color_set_r(color, (float)r_item->valuedouble);
    ese_color_set_g(color, (float)g_item->valuedouble);
    ese_color_set_b(color, (float)b_item->valuedouble);
    ese_color_set_a(color, (float)a_item->valuedouble);

    return color;
}
