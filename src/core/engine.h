/**
 * @file engine.h
 * 
 * @brief This file contains the public interface for the EseEngine.
 * 
 * @details The functions declared here provide the main entry points for creating, destroying, and
 * controlling the engine from a native application.
 */
#ifndef ESE_ENGINE_H
#define ESE_ENGINE_H

#include <stdbool.h>
#include "types/camera.h"
#include "types/display.h"
#include "types/input_state.h"
#include "core/console.h"
#include "scripting/lua_value.h"

typedef struct EseEntity EseEntity;
typedef struct EseEngine EseEngine;
typedef struct EseRenderer EseRenderer;
typedef struct EseRect EseRect;
typedef struct EseSprite EseSprite;
typedef struct EseEntityComponentMap EseEntityComponentMap;
typedef struct EseGui EseGui;
typedef struct EseJobQueue EseJobQueue;

/**
 * @brief Creates a new EseEngine instance.
 * 
 * @details This function allocates and initializes a new engine, including its renderer, asset
 * manager, and Lua engine. It also loads a startup script.
 * 
 * @param startup_script The path to the initial Lua script to load.
 * @return A pointer to the newly created EseEngine instance.
 * 
 * @note This function asserts that the `startup_script` pointer is not NULL.
 */
EseEngine *engine_create(const char *startup_script);

/**
 * @brief Destroys an EseEngine instance and frees its resources.
 * 
 * @details This function deallocates all memory associated with the engine, including the renderer,
 * render lists, entities, and Lua engine.
 * 
 * @param engine A pointer to the EseEngine instance to destroy.
 * 
 * @note This function asserts that the `engine` pointer is not NULL.
 */
void engine_destroy(EseEngine *engine);

/**
 * @brief Sets the renderer for the engine.
 * 
 * @details This function assigns a renderer to the engine and sets up the render list.
 *          If a renderer was previously set, the old asset manager is destroyed and a new one is created.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param renderer A pointer to the renderer to set, or NULL to clear the renderer.
 * 
 * @note This function asserts that the `engine` pointer is not NULL.
 */
void engine_set_renderer(EseEngine *engine, EseRenderer *renderer);

/**
 * @brief Adds an existing entity to the engine's management.
 * 
 * @details This function appends an entity to the engine's internal list, making it part of the
 * game loop for updates and rendering.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param entity A pointer to the EseEntity to add.
 * 
 * @note This function asserts that both the `engine` and `entity` pointers are not NULL.
 * @warning The engine will take ownership of the `entity` pointer, and it will be freed when the
 * engine is destroyed.
 */
void engine_add_entity(EseEngine *engine, EseEntity *entity);

/**
 * @brief Removes an existing entity from the engine's management.
 * 
 * @details This function appends an entity to the engine's internal list, making it part of the
 * game loop for updates and rendering.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param entity A pointer to the EseEntity to remove.
 * 
 * @note This function asserts that both the `engine` and `entity` pointers are not NULL.
 * @warning The engine will take ownership of the `entity` pointer, and it will be freed when the
 * engine is destroyed.
 */
 void engine_remove_entity(EseEngine *engine, EseEntity *entity);

/**
 * @brief Clears the engine's entities.
 * 
 * @param engine Pointer to the EseEngine
 * @param include_persistent Whether to include persistent entities
 */
void engine_clear_entities(EseEngine *engine, bool include_persistent);

/**
 * @brief Starts the engine's main loop.
 * 
 * @details This function should be called once by the native application to begin the engine's
 * lifecycle, typically by running the `startup` function in the main Lua script.
 * 
 * @param engine A pointer to the EseEngine instance.
 * 
 * @note This function asserts that the `engine` pointer is not NULL.
 */
void engine_start(EseEngine *engine);

/**
 * @brief Updates the engine state for a single frame.
 * 
 * @details This function processes input, updates all entities, and prepares the render lists for
 * the next frame. It should be called by the native application on every frame.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param delta_time The time elapsed since the last frame, in seconds.
 * @param state A pointer to the current input state.
 * 
 * @note This function asserts that both the `engine` and `state` pointers are not NULL.
 */
