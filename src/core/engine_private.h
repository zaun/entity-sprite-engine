/**
 * @file engine_private.h
 * 
 * @brief This file contains the private declarations for the EseEngine struct and its internal helper
 * functions.
 * 
 * @details These declarations are not intended for public use and provide the internal
 * implementation details for the engine's core functionality, such as managing render lists and
 * entities.
 */
#ifndef ESE_ENGINE_PRIVATE_H
#define ESE_ENGINE_PRIVATE_H

#include "core/asset_manager.h"
#include "core/console.h"
#include "core/engine.h"
#include "entity/entity.h"
#include "scripting/lua_engine.h"

typedef struct EseDrawList EseDrawList;
typedef struct EseRenderList EseRenderList;
typedef struct EseRenderer EseRenderer;
typedef struct EseDoubleLinkedList EseDoubleLinkedList;

/**
 * @brief Statistics tracking structure for the engine performance metrics.
 */
 typedef struct EseEngineStats {
    float updates_per_second;            /**< Average updates per second */
    float entity_count_average;          /**< Current number of entities in the engine */
    float entity_update_average_time;    /**< Average time spent updating entities per frame */
    float entity_collision_average_time; /**< Average time spent on collision detection per frame */
    float entity_draw_average_time;      /**< Average time spent drawing entities per frame */
    float lua_gc_average_time;           /**< Average time spent on Lua garbage collection per frame */
} EseEngineStats;

struct EseEngine {
    EseRenderer *renderer;              /**< Pointer to the engine's renderer */
    EseDrawList *draw_list;             /**< Flat render lists used in processes */
    EseRenderList *render_list_a;       /**< First render list for double buffering */
    EseRenderList *render_list_b;       /**< Second render list for double buffering */
    bool active_render_list;            /**< Flag to indicate which render list is currently active */

    EseDoubleLinkedList *entities;      /**< A doubly-linked list containing all active entities */
    EseInputState *input_state;         /**< The current input state of the application */
    EseDisplay *display_state;          /**< The current display state of the application */
    EseCamera *camera_state;            /**M The current camera state of the application */
    EseAssetManager *asset_manager;     /**< Pointer to the engine's asset manager */
    EseLuaEngine *lua_engine;           /**< Pointer to the Lua scripting engine */

    EseConsole *console;                /**< Pointer to the engine's console */

    int startup_ref;                    /**< Reference to the startup script in the Lua registry */
    bool draw_console;                  /**< Whether to draw the console */

    bool isRunning;
    
    EseEngineStats stats;               /**< Engine performance statistics */
};

/**
 * @brief Clears the active render list.
 * 
 * @details This private function determines which of the two render lists is currently active and
 * clears its contents, preparing it for the next frame's drawing commands.
 * 
 * @param engine A pointer to the EseEngine instance.
 * 
 * @note This function asserts that the engine pointer is not NULL.
 */
void _engine_render_list_clear(EseEngine *engine);

/**
 * @brief Adds a drawable object to the active render list.
 * 
 * @details This private function is a callback used to add a new render object with specified
 * position, texture, and z-index to the currently active render list.
 * 
 * @param screen_x The x-coordinate of the object on the screen.
 * @param screen_y The y-coordinate of the object on the screen.
 * @param screen_w The width of the object on the screen.
 * @param screen_h The height of the object on the screen.
 * @param z_index The z-index (draw order) of the object.
 * @param texture_id The ID of the texture to use.
 * @param texture_x1 The x1-coordinate of the texture on the sprite sheet (normalized).
 * @param texture_y2 The y1-coordinate of the texture on the sprite sheet (normalized).
 * @param texture_x1 The x2-coordinate of the texture on the sprite sheet (normalized).
 * @param texture_y2 The y2-coordinate of the texture on the sprite sheet (normalized).
 * @param width
 * @param 
 * @param user_data A pointer to the EseRenderList to add the object to.
 * 
 * @note This function asserts that the user_data pointer is not NULL.
 * @warning The `user_data` pointer is assumed to be a valid `EseRenderList*`.
 */
void _engine_add_texture_to_draw_list(
    float screen_x, float screen_y, float screen_w, float screen_h, int z_index,
    const char *texture_id, float texture_x1, float texture_y1, float texture_x2, float texture_y2,
    int width, int height,
    void *user_data
);

void _engine_add_rect_to_draw_list(
    float screen_x, float screen_y, int z_index,
    int width, int height, float rotation, bool filled,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a,
    void *user_data
);

/**
 * @brief Swaps the active render list.
 * 
 * @details This private function flips the active render list flag and sets the renderer to use the
 * newly active list, effectively preparing for the next frame while the previous one is being
 * rendered.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @return Returns true upon completion.
 * 
 * @note This function asserts that the engine pointer is not NULL.
 */
bool _engine_render_flip(EseEngine *engine);

/**
 * @brief Gets the currently active render list.
 * 
 * @details This private function returns a pointer to the render list that is currently being
 * populated with draw commands.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @return A pointer to the active EseRenderList.
 * 
 * @note This function asserts that the engine pointer is not NULL.
 */
EseRenderList *_engine_get_render_list(EseEngine *engine);

/**
 * @brief Creates a new entity and adds it to the engine.
 * 
 * @details This private function creates a new entity, initializes it, and appends it to the
 * engine's internal list of entities.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param id A unique string identifier for the new entity.
 * @return A pointer to the newly created EseEntity.
 * 
 * @note This function asserts that the engine pointer is not NULL.
 * @warning The `id` string is used to find the entity later, so it must be unique. The new entity
 * is appended to the engine's `entities` list and will be owned by the engine.
 */
EseEntity *_engine_new_entity(EseEngine *engine, const char *id);

/**
 * @brief Finds an entity by its ID.
 * 
 * @details This private function searches through the engine's entity list to find an entity with a
 * matching string ID.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param id The string ID of the entity to find.
 * @return A pointer to the found EseEntity, or NULL if no entity with the given ID is found.
 * 
 * @note This function asserts that both the engine and id pointers are not NULL.
 */
EseEntity *_engine_find_entity(EseEngine *engine, const char *id);

/**
 * @brief Deletes an entity from the engine.
 * 
 * @details This private function removes an entity from the engine's internal list and frees its
 * memory.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param entity A pointer to the EseEntity to be deleted.
 * 
 * @note This function asserts that both the engine and entity pointers are not NULL.
 */
void _engine_delete_entity(EseEngine *engine, EseEntity *entity);

#endif // ESE_ENGINE_PRIVATE_H
