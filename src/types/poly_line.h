#ifndef ESE_POLY_LINE_H
#define ESE_POLY_LINE_H

#include <stdbool.h>
#include <stddef.h>
#include "vendor/json/cJSON.h"

#define POLY_LINE_PROXY_META "PolyLineProxyMeta"
#define POLY_LINE_META "PolyLineMeta"

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePoint EsePoint;
typedef struct EseColor EseColor;

/**
 * @brief Enumeration for poly line types
 */
typedef enum {
    POLY_LINE_OPEN,     /** Open polyline (not closed) */
    POLY_LINE_CLOSED,   /** Closed polyline (connects last point to first) */
    POLY_LINE_FILLED    /** Filled polygon */
} EsePolyLineType;

/**
 * @brief Represents a polyline with points, stroke properties, and fill properties.
 * 
 * @details This structure stores a collection of points that form a polyline,
 *          along with stroke width, stroke color, fill color, and line type.
 */
typedef struct EsePolyLine EsePolyLine;

/**
 * @brief Callback function type for polyline property change notifications.
 * 
 * @param poly_line Pointer to the EsePolyLine that changed
 * @param userdata User-provided data passed when registering the watcher
 */
typedef void (*EsePolyLineWatcherCallback)(EsePolyLine *poly_line, void *userdata);

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
/**
 * @brief Creates a new EsePolyLine object.
 * 
 * @details Allocates memory for a new EsePolyLine and initializes to empty state.
 *          The polyline is created without Lua references and must be explicitly
 *          referenced with ese_poly_line_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EsePolyLine object
 * 
 * @warning The returned EsePolyLine must be freed with ese_poly_line_destroy() to prevent memory leaks
 */
EsePolyLine *ese_poly_line_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EsePolyLine into a new EsePolyLine object.
 * 
 * @details This function creates a deep copy of an EsePolyLine object. It allocates a new EsePolyLine
 *          struct and copies all members including the points collection. The copy is created without 
 *          Lua references and must be explicitly referenced with ese_poly_line_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EsePolyLine to copy.
 * @return A new, distinct EsePolyLine object that is a copy of the source.
 * 
 * @warning The returned EsePolyLine must be freed with ese_poly_line_destroy() to prevent memory leaks.
 */
EsePolyLine *ese_poly_line_copy(const EsePolyLine *source);

/**
 * @brief Destroys a EsePolyLine object, managing memory based on Lua references.
 * 
 * @details If the polyline has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the polyline has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the polyline is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param poly_line Pointer to the EsePolyLine object to destroy
 */
void ese_poly_line_destroy(EsePolyLine *poly_line);

/**
 * @brief Gets the size of the EsePolyLine structure in bytes.
 * 
 * @return The size of the EsePolyLine structure in bytes
 */
size_t ese_poly_line_sizeof(void);

// Property access
/**
 * @brief Sets the line type of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param type The line type (OPEN, CLOSED, or FILLED)
 */
void ese_poly_line_set_type(EsePolyLine *poly_line, EsePolyLineType type);

/**
 * @brief Gets the line type of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return The line type
 */
EsePolyLineType ese_poly_line_get_type(const EsePolyLine *poly_line);

/**
 * @brief Sets the stroke width of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param width The stroke width value
 */
void ese_poly_line_set_stroke_width(EsePolyLine *poly_line, float width);

/**
 * @brief Gets the stroke width of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return The stroke width value
 */
float ese_poly_line_get_stroke_width(const EsePolyLine *poly_line);

/**
 * @brief Sets the stroke color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param color Pointer to the EseColor object for stroke color
 */
void ese_poly_line_set_stroke_color(EsePolyLine *poly_line, EseColor *color);

/**
 * @brief Gets the stroke color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return Pointer to the EseColor object for stroke color
 */
EseColor *ese_poly_line_get_stroke_color(const EsePolyLine *poly_line);

/**
 * @brief Sets the fill color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param color Pointer to the EseColor object for fill color
 */
void ese_poly_line_set_fill_color(EsePolyLine *poly_line, EseColor *color);

