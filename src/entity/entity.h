#ifndef ESE_ENTITY_H
#define ESE_ENTITY_H

#include <stdbool.h>
#include "entity/entity_lua.h"

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;

/**
 * @brief Callback function type for entity drawing operations.
 * 
 * @param screen_x      Screen X coordinate to draw at
 * @param screen_y      Screen Y coordinate to draw at  
 * @param z_index       Draw order/depth
 * @param texture_id    ID of texture to draw
 * @param texture_x1    Source X1 coordinate in texture (normalized)
 * @param texture_y1    Source Y1 coordinate in texture (normalized)
 * @param texture_x2    Source X1 coordinate in texture (normalized)
 * @param texture_y2    Source Y1 coordinate in texture (normalized)
 * @param width         Source width in pixels
 * @param height        Source height in pixels
 * @param user_data     User-provided callback data
 */
typedef void (*EntityDrawTextureCallback)(
    float screen_x, float screen_y, float screen_w, float screen_h, int z_index,
    const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
    int width, int height,
    void *user_data
);

typedef void (*EntityDrawRectCallback)(
    float screen_x, float screen_y, int z_index,
    int width, int height, bool filled, unsigned char r, unsigned char g, unsigned char b, unsigned char a,
    void *user_data
);

/**
 * @brief Creates a new EseEntity object.
 * 
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseEntity object
 * 
 * @warning The returned EseEntity must be freed with entity_destroy() to prevent memory leaks
 */
EseEntity *entity_create(EseLuaEngine *engine);

/**
 * @brief Creates a copy of an existing EseEntity.
 * 
 * @param entity Pointer to the EseEntity to copy
 * @return Pointer to newly created EseEntity copy
 */
EseEntity *entity_copy(EseEntity *entity);

/**
 * @brief Destroys an EseEntity object and frees its memory.
 * 
 * @param entity Pointer to the EseEntity object to destroy
 */
void entity_destroy(EseEntity *entity);

/**
 * @brief Updates an entity and its components.
 * 
 * @param entity Pointer to the EseEntity to update
 * @param delta_time Time elapsed since last update
 */
void entity_update(EseEntity *entity, float delta_time);

void entity_run_function_with_args(
    EseEntity *entity,
    const char *func_name,
    int argc,
    EseLuaValue *argv
);

/**
 * @brief Check for a collision between entitiy and test.
 * 
 * @param entity Pointer to the primanry EseEntity to check
 * @param test Pointer to the secondary EseEntity to test
 */
void entity_process_collision(EseEntity *entity, EseEntity *test);

/**
 * @brief Draws an entity using the provided callback.
 * 
 * @param entity Pointer to the EseEntity to draw
 * @param camera_x Camera X position
 * @param camera_y Camera Y position
 * @param view_width View width
 * @param view_height View height
 * @param callback Drawing callback function
 * @param callback_user_data User data for callback
 */
void entity_draw(
    EseEntity *entity,
    float camera_x, float camera_y,
    float view_width, float view_height,
    EntityDrawTextureCallback texCallback,
    EntityDrawRectCallback rectCallback,
    void *callback_user_data
);

/**
 * @brief Adds a component to an entity.
 * 
 * @param entity Pointer to the EseEntity
 * @param comp Component data (entity takes ownership)
 * @return Component ID string, or NULL on failure
 */
const char *entity_component_add(EseEntity *entity, EseEntityComponent *comp);

/**
 * @brief Removes a component from an entity.
 * 
 * @param entity Pointer to the EseEntity
 * @param id ID of component to remove
 * @return true if component was removed, false otherwise
 */
bool entity_component_remove(EseEntity *entity, const char *id);

/**
 * @brief Adds a property from a EseLuaValue to an entity's Lua proxy table.
 * 
 * @details Retrieves the entity's Lua proxy table from the registry using lua_ref,
 *          converts the EseLuaValue to appropriate Lua stack values, and sets the
 *          property in the proxy table. Handles all EseLuaValue types including
 *          nested tables.
 * 
 * @param entity Pointer to the EseEntity to add the property to
 * @param value Pointer to the EseLuaValue containing the property data
 * 
 * @return true if property was successfully added, false on failure
 * 
 * @warning entity and value must not be NULL
 * @warning entity must have a valid lua_ref
 */
bool entity_add_prop(EseEntity *entity, EseLuaValue *value);

int entity_get_lua_ref(EseEntity *entity);

#endif // ESE_ENTITY_H
