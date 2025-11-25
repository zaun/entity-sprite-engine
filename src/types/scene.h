/**
 * @file scene.h
 * @brief Scene type for describing and instantiating sets of entities.
 *
 * @details Provides a C-owned Scene blueprint type that can snapshot core
 *          entity state from an EseEngine and later re-instantiate new
 *          entities matching that state. Lua bindings expose a Scene global
 *          with class methods (create/clear/reset) and instance methods
 *          (run/entity_count).
 */
#ifndef ESE_SCENE_H
#define ESE_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCENE_PROXY_META "SceneProxyMeta"

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEngine EseEngine;
typedef struct EseScene EseScene;

// Core lifecycle
EseScene *ese_scene_create(EseLuaEngine *engine);
void ese_scene_destroy(EseScene *scene);

// Query
size_t ese_scene_entity_count(const EseScene *scene);

// Snapshot from engine
EseScene *ese_scene_create_from_engine(EseEngine *engine, bool include_persistent);

// Instantiate into engine
void ese_scene_run(EseScene *scene, EseEngine *engine);

// Lua integration entry point
void ese_scene_lua_init(EseLuaEngine *engine);

#endif // ESE_SCENE_H