/**
 * @brief Gets the fill color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return Pointer to the EseColor object for fill color
 */
EseColor *ese_poly_line_get_fill_color(const EsePolyLine *poly_line);

// Points collection management
/**
 * @brief Adds a point to the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param point Pointer to the EsePoint object to add
 * @return true if point was added successfully, false otherwise
 */
bool ese_poly_line_add_point(EsePolyLine *poly_line, EsePoint *point);

/**
 * @brief Removes a point from the polyline at the specified index.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param index Index of the point to remove
 * @return true if point was removed successfully, false if index is invalid
 */
bool ese_poly_line_remove_point(EsePolyLine *poly_line, size_t index);

/**
 * @brief Gets a point from the polyline at the specified index.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param index Index of the point to get
 * @return Pointer to the EsePoint object, or NULL if index is invalid
 */
EsePoint *ese_poly_line_get_point(const EsePolyLine *poly_line, size_t index);

/**
 * @brief Gets the number of points in the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return Number of points in the polyline
 */
size_t ese_poly_line_get_point_count(const EsePolyLine *poly_line);

/**
 * @brief Clears all points from the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 */
void ese_poly_line_clear_points(EsePolyLine *poly_line);

/**
 * @brief Gets the raw float array of point coordinates.
 * 
 * @details Returns a pointer to the internal float array containing point coordinates.
 *          The array contains x,y pairs for each point (x1, y1, x2, y2, ...).
 *          The number of floats in the array is point_count * 2.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return Pointer to the float array, or NULL if no points
 */
const float *ese_poly_line_get_points(const EsePolyLine *poly_line);

// Lua-related access
/**
 * @brief Gets the Lua state associated with this polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return Pointer to the Lua state, or NULL if none
 */
lua_State *ese_poly_line_get_state(const EsePolyLine *poly_line);

/**
 * @brief Gets the Lua registry reference for this polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return The Lua registry reference value
 */
int ese_poly_line_get_lua_ref(const EsePolyLine *poly_line);

/**
 * @brief Gets the Lua reference count for this polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @return The current reference count
 */
int ese_poly_line_get_lua_ref_count(const EsePolyLine *poly_line);

/**
 * @brief Gets the x-coordinate of a point at the specified index.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param index Index of the point (0-based)
 * @return The x-coordinate of the point
 */
float ese_poly_line_get_point_x(const EsePolyLine *poly_line, size_t index);

/**
 * @brief Gets the y-coordinate of a point at the specified index.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param index Index of the point (0-based)
 * @return The y-coordinate of the point
 */
float ese_poly_line_get_point_y(const EsePolyLine *poly_line, size_t index);

/**
 * @brief Sets the type of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param type The new type (OPEN, CLOSED, or FILLED)
 */
void ese_poly_line_set_type(EsePolyLine *poly_line, EsePolyLineType type);

/**
 * @brief Sets the stroke width of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param stroke_width The new stroke width
 */
void ese_poly_line_set_stroke_width(EsePolyLine *poly_line, float stroke_width);

/**
 * @brief Sets the stroke color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param stroke_color The new stroke color (can be NULL)
 */
void ese_poly_line_set_stroke_color(EsePolyLine *poly_line, EseColor *stroke_color);

/**
 * @brief Sets the fill color of the polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param fill_color The new fill color (can be NULL)
 */
void ese_poly_line_set_fill_color(EsePolyLine *poly_line, EseColor *fill_color);

/**
 * @brief Sets the Lua state associated with this polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param state Pointer to the Lua state
 */
void ese_poly_line_set_state(EsePolyLine *poly_line, lua_State *state);

/**
 * @brief Internal function to create a new polyline (used by Lua constructor).
 * 
 * @return Pointer to the new EsePolyLine object
 */
EsePolyLine *_ese_poly_line_make(void);

/**
 * @brief Internal function to notify watchers (used by Lua functions).
 * 
 * @param poly_line Pointer to the EsePolyLine object
 */
void _ese_poly_line_notify_watchers(EsePolyLine *poly_line);