void engine_update(EseEngine *engine, float delta_time, const EseInputState *state);

/**
 * @brief Detects collisions between entities and a rectangular area.
 * 
 * @details This function checks all active entities in the engine against the specified
 *          rectangle and returns those that intersect with it.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param rect A pointer to the rectangle to test against.
 * @param max_count Maximum number of entities to return.
 * @return Array of entity pointers, NULL-terminated (caller must free).
 */
EseEntity **engine_detect_collision_rect(EseEngine *engine, EseRect *rect, int max_count);

/**
 * @brief Retrieves a sprite from the engine's asset manager.
 * 
 * @details This function looks up a sprite by its ID in the engine's asset manager
 *          and returns a pointer to it if found.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param sprite_id The ID of the sprite to retrieve.
 * @return Pointer to the sprite if found, NULL otherwise.
 */
EseSprite *engine_get_sprite(EseEngine *engine, const char *sprite_id);

/**
 * @brief Finds all entities with a specific tag.
 * 
 * @param engine Pointer to the EseEngine
 * @param tag Tag string to search for
 * @param max_count Maximum number of entities to return
 * @return Array of entity pointers, NULL-terminated (caller must free), or NULL if none found
 */
EseEntity **engine_find_by_tag(EseEngine *engine, const char *tag, int max_count);

/**
 * @brief Finds an entity by its UUID.
 * 
 * @param engine Pointer to the EseEngine
 * @param uuid_string UUID string to search for
 * @return Pointer to the found entity, or NULL if not found
 */
EseEntity *engine_find_by_id(EseEngine *engine, const char *uuid_string);

/**
 * @brief Gets the number of entities in the engine.
 * 
 * @param engine Pointer to the EseEngine
 * @return Number of entities in the engine
 */
int engine_get_entity_count(EseEngine *engine);

/**
 * @brief Adds a line to the engine's console.
 * 
 * @param engine Pointer to the engine instance.
 * @param type The type of console line (normal, info, warn, error).
 * @param prefix The prefix for the console line (max 16 characters).
 * @param message The message to display.
 */
void engine_add_to_console(EseEngine *engine, EseConsoleLineType type, const char *prefix, const char *message);

/**
 * @brief Sets whether the console should be drawn.
 * 
 * @param engine Pointer to the engine instance.
 * @param show True to show the console, false to hide it.
 */
void engine_show_console(EseEngine *engine, bool show);

// Pub/Sub passthrough functions
/**
 * @brief Publishes data to a topic.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param name The topic name to publish to.
 * @param data The EseLuaValue data to publish.
 */
void engine_pubsub_pub(EseEngine *engine, const char *name, const EseLuaValue *data);

/**
 * @brief Subscribes an entity to a topic with a function name.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param name The topic name to subscribe to.
 * @param entity The entity to call the function on.
 * @param function_name The name of the function to call on the entity.
 */
void engine_pubsub_sub(EseEngine *engine, const char *name, EseEntity *entity, const char *function_name);

/**
 * @brief Unsubscribes an entity from a topic.
 * 
 * @param engine A pointer to the EseEngine instance.
 * @param name The topic name to unsubscribe from.
 * @param entity The entity to unsubscribe.
 * @param function_name The name of the function that was subscribed.
 */
void engine_pubsub_unsub(EseEngine *engine, const char *name, EseEntity *entity, const char *function_name);

/**
 * @brief Retrieves the Nuklear context for the engine.
 *
 * This function returns a pointer to the Nuklear context, which can be used to
 * access the Nuklear API for custom UI elements or integration.
 *
 * @param engine Pointer to the EseEngine instance.
 * @return Pointer to the EseGui instance.
 */
EseGui *engine_get_gui(EseEngine *engine);

/**
 * @brief Returns the engine's job queue instance.
 *
 * @param engine Pointer to the EseEngine instance.
 * @return Pointer to the EseJobQueue managed by the engine.
 */
EseJobQueue *engine_get_job_queue(EseEngine *engine);

#endif // ESE_ENGINE_H
