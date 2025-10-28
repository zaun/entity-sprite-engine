/**
 * @file rect.h
 * @brief Rectangle type with floating-point coordinates and dimensions
 * @details Provides rectangle operations, collision detection, and JSON
 * serialization
 *
 * @copyright Copyright (c) 2024 ESE Project
 * @license See LICENSE.md for license information
 */

#ifndef ESE_RECT_H
#define ESE_RECT_H

#include "vendor/json/cJSON.h"
#include <stdbool.h>
#include <stddef.h>

// ========================================
// DEFINES AND STRUCTS
// ========================================

#define RECT_PROXY_META "RectProxyMeta"
#define RECT_META "RectMeta"

/**
 * @brief Represents a rectangle with floating-point coordinates and dimensions.
 */
typedef struct EseRect EseRect;

/**
 * @brief Callback function type for rect property change notifications.
 *
 * @param rect Pointer to the EseRect that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EseRectWatcherCallback)(EseRect *rect, void *userdata);

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseRect object.
 *
 * @details Allocates memory for a new EseRect and initializes to (0, 0, 0, 0).
 *          The rect is created without Lua references and must be explicitly
 *          referenced with ese_rect_ref() if Lua access is desired.
 *
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseRect object
 *
 * @warning The returned EseRect must be freed with ese_rect_destroy() to
 * prevent memory leaks
 */
EseRect *ese_rect_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseRect into a new EseRect object.
 *
 * @details This function creates a deep copy of an EseRect object. It allocates
 * a new EseRect struct and copies all numeric members. The copy is created
 * without Lua references and must be explicitly referenced with ese_rect_ref()
 * if Lua access is desired.
 *
 * @param source Pointer to the source EseRect to copy.
 * @return A new, distinct EseRect object that is a copy of the source.
 *
 * @warning The returned EseRect must be freed with ese_rect_destroy() to
 * prevent memory leaks
 */
EseRect *ese_rect_copy(const EseRect *source);

/**
 * @brief Destroys a EseRect object, managing memory based on Lua references.
 *
 * @details If the rect has no Lua references (lua_ref == LUA_NOREF), frees
 * memory immediately. If the rect has Lua references, decrements the reference
 * counter. When the counter reaches 0, removes the Lua reference and lets Lua's
 * garbage collector handle final cleanup.
 *
 * @note If the rect is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 *
 * @param rect Pointer to the EseRect object to destroy
 */
void ese_rect_destroy(EseRect *rect);

/**
 * @brief Gets the size of the EseRect structure in bytes.
 *
 * @return The size of the EseRect structure in bytes
 */
size_t ese_rect_sizeof(void);

// Property access
/**
 * @brief Sets the x-coordinate of the rectangle's top-left corner.
 *
 * @param rect Pointer to the EseRect object
 * @param x The x-coordinate value
 */
void ese_rect_set_x(EseRect *rect, float x);

/**
 * @brief Gets the x-coordinate of the rectangle's top-left corner.
 *
 * @param rect Pointer to the EseRect object
 * @return The x-coordinate value
 */
float ese_rect_get_x(const EseRect *rect);

/**
 * @brief Sets the y-coordinate of the rectangle's top-left corner.
 *
 * @param rect Pointer to the EseRect object
 * @param y The y-coordinate value
 */
void ese_rect_set_y(EseRect *rect, float y);

/**
 * @brief Gets the y-coordinate of the rectangle's top-left corner.
 *
 * @param rect Pointer to the EseRect object
 * @return The y-coordinate value
 */
float ese_rect_get_y(const EseRect *rect);

/**
 * @brief Sets the width of the rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @param width The width value
 */
void ese_rect_set_width(EseRect *rect, float width);

/**
 * @brief Gets the width of the rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @return The width value
 */
float ese_rect_get_width(const EseRect *rect);

/**
 * @brief Sets the height of the rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @param height The height value
 */
void ese_rect_set_height(EseRect *rect, float height);

/**
 * @brief Gets the height of the rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @return The height value
 */
float ese_rect_get_height(const EseRect *rect);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_rect_get_state(const EseRect *rect);

/**
 * @brief Gets the Lua registry reference for this rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @return The Lua registry reference value
 */
int ese_rect_get_lua_ref(const EseRect *rect);

/**
 * @brief Gets the Lua reference count for this rectangle.
 *
 * @param rect Pointer to the EseRect object
 * @return The current reference count
 */
int ese_rect_get_lua_ref_count(const EseRect *rect);

/**
 * @brief Adds a watcher callback to be notified when any rect property changes.
 *
 * @details The callback will be called whenever any property (x, y, width,
 * height, rotation) of the rect is modified. Multiple watchers can be
 * registered on the same rect.
 *
 * @param rect Pointer to the EseRect object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_rect_add_watcher(EseRect *rect, EseRectWatcherCallback callback,
                          void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 *
 * @details Removes the first occurrence of the callback with matching userdata.
 *          If the same callback is registered multiple times with different
 * userdata, only the first match will be removed.
 *
 * @param rect Pointer to the EseRect object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_rect_remove_watcher(EseRect *rect, EseRectWatcherCallback callback,
                             void *userdata);

/**
 * @brief Notifies all registered watchers of a rect change
 *
 * @details Internal function used to notify all registered watcher callbacks
 *          when any rect property (x, y, width, height, rotation) is modified.
 *
 * @param rect Pointer to the EseRect that has changed
 */
