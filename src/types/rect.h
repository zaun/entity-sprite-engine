#ifndef ESE_RECT_H
#define ESE_RECT_H

#include <stdbool.h>

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;

/**
 * @brief Represents a rectangle with floating-point coordinates and dimensions.
 * 
 * @details This structure stores the position and size of a rectangle in 2D space.
 *          The rotation is stored in radians around the center point.
 */
typedef struct EseRect {
    float x;            /**< The x-coordinate of the rectangle's top-left corner */
    float y;            /**< The y-coordinate of the rectangle's top-left corner */
    float width;        /**< The width of the rectangle */
    float height;       /**< The height of the rectangle */
    float rotation;     /**< The rotation of the rect around the center point in radians */

    lua_State *state;   /**< Lua State this EseRect belongs to */
    int lua_ref;        /**< Lua registry reference to its own proxy table */
    int lua_ref_count;  /**< Number of times this rect has been referenced in C */
} EseRect;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EseRect object.
 * 
 * @details Allocates memory for a new EseRect and initializes to (0, 0, 0, 0).
 *          The rect is created without Lua references and must be explicitly
 *          referenced with rect_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseRect object
 * 
 * @warning The returned EseRect must be freed with rect_destroy() to prevent memory leaks
 */
EseRect *rect_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseRect into a new EseRect object.
 * 
 * @details This function creates a deep copy of an EseRect object. It allocates a new EseRect
 *          struct and copies all numeric members. The copy is created without Lua references
 *          and must be explicitly referenced with rect_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseRect to copy.
 * @return A new, distinct EseRect object that is a copy of the source.
 * 
 * @warning The returned EseRect must be freed with rect_destroy() to prevent memory leaks.
 */
EseRect *rect_copy(const EseRect *source);

/**
 * @brief Destroys a EseRect object, managing memory based on Lua references.
 * 
 * @details If the rect has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the rect has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the rect is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param rect Pointer to the EseRect object to destroy
 */
void rect_destroy(EseRect *rect);

// Lua integration
/**
 * @brief Initializes the EseRect userdata type in the Lua state.
 * 
 * @details Creates and registers the "RectProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseRect objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Rect" table with "new" and "zero" constructors.
 * 
 * @param engine EseLuaEngine pointer where the EseRect type will be registered
 */
void rect_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseRect object to the Lua stack.
 * 
 * @details If the rect has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the rect has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param rect Pointer to the EseRect object to push to Lua
 */
void rect_lua_push(EseRect *rect);

/**
 * @brief Extracts a EseRect pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseRect pointer from the "__ptr" field of a Lua
 *          table that was created by rect_lua_push(). Performs
 *          type checking to ensure the object is a valid EseRect proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseRect object
 * @return Pointer to the EseRect object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseRect *rect_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseRect object for Lua access with reference counting.
 * 
 * @details If rect->lua_ref is LUA_NOREF, pushes the rect to Lua and references it,
 *          setting lua_ref_count to 1. If rect->lua_ref is already set, increments
 *          the reference count by 1. This prevents the rect from being garbage
 *          collected while C code holds references to it.
 * 
 * @param rect Pointer to the EseRect object to reference
 */
void rect_ref(EseRect *rect);

/**
 * @brief Unreferences a EseRect object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param rect Pointer to the EseRect object to unreference
 */
void rect_unref(EseRect *rect);

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
bool rect_contains_point(const EseRect *rect, float x, float y);

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
bool rect_intersects(const EseRect *rect1, const EseRect *rect2);

/**
 * @brief Gets the area of the rectangle.
 * 
 * @details Calculates the area as width × height. This is unaffected by rotation.
 * 
 * @param rect Pointer to the EseRect object
 * @return The area of the rectangle (width × height)
 */
float rect_area(const EseRect *rect);

// Property access
/**
 * @brief Sets the rotation of the rectangle.
 * 
 * @details Sets the rotation in radians. Positive values rotate counterclockwise.
 * 
 * @param rect Pointer to the EseRect object
 * @param radians Rotation angle in radians
 */
void rect_set_rotation(EseRect *rect, float radians);

/**
 * @brief Gets the rotation of the rectangle.
 * 
 * @details Returns the rotation in radians.
 * 
 * @param rect Pointer to the EseRect object
 * @return Rotation angle in radians
 */
float rect_get_rotation(const EseRect *rect);

#endif // ESE_RECT_H
