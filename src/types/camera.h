#ifndef ESE_CAMERA_STATE_H
#define ESE_CAMERA_STATE_H

// Forward declarations
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePoint EsePoint;

/**
 * @brief Represents the complete state of a 2D camera.
 * 
 * @details This structure stores the position, rotation, and scale of a camera in 2D space.
 *          The position is represented as a EsePoint object.
 */
typedef struct {
    EsePoint *position; /**< The position of the camera as a EsePoint object */
    float rotation; /**< The rotation of the camera in radians */
    float scale; /**< The scale/zoom level of the camera */
    lua_State *state; /**< Lua State this EseCamera belongs to */
    int lua_ref; /**< Lua registry reference to its own proxy table */
} EseCamera;

/**
 * @brief Initializes the EseCamera userdata type in the Lua state.
 * 
 * @details Creates and registers the "CameraStateProxyMeta" metatable with __index and 
 *          __newindex metamethods for property access. This allows EseCamera objects
 *          to be used naturally from Lua with dot notation (camera.position.x, camera.rotation, etc.).
 * 
 * @param engine EseLuaEngine pointer where the EseCamera type will be registered
 */
void camera_state_lua_init(EseLuaEngine *engine);

/**
 * @brief Creates a new EseCamera object.
 * 
 * @details Allocates memory for a new EseCamera and initializes to default values
 *          (position at (0, 0), rotation 0, scale 1.0). The position EsePoint is C-owned.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseCamera object
 * 
 * @warning The returned EseCamera must be freed with camera_state_destroy() to prevent memory leaks
 */
EseCamera *camera_state_create(EseLuaEngine *engine);

/**
 * @brief Destroys a EseCamera object and frees its memory.
 * 
 * @details Frees the memory allocated by camera_state_create(), including the EsePoint position.
 * 
 * @param camera_state Pointer to the EseCamera object to destroy
 */
void camera_state_destroy(EseCamera *camera_state);

/**
 * @brief Extracts a EseCamera pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseCamera pointer from the "__ptr" field of a Lua
 *          table that was created by _camera_state_lua_push(). Performs
 *          type checking to ensure the object is a valid EseCamera proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseCamera object
 * @return Pointer to the EseCamera object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseCamera *camera_state_lua_get(lua_State *L, int idx);

#endif // ESE_CAMERA_STATE_H
