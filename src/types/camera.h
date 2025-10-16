#ifndef ESE_CAMERA_H
#define ESE_CAMERA_H

// Constants
#define CAMERA_META "CameraMeta"

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
    EsePoint *position; /** The position of the camera as a EsePoint object */
    float rotation; /** The rotation of the camera in radians */
    float scale; /** The scale/zoom level of the camera */

    lua_State *state;  /** Lua State this EseCamera belongs to */
    int lua_ref;       /** Lua registry reference to its own proxy table */
    int lua_ref_count; /** Number of times this camera has been referenced in C */
} EseCamera;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Property accessors
/**
 * @brief Gets the position of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The position EsePoint of the camera
 */
EsePoint *ese_camera_get_position(const EseCamera *camera_state);

/**
 * @brief Sets the position of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @param position The new position EsePoint
 */
void ese_camera_set_position(EseCamera *camera_state, EsePoint *position);

/**
 * @brief Gets the rotation of the camera in radians
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The rotation of the camera in radians
 */
float ese_camera_get_rotation(const EseCamera *camera_state);

/**
 * @brief Sets the rotation of the camera in radians
 *
 * @param camera_state Pointer to the EseCamera object
 * @param rotation The new rotation value in radians
 */
void ese_camera_set_rotation(EseCamera *camera_state, float rotation);

/**
 * @brief Gets the scale of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The scale of the camera
 */
float ese_camera_get_scale(const EseCamera *camera_state);

/**
 * @brief Sets the scale of the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @param scale The new scale value
 */
void ese_camera_set_scale(EseCamera *camera_state, float scale);

/**
 * @brief Gets the Lua state associated with the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua state associated with the camera
 */
lua_State *ese_camera_get_state(const EseCamera *camera_state);

/**
 * @brief Gets the Lua registry reference for the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua registry reference for the camera
 */
int ese_camera_get_lua_ref(const EseCamera *camera_state);

/**
 * @brief Gets the Lua reference count for the camera
 *
 * @param camera_state Pointer to the EseCamera object
 * @return The Lua reference count for the camera
 */
int ese_camera_get_lua_ref_count(const EseCamera *camera_state);

// Core lifecycle
/**
 * @brief Creates a new EseCamera object.
 * 
 * @details Allocates memory for a new EseCamera and initializes to default values
 *          (position at (0, 0), rotation 0, scale 1.0). The position EsePoint is C-owned.
 *          The camera is created without Lua references and must be explicitly
 *          referenced with ese_camera_ref() if Lua access is desired.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseCamera object
 * 
 * @warning The returned EseCamera must be freed with ese_camera_destroy() to prevent memory leaks
 */
EseCamera *ese_camera_create(EseLuaEngine *engine);

/**
 * @brief Copies a source EseCamera into a new EseCamera object.
 * 
 * @details This function creates a deep copy of an EseCamera object. It allocates a new EseCamera
 *          struct and copies all members, including creating a new EsePoint for the position.
 *          The copy is created without Lua references and must be explicitly referenced
 *          with ese_camera_ref() if Lua access is desired.
 * 
 * @param source Pointer to the source EseCamera to copy.
 * @return A new, distinct EseCamera object that is a copy of the source.
 * 
 * @warning The returned EseCamera must be freed with ese_camera_destroy() to prevent memory leaks.
 */
EseCamera *ese_camera_copy(const EseCamera *source);

/**
 * @brief Destroys a EseCamera object, managing memory based on Lua references.
 * 
 * @details If the camera has no Lua references (lua_ref == LUA_NOREF), frees memory immediately.
 *          If the camera has Lua references, decrements the reference counter.
 *          When the counter reaches 0, removes the Lua reference and lets
 *          Lua's garbage collector handle final cleanup.
 * 
 * @note If the camera is Lua-owned, memory may not be freed immediately.
 *       Lua's garbage collector will finalize it once no references remain.
 * 
 * @param camera_state Pointer to the EseCamera object to destroy
 */
void ese_camera_destroy(EseCamera *camera_state);

// Lua integration
/**
 * @brief Initializes the EseCamera userdata type in the Lua state.
 * 
 * @details Creates and registers the "CameraStateProxyMeta" metatable with __index, __newindex,
 *          __gc, __tostring metamethods for property access and garbage collection.
 *          This allows EseCamera objects to be used naturally from Lua with dot notation.
 *          Also creates the global "Camera" table with "new" constructor.
 * 
 * @param engine EseLuaEngine pointer where the EseCamera type will be registered
 */
void ese_camera_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes a EseCamera object to the Lua stack.
 * 
 * @details If the camera has no Lua references (lua_ref == LUA_NOREF), creates a new
 *          proxy table. If the camera has Lua references, retrieves the existing
 *          proxy table from the registry.
 * 
 * @param camera_state Pointer to the EseCamera object to push to Lua
 */
void ese_camera_lua_push(EseCamera *camera_state);

/**
 * @brief Extracts a EseCamera pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseCamera pointer from the "__ptr" field of a Lua
 *          table that was created by ese_camera_lua_push(). Performs
 *          type checking to ensure the object is a valid EseCamera proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param L Lua state pointer
 * @param idx Stack index of the Lua EseCamera object
 * @return Pointer to the EseCamera object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseCamera *ese_camera_lua_get(lua_State *L, int idx);

/**
 * @brief References a EseCamera object for Lua access with reference counting.
 * 
 * @details If camera_state->lua_ref is LUA_NOREF, pushes the camera to Lua and references it,
 *          setting lua_ref_count to 1. If camera_state->lua_ref is already set, increments
 *          the reference count by 1. This prevents the camera from being garbage
 *          collected while C code holds references to it.
 * 
 * @param camera_state Pointer to the EseCamera object to reference
 */
void ese_camera_ref(EseCamera *camera_state);

/**
 * @brief Unreferences a EseCamera object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param camera_state Pointer to the EseCamera object to unreference
 */
void ese_camera_unref(EseCamera *camera_state);

#endif // ESE_CAMERA_H
