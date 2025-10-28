#ifndef ESE_COLOR_H
#define ESE_COLOR_H

#include "vendor/json/cJSON.h"
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

// Constants
#define COLOR_META "ColorMeta"

/**
 * @brief Represents a color with RGBA components as normalized floats (0-1).
 *
 * @details This structure stores red, green, blue, and alpha components as
 *          floating-point values in the range [0.0, 1.0].
 */
typedef struct EseColor EseColor;

/**
 * @brief Callback function type for color property change notifications.
 *
 * @param color Pointer to the EseColor that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EseColorWatcherCallback)(EseColor *color, void *userdata);

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseColor object.
 *
 * @details Allocates memory for a new EseColor and initializes to (0, 0, 0, 1).
 *          The color is created without Lua references and must be explicitly
 *          referenced with ese_color_ref() if Lua access is desired.
 *
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseColor object
 *
 * @warning The returned EseColor must be freed with ese_color_destroy() to
 * prevent memory leaks
 */
EseColor *ese_color_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseColor into a new EseColor object.
 *
 * @details This function creates a deep copy of an EseColor object. It
 * allocates a new EseColor struct and copies all numeric members. The copy is
 * created without Lua references and must be explicitly referenced with
 * ese_color_ref() if Lua access is desired.
 *
 * @param source Pointer to the source EseColor to copy.
 * @return A new, distinct EseColor object that is a copy of the source.
 *
 * @warning The returned EseColor must be freed with ese_color_destroy() to
 * prevent memory leaks.
 */
EseColor *ese_color_copy(const EseColor *source);

/**
 * @brief Destroys a EseColor object, managing memory based on Lua references.
 *
 * @details If the color has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the color has Lua references, decrements the reference
 * counter. When the counter reaches 0, removes the Lua reference and lets Lua's
 * garbage collector handle final cleanup.
 *
 * @note If the color is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param color Pointer to the EseColor object to destroy
 */
void ese_color_destroy(EseColor *color);

/**
 * @brief Gets the size of the EseColor structure in bytes.
 *
 * @return The size of the EseColor structure in bytes
 */
size_t ese_color_sizeof(void);

// Property access
/**
 * @brief Sets the red component of the color.
 *
 * @param color Pointer to the EseColor object
 * @param r The red component value (0.0-1.0)
 */
void ese_color_set_r(EseColor *color, float r);

/**
 * @brief Gets the red component of the color.
 *
 * @param color Pointer to the EseColor object
 * @return The red component value (0.0-1.0)
 */
float ese_color_get_r(const EseColor *color);

/**
 * @brief Sets the green component of the color.
 *
 * @param color Pointer to the EseColor object
 * @param g The green component value (0.0-1.0)
 */
void ese_color_set_g(EseColor *color, float g);

/**
 * @brief Gets the green component of the color.
 *
 * @param color Pointer to the EseColor object
 * @return The green component value (0.0-1.0)
 */
float ese_color_get_g(const EseColor *color);

/**
 * @brief Sets the blue component of the color.
 *
 * @param color Pointer to the EseColor object
 * @param b The blue component value (0.0-1.0)
 */
void ese_color_set_b(EseColor *color, float b);

/**
 * @brief Gets the blue component of the color.
 *
 * @param color Pointer to the EseColor object
 * @return The blue component value (0.0-1.0)
 */
float ese_color_get_b(const EseColor *color);

/**
 * @brief Sets the alpha component of the color.
 *
 * @param color Pointer to the EseColor object
 * @param a The alpha component value (0.0-1.0)
 */
void ese_color_set_a(EseColor *color, float a);

/**
 * @brief Gets the alpha component of the color.
 *
 * @param color Pointer to the EseColor object
 * @return The alpha component value (0.0-1.0)
 */
float ese_color_get_a(const EseColor *color);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this color.
 *
 * @param color Pointer to the EseColor object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_color_get_state(const EseColor *color);

/**
 * @brief Gets the Lua registry reference for this color.
 *
 * @param color Pointer to the EseColor object
 * @return The Lua registry reference value
 */
int ese_color_get_lua_ref(const EseColor *color);

/**
 * @brief Gets the Lua reference count for this color.
 *
 * @param color Pointer to the EseColor object
 * @return The current reference count
 */
int ese_color_get_lua_ref_count(const EseColor *color);

