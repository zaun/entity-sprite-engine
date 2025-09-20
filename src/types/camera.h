#ifndef ESE_CAMERA_STATE_H
#define ESE_CAMERA_STATE_H

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePoint EsePoint;
typedef struct EseCamera EseCamera;


// ========================================
// PUBLIC FUNCTIONS
// ========================================

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
void ese_camera_lua_push(EseLuaEngine *engine, EseCamera *camera_state);

/**
 * @brief Extracts a EseCamera pointer from a Lua userdata object with type safety.
 * 
 * @details Retrieves the C EseCamera pointer from the "__ptr" field of a Lua
 *          table that was created by ese_camera_lua_push(). Performs
 *          type checking to ensure the object is a valid EseCamera proxy table
 *          with the correct metatable and userdata pointer.
 * 
 * @param engine EseLuaEngine pointer
 * @param idx Stack index of the Lua EseCamera object
 * @return Pointer to the EseCamera object, or NULL if extraction fails or type check fails
 * 
 * @warning Returns NULL for invalid objects - always check return value before use
 */
EseCamera *ese_camera_lua_get(EseLuaEngine *engine, int idx);

/**
 * @brief References a EseCamera object for Lua access with reference counting.
 * 
 * @details If camera_state->lua_ref is ESE_LUA_NOREF, pushes the camera to Lua and references it,
 *          setting lua_ref_count to 1. If camera_state->lua_ref is already set, increments
 *          the reference count by 1. This prevents the camera from being garbage
 *          collected while C code holds references to it.
 * 
 * @param engine EseLuaEngine pointer
 * @param camera_state Pointer to the EseCamera object to reference
 */
void ese_camera_ref(EseLuaEngine *engine, EseCamera *camera_state);

/**
 * @brief Unreferences a EseCamera object, decrementing the reference count.
 * 
 * @details Decrements lua_ref_count by 1. If the count reaches 0, the Lua reference
 *          is removed from the registry. This function does NOT free memory.
 * 
 * @param engine EseLuaEngine pointer
 * @param camera_state Pointer to the EseCamera object to unreference
 */
void ese_camera_unref(EseLuaEngine *engine, EseCamera *camera_state);

// Property access
void ese_camera_set_position(EseCamera *camera_state, EsePoint *position);
EsePoint *ese_camera_get_position(const EseCamera *camera_state);
void ese_camera_set_rotation(EseCamera *camera_state, float rotation);
float ese_camera_get_rotation(const EseCamera *camera_state);
void ese_camera_set_scale(EseCamera *camera_state, float scale);
float ese_camera_get_scale(const EseCamera *camera_state);

// Lua-related access
int ese_camera_get_lua_ref(const EseCamera *camera_state);
int ese_camera_get_lua_ref_count(const EseCamera *camera_state);

// Utility
size_t ese_camera_sizeof(void);

#endif // ESE_CAMERA_STATE_H
