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
 */
typedef struct EseRect {
    float x;      /**< The x-coordinate of the rectangle's top-left corner */
    float y;      /**< The y-coordinate of the rectangle's top-left corner */
    float width;  /**< The width of the rectangle */
    float height; /**< The height of the rectangle */
    lua_State *state; /**< Lua State this EseRect belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseRect;

/**
 * @brief Initializes the EseRect userdata type in the Lua state.
 * 
 * @details Creates and registers the "RectProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseRect objects
 *          to be used naturally from Lua with dot notation.
 * 
 * @param engine EseLuaEngine pointer where the EseRect type will be registered
 */
void rect_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseRect object.
 * 
 * @details Allocates memory for a new EseRect and initializes to (0, 0, 0, 0).
 * 
 * @param engine Pointer to a EseLuaEngine
 * @param c_only True if this object wont be accessable in LUA.
 * @return Pointer to newly created EseRect object
 * 
 * @warning The returned EseRect must be freed with rect_destroy() to prevent memory leaks
 */
EseRect *rect_create(EseLuaEngine *engine, bool c_only);

/**
 * @brief Copies a source EseRect into a new EseRect object.
 * 
 * @details This function creates a deep copy of an EseRect object. It allocates a new EseRect
 * struct and copies the numeric members.
 * 
 * @param source Pointer to the source EseRect to copy.
 * @param c_only True if the copied object wont be accessable in LUA.
 * @return A new, distinct EseRect object that is a copy of the source.
 * 
 * @warning The returned EseRect must be freed with rect_destroy() to prevent memory leaks.
 */
EseRect *rect_copy(const EseRect *source, bool c_only);

/**
 * @brief Destroys a EseRect object and frees its memory.
 * 
 * @details Frees the memory allocated by rect_create().
 * 
 * @param rect Pointer to the EseRect object to destroy
 */
void rect_destroy(EseRect *rect);

/**
 * @brief Extracts a EseRect pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseRect pointer from the "__ptr" field of a Lua
 *          table that was created by _rect_lua_push(). Performs
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
 * @brief Checks if a point is inside the rectangle.
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
 * @param rect1 Pointer to the first EseRect object
 * @param rect2 Pointer to the second EseRect object
 * @return true if rectangles intersect, false otherwise
 */
bool rect_intersects(const EseRect *rect1, const EseRect *rect2);

/**
 * @brief Gets the area of the rectangle.
 * 
 * @param rect Pointer to the EseRect object
 * @return The area of the rectangle
 */
float rect_area(const EseRect *rect);

#endif // ESE_RECT_H
