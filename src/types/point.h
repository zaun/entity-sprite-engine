#ifndef ESE_POINT_H
#define ESE_POINT_H

#include <stdint.h>
#include <stdbool.h>

#define POINT_PROXY_META "PointProxyMeta"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a 2D point with floating-point coordinates.
 * 
 * @details This structure stores the x and y coordinates of a point in 2D space.
 */
typedef struct EsePoint EsePoint;

/**
 * @brief Callback function type for point property change notifications.
 * 
 * @param point Pointer to the EsePoint that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EsePointWatcherCallback)(EsePoint *point, void *userdata);

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EsePoint object.
 * 
 * @details Allocates memory for a new EsePoint and initializes to (0, 0).
 *          The point is created without Lua references and must be explicitly
 *          referenced with ese_point_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EsePoint object
 * 
 * @warning The returned EsePoint must be freed with ese_point_destroy() to prevent memory leaks
 */
EsePoint *ese_point_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EsePoint into a new EsePoint object.
 * 
 * @details This function creates a deep copy of an EsePoint object. It allocates a new EsePoint
 *          struct and copies all numeric members. The copy is created without Lua references
 *          and must be explicitly referenced with ese_point_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EsePoint to copy.
 * @return A new, distinct EsePoint object that is a copy of the source.
 * 
 * @warning The returned EsePoint must be freed with ese_point_destroy() to prevent memory leaks.
 */
EsePoint *ese_point_copy(const EsePoint *source);

/**
 * @brief Destroys a EsePoint object, managing memory based on Lua references.
 * 
 * @details If the point has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the point has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the point is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param point Pointer to the EsePoint object to destroy
 */
void ese_point_destroy(EsePoint *point);

/**
 * @brief Gets the size of the EsePoint structure in bytes.
 * 
 * @return The size of the EsePoint structure in bytes
 */
size_t ese_point_sizeof(void);

// Property access
/**
 * @brief Sets the x-coordinate of the point.
 * 
 * @param point Pointer to the EsePoint object
 * @param x The x-coordinate value
 */
void ese_point_set_x(EsePoint *point, float x);

/**
 * @brief Gets the x-coordinate of the point.
 * 
 * @param point Pointer to the EsePoint object
 * @return The x-coordinate value
 */
float ese_point_get_x(const EsePoint *point);

/**
 * @brief Sets the y-coordinate of the point.
 * 
 * @param point Pointer to the EsePoint object
 * @param y The y-coordinate value
 */
void ese_point_set_y(EsePoint *point, float y);

/**
 * @brief Gets the y-coordinate of the point.
 * 
 * @param point Pointer to the EsePoint object
 * @return The y-coordinate value
 */
float ese_point_get_y(const EsePoint *point);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this point.
 * 
 * @param point Pointer to the EsePoint object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_point_get_state(const EsePoint *point);

/**
 * @brief Gets the Lua registry reference for this point.
 * 
 * @param point Pointer to the EsePoint object
 * @return The Lua registry reference value
 */
int ese_point_get_lua_ref(const EsePoint *point);

/**
 * @brief Gets the Lua reference count for this point.
 * 
 * @param point Pointer to the EsePoint object
 * @return The current reference count
 */
int ese_point_get_lua_ref_count(const EsePoint *point);

/**
 * @brief Adds a watcher callback to be notified when any point property changes.
 * 
 * @details The callback will be called whenever any property (x, y) of the point is modified.
 *          Multiple watchers can be registered on the same point.
 * 
 * @param point Pointer to the EsePoint object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_point_add_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 * 
 * @details Removes the first occurrence of the callback with matching userdata.
 *          If the same callback is registered multiple times with different userdata,
 *          only the first match will be removed.
 * 
 * @param point Pointer to the EsePoint object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_point_remove_watcher(EsePoint *point, EsePointWatcherCallback callback, void *userdata);

// Lua integration
/**
 * @brief Initializes the EsePoint userdata type in the Lua state.
 * 
 * @details Creates and registers the "PointProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EsePoint objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Point" table with "new" and "zero" constructors.
 * 
 * @param engine EseLuaEngine pointer where the EsePoint type will be registered
 */
void ese_point_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EsePoint object to the Lua stack.
 * 
 * @details If the point has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the point has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param point Pointer to the EsePoint object to push to Lua
 */
void ese_point_lua_push(EsePoint *point);

/**
 * @brief Extracts a EsePoint pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EsePoint pointer from the "__ptr" field of a Lua
 *          table that was created by ese_point_lua_push(). Performs
 *          type checking to ensure the object is a valid EsePoint proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EsePoint object
 * @return Pointer to the EsePoint object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EsePoint *ese_point_lua_get(lua_State *L, int idx);

/**
 * @brief References a EsePoint object for Lua access with reference counting.
 * 
 * @details If point->lua_ref is LUA_NOREF, pushes the point to Lua and references it,
 *          setting lua_ref_count to 1. If point->lua_ref is already set, increments
 *          the reference count by 1. This prevents the point from being garbage
 *          collected while C code holds references to it.
 * 
 * @param point Pointer to the EsePoint object to reference
 */
void ese_point_ref(EsePoint *point);

/**
 * @brief Unreferences a EsePoint object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param point Pointer to the EsePoint object to unreference
 */
void ese_point_unref(EsePoint *point);

// Mathematical operations
/**
 * @brief Calculates the distance between two points.
 * 
 * @param point1 Pointer to the first EsePoint object
 * @param point2 Pointer to the second EsePoint object
 * @return The Euclidean distance between the two points
 */
float ese_point_distance(const EsePoint *point1, const EsePoint *point2);

/**
 * @brief Calculates the squared distance between two points.
 * 
 * @param point1 Pointer to the first EsePoint object
 * @param point2 Pointer to the second EsePoint object
 * @return The squared Euclidean distance between the two points
 */
float ese_point_distance_squared(const EsePoint *point1, const EsePoint *point2);

#endif // ESE_POINT_H