/**
 * @brief Adds a watcher callback to be notified when any color property
 * changes.
 *
 * @details The callback will be called whenever any property (r, g, b, a) of
 * the color is modified. Multiple watchers can be registered on the same color.
 *
 * @param color Pointer to the EseColor object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_color_add_watcher(EseColor *color, EseColorWatcherCallback callback,
                           void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 *
 * @details Removes the first occurrence of the callback with matching userdata.
 *          If the same callback is registered multiple times with different
 * userdata, only the first match will be removed.
 *
 * @param color Pointer to the EseColor object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_color_remove_watcher(EseColor *color, EseColorWatcherCallback callback,
                              void *userdata);

// Lua integration
/**
 * @brief Initializes the EseColor userdata type in the Lua state.
 *
 * @details Creates and registers the "ColorProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseColor objects to be used naturally from Lua with
 * dot notation. Also creates the global "Color" table with "new", "white",
 * "black", "red", "green", "blue" constructors.
 *
 * @param engine EseLuaEngine pointer where the EseColor type will be registered
 */
void ese_color_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseColor object to the Lua stack.
 *
 * @details If the color has no Lua references (lua_ref == LUA_NOREF), creates a
 * new proxy table. If the color has Lua references, retrieves the existing
 *          proxy table from the registry.
 *
 * @param color Pointer to the EseColor object to push to Lua
 */
void ese_color_lua_push(EseColor *color);

/**
 * @brief Extracts a EseColor pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseColor pointer from the "__ptr" field of a Lua
 *          table that was created by ese_color_lua_push(). Performs
 *          type checking to ensure the object is a valid EseColor proxy table
 *          with the correct metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseColor object
 * @return Pointer to the EseColor object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseColor *ese_color_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseColor object for Lua access with reference counting.
 *
 * @details If color->lua_ref is LUA_NOREF, pushes the color to Lua and
 * references it, setting lua_ref_count to 1. If color->lua_ref is already set,
 * increments the reference count by 1. This prevents the color from being
 * garbage collected while C code holds references to it.
 *
 * @param color Pointer to the EseColor object to reference
 */
void ese_color_ref(EseColor *color);

/**
 * @brief Unreferences a EseColor object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param color Pointer to the EseColor object to unreference
 */
void ese_color_unref(EseColor *color);

// Utility functions
/**
 * @brief Sets the color from a hex string.
 *
 * @details Parses hex color strings in formats: #RGB, #RRGGBB, #RGBA, #RRGGBBAA
 *          and sets the corresponding RGBA values. Values are automatically
 *          normalized to 0.0-1.0 range.
 *
 * @param color Pointer to the EseColor object
 * @param hex_string Hex color string (e.g., "#FF0000", "#FF0000FF")
 * @return true if parsing was successful, false otherwise
 */
bool ese_color_set_hex(EseColor *color, const char *hex_string);

/**
 * @brief Sets the color from byte values (0-255).
 *
 * @details Takes byte values for RGBA components and converts them to
 *          normalized float values (0.0-1.0).
 *
 * @param color Pointer to the EseColor object
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void ese_color_set_byte(EseColor *color, unsigned char r, unsigned char g,
                        unsigned char b, unsigned char a);

/**
 * @brief Gets the color as byte values (0-255).
 *
 * @details Converts normalized float values to byte values for each component.
 *
 * @param color Pointer to the EseColor object
 * @param r Pointer to store red component (0-255)
 * @param g Pointer to store green component (0-255)
 * @param b Pointer to store blue component (0-255)
 * @param a Pointer to store alpha component (0-255)
 */
void ese_color_get_byte(const EseColor *color, unsigned char *r,
                        unsigned char *g, unsigned char *b, unsigned char *a);

/**
 * @brief Serializes an EseColor to a cJSON object.
 *
 * @details Creates a cJSON object representing the color with type "COLOR"
 *          and r, g, b, a components. Only serializes the
 *          color data, not Lua-related fields.
 *
 * @param color Pointer to the EseColor object to serialize
 * @return cJSON object representing the color, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned
 * object
 */
cJSON *ese_color_serialize(const EseColor *color);

/**
 * @brief Deserializes an EseColor from a cJSON object.
 *
 * @details Creates a new EseColor from a cJSON object with type "COLOR"
 *          and r, g, b, a components. The color is created
 *          with the specified engine and must be explicitly referenced with
 *          ese_color_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for color creation
 * @param data cJSON object containing color data
 * @return Pointer to newly created EseColor object, or NULL on failure
 *
 * @warning The returned EseColor must be freed with ese_color_destroy() to
 * prevent memory leaks
 */
EseColor *ese_color_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_COLOR_H
