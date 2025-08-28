#ifndef ESE_ENTITY_H
#define ESE_ENTITY_H

#include <stdbool.h>
#include "entity/entity_lua.h"

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;
typedef struct EseRect EseRect;

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
    int width, int height, float rotation, bool filled,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a,
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
 * @brief Check for a collision between entity and test, returning collision state.
 * 
 * @param entity Pointer to the primary EseEntity to check
 * @param test Pointer to the secondary EseEntity to check
 * @return int indicating the collision state (0=none, 1=enter, 2=stay, 3=exit)
 */
int entity_check_collision_state(EseEntity *entity, EseEntity *test);

/**
 * @brief Process collision callbacks for a collision pair with known state.
 * 
 * @param entity_a Pointer to the first EseEntity
 * @param entity_b Pointer to the second EseEntity  
 * @param state The collision state to process (0=none, 1=enter, 2=stay, 3=exit)
 */
void entity_process_collision_callbacks(EseEntity *entity_a, EseEntity *entity_b, int state);

/**
 * @brief Check for a collision between entitiy and a rect.
 * 
 * @param entity Pointer to the an EseEntity to check
 * @param test Pointer to the an EseRect to test
 * 
 * @return True is there is a collisiton false if not.
 */
bool entity_detect_collision_rect(EseEntity *entity, EseRect *rect);

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

/**
 * @brief Adds a tag to an entity.
 * 
 * @param entity Pointer to the EseEntity
 * @param tag Tag string to add (will be capitalized and truncated to 16 characters)
 * @return true if tag was added, false if already exists or on failure
 */
bool entity_add_tag(EseEntity *entity, const char *tag);

/**
 * @brief Removes a tag from an entity.
 * 
 * @param entity Pointer to the EseEntity
 * @param tag Tag string to remove
 * @return true if tag was removed, false if not found
 */
bool entity_remove_tag(EseEntity *entity, const char *tag);

/**
 * @brief Checks if an entity has a specific tag.
 * 
 * @param entity Pointer to the EseEntity
 * @param tag Tag string to check
 * @return true if entity has the tag, false otherwise
 */
bool entity_has_tag(EseEntity *entity, const char *tag);

/**
 * @brief Gets the entity's collision bounds.
 * 
 * @param entity Pointer to the EseEntity
 * @param to_world_coords If true, returns bounds in world coordinates. If false, returns bounds relative to entity.
 * @return Pointer to a copy of the collision bounds rect (caller must free), or NULL if no collider component exists
 */
EseRect *entity_get_collision_bounds(EseEntity *entity, bool to_world_coords);

int entity_get_lua_ref(EseEntity *entity);

#endif // ESE_ENTITY_H