/**
 * @brief Adds a watcher callback to be notified when any polyline property changes.
 * 
 * @details The callback will be called whenever any property of the polyline is modified.
 *          Multiple watchers can be registered on the same polyline.
 * 
 * @param poly_line Pointer to the EsePolyLine object to watch
 * @param callback Function to call when properties change
 * @param userdata User-provided data to pass to the callback
 * @return true if watcher was added successfully, false otherwise
 */
bool ese_poly_line_add_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata);

/**
 * @brief Removes a previously registered watcher callback.
 * 
 * @details Removes the first occurrence of the callback with matching userdata.
 *          If the same callback is registered multiple times with different userdata,
 *          only the first match will be removed.
 * 
 * @param poly_line Pointer to the EsePolyLine object
 * @param callback Function to remove
 * @param userdata User data that was used when registering
 * @return true if watcher was removed, false if not found
 */
bool ese_poly_line_remove_watcher(EsePolyLine *poly_line, EsePolyLineWatcherCallback callback, void *userdata);

// Lua integration
/**
 * @brief Initializes the EsePolyLine userdata type in the Lua state.
 * 
 * @details Creates and registers the "PolyLineProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EsePolyLine objects to be used naturally from Lua with dot notation.
 *          Also creates the global "PolyLine" table with "new" constructor.
 * 
 * @param engine EseLuaEngine pointer where the EsePolyLine type will be registered
 */
void ese_poly_line_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EsePolyLine object to the Lua stack.
 * 
 * @details If the polyline has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the polyline has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param poly_line Pointer to the EsePolyLine object to push to Lua
 */
void ese_poly_line_lua_push(EsePolyLine *poly_line);

/**
 * @brief Extracts a EsePolyLine pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EsePolyLine pointer from the "__ptr" field of a Lua
 *          table that was created by ese_poly_line_lua_push(). Performs
 *          type checking to ensure the object is a valid EsePolyLine proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EsePolyLine object
 * @return Pointer to the EsePolyLine object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EsePolyLine *ese_poly_line_lua_get(lua_State *L, int idx);

/**
 * @brief References a EsePolyLine object for Lua access with reference counting.
 * 
 * @details If poly_line->lua_ref is LUA_NOREF, pushes the polyline to Lua and references it,
 *          setting lua_ref_count to 1. If poly_line->lua_ref is already set, increments
 *          the reference count by 1. This prevents the polyline from being garbage
 *          collected while C code holds references to it.
 * 
 * @param poly_line Pointer to the EsePolyLine object to reference
 */
void ese_poly_line_ref(EsePolyLine *poly_line);

/**
 * @brief Unreferences a EsePolyLine object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param poly_line Pointer to the EsePolyLine object to unreference
 */
void ese_poly_line_unref(EsePolyLine *poly_line);

/**
 * @brief Serializes an EsePolyLine to a cJSON object.
 *
 * @details Creates a cJSON object representing the polyline with type "POLY_LINE"
 *          and all properties including points, colors, and styling. Only serializes the
 *          geometric and styling data, not Lua-related fields.
 *
 * @param poly_line Pointer to the EsePolyLine object to serialize
 * @return cJSON object representing the polyline, or NULL on failure
 *
 * @warning The caller is responsible for calling cJSON_Delete() on the returned object
 */
cJSON *ese_poly_line_serialize(const EsePolyLine *poly_line);

/**
 * @brief Deserializes an EsePolyLine from a cJSON object.
 *
 * @details Creates a new EsePolyLine from a cJSON object with type "POLY_LINE"
 *          and all properties including points, colors, and styling. The polyline is created
 *          with the specified engine and must be explicitly referenced with
 *          ese_poly_line_ref() if Lua access is desired.
 *
 * @param engine EseLuaEngine pointer for polyline creation
 * @param data cJSON object containing polyline data
 * @return Pointer to newly created EsePolyLine object, or NULL on failure
 *
 * @warning The returned EsePolyLine must be freed with ese_poly_line_destroy() to prevent memory leaks
 */
EsePolyLine *ese_poly_line_deserialize(EseLuaEngine *engine, const cJSON *data);

#endif // ESE_POLY_LINE_H