void _ese_rect_notify_watchers(EseRect *rect);

/**
 * @brief Sets the rotation of the rectangle.
 *
 * @details Sets the rotation in radians. Positive values rotate
 * counterclockwise.
 *
 * @param rect Pointer to the EseRect object
 * @param radians Rotation angle in radians
 */
void ese_rect_set_rotation(EseRect *rect, float radians);

/**
 * @brief Gets the rotation of the rectangle.
 *
 * @details Returns the rotation in radians.
 *
 * @param rect Pointer to the EseRect object
 * @return Rotation angle in radians
 */
float ese_rect_get_rotation(const EseRect *rect);

// Lua integration
/**
 * @brief Initializes the EseRect userdata type in the Lua state.
 *
 * @details Creates and registers the "RectProxyMeta" metatable with __index,
 * __newindex,
 *          __gc, __tostring metamethods for property access and garbage
 * collection. This allows EseRect objects to be used naturally from Lua with
 * dot notation. Also creates the global "Rect" table with "new" and "zero"
 * constructors.
 *
 * @param engine EseLuaEngine pointer where the EseRect type will be registered
 */
void ese_rect_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseRect object to the Lua stack.
 *
 * @details If the rect has no Lua references (lua_ref == LUA_NOREF), creates a
 * new proxy table. If the rect has Lua references, retrieves the existing proxy
 * table from the registry.
 *
 * @param rect Pointer to the EseRect object to push to Lua
 */
void ese_rect_lua_push(EseRect *rect);

/**
 * @brief Extracts a EseRect pointer from a Lua userdata object with type
 * safety.
 *
 * @details Retrieves the C EseRect pointer from the "__ptr" field of a Lua
 *          table that was created by ese_rect_lua_push(). Performs
 *          type checking to ensure the object is a valid EseRect proxy table
 *          with the correct metatable and userdata pointer.
 *
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseRect object
 * @return Pointer to the EseRect object, or NULL if extraction fails or type
 * check fails
 *
 * @warning Returns NULL for invalid objects - always check return value before
 * use
 */
EseRect *ese_rect_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseRect object for Lua access with reference counting.
 *
 * @details If rect->lua_ref is LUA_NOREF, pushes the rect to Lua and references
 * it, setting lua_ref_count to 1. If rect->lua_ref is already set, increments
 *          the reference count by 1. This prevents the rect from being garbage
 *          collected while C code holds references to it.
 *
 * @param rect Pointer to the EseRect object to reference
 */
void ese_rect_ref(EseRect *rect);

/**
 * @brief Unreferences a EseRect object, decrementing the reference count.
 *
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua
 * reference is removed from the registry. This function does NOT free memory.
 *
 * @param rect Pointer to the EseRect object to unreference
 */
void ese_rect_unref(EseRect *rect);

// Mathematical operations
/**
 * @brief Checks if a point is inside the rectangle.
 *
 * @details Performs a point-in-rectangle test, handling both axis-aligned and
 *          rotated rectangles. For rotated rectangles, transforms the point to
 *          local coordinates before testing.
 *
 * @param rect Pointer to the EseRect object
 * @param x X coordinate of the point
 * @param y Y coordinate of the point
 * @return true if point is inside, false otherwise
 */
bool ese_rect_contains_point(const EseRect *rect, float x, float y);

/**
 * @brief Checks if two rectangles intersect.
 *
 * @details Performs collision detection between two rectangles using the
 *          Separating Axis Theorem (SAT) for rotated rectangles. Falls back
 *          to simple AABB intersection for axis-aligned rectangles.
 *
 * @param rect1 Pointer to the first EseRect object
 * @param rect2 Pointer to the second EseRect object
 * @return true if rectangles intersect, false otherwise
 */
bool ese_rect_intersects(const EseRect *rect1, const EseRect *rect2);

/**
 * @brief Gets the area of the rectangle.
 *
 * @details Calculates the area as width × height. This is unaffected by
 * rotation.
 *
 * @param rect Pointer to the EseRect object
 * @return The area of the rectangle (width × height)
 */
float ese_rect_area(const EseRect *rect);

/**
 * @brief Serializes an EseRect to a cJSON object.
 *
 * @details Creates a cJSON object representing the rect with type "RECT"
 *          and x, y, width, height, rotation coordinates. Only serializes the
 *          coordinate and dimension data, not Lua-related fields.
 *
 * @param rect Pointer to the EseRect object to serialize
 * @return cJSON object representing the rect, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned
 * object
 */
cJSON *ese_rect_serialize(const EseRect *rect);

/**
 * @brief Deserializes an EseRect from a cJSON object.
 *
 * @details Creates a new EseRect from a cJSON object with type "RECT"
 *          and x, y, width, height, rotation coordinates. The rect is created
 *          with the specified engine and must be explicitly referenced with
 *          ese_rect_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for rect creation
 * @param data cJSON object containing rect data
 * @return Pointer to newly created EseRect object, or NULL on failure
 *
 * @warning The returned EseRect must be freed with ese_rect_destroy() to
 * prevent memory leaks
 */
EseRect *ese_rect_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_RECT_H
