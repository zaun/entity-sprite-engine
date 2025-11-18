#ifndef ESE_ENTITY_H
#define ESE_ENTITY_H

#include "entity/entity_lua.h"
#include "scripting/lua_engine_private.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_TAG_LENGTH 16
#define MAX_TAGS_PER_ENTITY 32

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEntity EseEntity;
typedef struct EseEntityComponent EseEntityComponent;
typedef struct EseRect EseRect;
typedef struct EseCollisionHit EseCollisionHit;
typedef struct EseArray EseArray;

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
typedef void (*EntityDrawTextureCallback)(float screen_x, float screen_y, float screen_w,
                                          float screen_h, uint64_t z_index, const char *texture_id,
                                          float texture_x1, float texture_y1, float texture_x2,
                                          float texture_y2, int width, int height, void *user_data);

/**
 * @brief Callback function type for entity rectangle drawing operations.
 *
 * @param screen_x      Screen X coordinate to draw at
 * @param screen_y      Screen Y coordinate to draw at
 * @param z_index       Draw order/depth
 * @param width         Width of the rectangle in pixels
 * @param height        Height of the rectangle in pixels
 * @param rotation      Rotation angle in radians
 * @param filled        Whether the rectangle should be filled (true) or just
 * outlined (false)
 * @param r             Red color component (0-255)
 * @param g             Green color component (0-255)
 * @param b             Blue color component (0-255)
 * @param a             Alpha color component (0-255)
 * @param user_data     User-provided callback data
 */
typedef void (*EntityDrawRectCallback)(float screen_x, float screen_y, uint64_t z_index, int width,
                                       int height, float rotation, bool filled, unsigned char r,
                                       unsigned char g, unsigned char b, unsigned char a,
                                       void *user_data);

/**
 * @brief Callback function type for entity polyline drawing operations.
 *
 * @param screen_x      Screen X coordinate to draw at
 * @param screen_y      Screen Y coordinate to draw at
 * @param z_index       Draw order/depth
 * @param points        Array of points (x,y pairs) for the polyline
 * @param point_count   Number of points in the array
 * @param stroke_width  Width of the stroke line
 * @param fill_r        Fill color red component (0-255)
 * @param fill_g        Fill color green component (0-255)
 * @param fill_b        Fill color blue component (0-255)
 * @param fill_a        Fill color alpha component (0-255)
 * @param stroke_r      Stroke color red component (0-255)
 * @param stroke_g      Stroke color green component (0-255)
 * @param stroke_b      Stroke color blue component (0-255)
 * @param stroke_a      Stroke color alpha component (0-255)
 * @param user_data     User-provided callback data
 */
typedef void (*EntityDrawPolyLineCallback)(float screen_x, float screen_y, uint64_t z_index,
                                           const float *points, size_t point_count,
                                           float stroke_width, unsigned char fill_r,
                                           unsigned char fill_g, unsigned char fill_b,
                                           unsigned char fill_a, unsigned char stroke_r,
                                           unsigned char stroke_g, unsigned char stroke_b,
                                           unsigned char stroke_a, void *user_data);

/**
 * @brief Structure containing pointers to all entity drawing callback
 * functions.
 *
 * @details This structure provides a convenient way to group all drawing
 * callbacks together, making it easy to pass them around as a single unit or
 * store them in a single data structure.
 *
 * @param draw_texture   Callback for drawing texture-based entities
 * @param draw_rect      Callback for drawing rectangle-based entities
 * @param draw_polyline  Callback for drawing polyline-based entities
 */
typedef struct EntityDrawCallbacks {
    EntityDrawTextureCallback draw_texture;
    EntityDrawRectCallback draw_rect;
    EntityDrawPolyLineCallback draw_polyline;
} EntityDrawCallbacks;

/**
 * @brief Creates a new EseEntity object.
 *
 * @param engine Pointer to a EseLuaEngine
 * @return Pointer to newly created EseEntity object
 *
 * @warning The returned EseEntity must be freed with entity_destroy() to
 * prevent memory leaks
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
 * @brief Sets the position of an entity.
 *
 * @param entity Pointer to the EseEntity
 * @param x X coordinate
 * @param y Y coordinate
 */
void entity_set_position(EseEntity *entity, float x, float y);

void entity_run_function_with_args(EseEntity *entity, const char *func_name, int argc,
                                   EseLuaValue *argv[]);

/**
 * @brief Process collision callbacks for a collision pair with known state.
 *
 * @param hit Pointer to the collision hit
 */
void entity_process_collision_callbacks(EseCollisionHit *hit);

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
 * @brief Gets the number of components attached to an entity.
 *
 * @param entity Pointer to the EseEntity
 * @return Number of components attached to the entity
 */
size_t entity_component_count(EseEntity *entity);

/**
 * @brief Adds a property from a EseLuaValue to an entity's Lua proxy table.
 *
 * @details Retrieves the entity's Lua proxy table from the registry using
 * lua_ref, converts the EseLuaValue to appropriate Lua stack values, and sets
 * the property in the proxy table. Handles all EseLuaValue types including
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
 * @param tag Tag string to add (will be capitalized and truncated to 16
 * characters)
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
 * @param to_world_coords If true, returns bounds in world coordinates. If
 * false, returns bounds relative to entity.
 * @return Pointer to a copy of the collision bounds rect (caller must free), or
 * NULL if no collider component exists
 */
EseRect *entity_get_collision_bounds(EseEntity *entity, bool to_world_coords);

int entity_get_lua_ref(EseEntity *entity);

void entity_set_visible(EseEntity *entity, bool visible);

bool entity_get_visible(EseEntity *entity);

void entity_set_persistent(EseEntity *entity, bool persistent);

bool entity_get_persistent(EseEntity *entity);

bool entity_test_collision(EseEntity *a, EseEntity *b, EseArray *out_hits);

#endif // ESE_ENTITY_H
