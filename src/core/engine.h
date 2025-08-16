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

typedef struct EseEntity EseEntity;
typedef struct EseEngine EseEngine;
typedef struct EseRenderer EseRenderer;

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

#endif // ESE_ENGINE_H
