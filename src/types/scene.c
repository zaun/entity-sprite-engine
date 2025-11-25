#include "types/scene.h"

#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "types/scene_lua.h"
#include "utility/double_linked_list.h"
#include "utility/log.h"
#include "vendor/json/cJSON.h"

#include <string.h>

// ========================================
// INTERNAL TYPES
// ========================================

typedef struct EseSceneComponentBlueprint {
    cJSON *json; // placeholder for future per-component JSON configuration
} EseSceneComponentBlueprint;

typedef struct EseSceneEntityDesc {
    bool active;
    bool visible;
    bool persistent;
    uint64_t draw_order;

    float x;
    float y;

    char **tags;
    size_t tag_count;
    size_t tag_capacity;

    EseSceneComponentBlueprint *components;
    size_t component_count;
    size_t component_capacity;
} EseSceneEntityDesc;

struct EseScene {
    EseLuaEngine *lua;

    EseSceneEntityDesc *entities;
    size_t entity_count;
    size_t entity_capacity;
};

// ========================================
// STATIC HELPERS
// ========================================

static void _ese_scene_free_entity_desc(EseSceneEntityDesc *desc) {
    if (!desc) {
        return;
    }

    // Free tags
    if (desc->tags) {
        for (size_t i = 0; i < desc->tag_count; i++) {
            if (desc->tags[i]) {
                memory_manager.free(desc->tags[i]);
            }
        }
        memory_manager.free(desc->tags);
    }
    desc->tags = NULL;
    desc->tag_count = 0;
    desc->tag_capacity = 0;

    // Free component blueprints (JSON placeholders for future use)
    if (desc->components) {
        for (size_t i = 0; i < desc->component_count; i++) {
            EseSceneComponentBlueprint *bp = &desc->components[i];
            if (bp->json) {
                cJSON_Delete(bp->json);
                bp->json = NULL;
            }
        }
        memory_manager.free(desc->components);
    }
    desc->components = NULL;
    desc->component_count = 0;
    desc->component_capacity = 0;
}

static bool _ese_scene_ensure_entity_capacity(EseScene *scene, size_t min_capacity) {
    if (scene->entity_capacity >= min_capacity) {
        return true;
    }

    size_t new_capacity = scene->entity_capacity == 0 ? 8 : scene->entity_capacity * 2;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    EseSceneEntityDesc *new_entities = memory_manager.realloc(
        scene->entities, sizeof(EseSceneEntityDesc) * new_capacity, MMTAG_ENTITY);
    if (!new_entities) {
        log_error("SCENE", "Failed to grow Scene entity array to %zu entries", new_capacity);
        return false;
    }

    // Zero-initialize the newly added slots
    if (new_capacity > scene->entity_capacity) {
        size_t old_bytes = scene->entity_capacity * sizeof(EseSceneEntityDesc);
        size_t new_bytes = new_capacity * sizeof(EseSceneEntityDesc);
        memset(((unsigned char *)new_entities) + old_bytes, 0, new_bytes - old_bytes);
    }

    scene->entities = new_entities;
    scene->entity_capacity = new_capacity;
    return true;
}

static void _ese_scene_clone_tags_from_entity(EseSceneEntityDesc *desc, const EseEntity *entity) {
    desc->tags = NULL;
    desc->tag_count = 0;
    desc->tag_capacity = 0;

    if (!entity || entity->tag_count == 0) {
        return;
    }

    desc->tags = memory_manager.malloc(sizeof(char *) * entity->tag_count, MMTAG_ENTITY);
    if (!desc->tags) {
        log_error("SCENE", "Failed to allocate tags array for Scene entity descriptor");
        return;
    }

    desc->tag_capacity = entity->tag_count;
    desc->tag_count = entity->tag_count;

    for (size_t i = 0; i < entity->tag_count; i++) {
        desc->tags[i] = memory_manager.strdup(entity->tags[i], MMTAG_ENTITY);
        if (!desc->tags[i]) {
            log_error("SCENE", "Failed to duplicate tag for Scene entity descriptor");
        }
    }
}

// Placeholder: later this will apply per-component JSON blueprints.
static bool entity_apply_blueprint(EseEntity *entity, const EseSceneEntityDesc *desc) {
    (void)entity;
    (void)desc;

    // TODO: Apply desc->components using per-component JSON serializers
    // once each component type exposes to/from JSON helpers.
    return true;
}

// ========================================
// PUBLIC API
// ========================================

EseScene *ese_scene_create(EseLuaEngine *engine) {
    log_assert("SCENE", engine, "ese_scene_create called with NULL engine");

    EseScene *scene = memory_manager.malloc(sizeof(EseScene), MMTAG_ENTITY);
    if (!scene) {
        log_error("SCENE", "Failed to allocate EseScene");
        return NULL;
    }

    scene->lua = engine;
    scene->entities = NULL;
    scene->entity_count = 0;
    scene->entity_capacity = 0;

    return scene;
}

void ese_scene_destroy(EseScene *scene) {
    if (!scene) {
        return;
    }

    for (size_t i = 0; i < scene->entity_count; i++) {
        _ese_scene_free_entity_desc(&scene->entities[i]);
    }

    if (scene->entities) {
        memory_manager.free(scene->entities);
    }

    memory_manager.free(scene);
}

size_t ese_scene_entity_count(const EseScene *scene) {
    if (!scene) {
        return 0;
    }
    return scene->entity_count;
}

EseScene *ese_scene_create_from_engine(EseEngine *engine, bool include_persistent) {
    log_assert("SCENE", engine, "ese_scene_create_from_engine called with NULL engine");
    log_assert("SCENE", engine->lua_engine,
               "ese_scene_create_from_engine called with engine missing lua_engine");

    EseScene *scene = ese_scene_create(engine->lua_engine);
    if (!scene) {
        return NULL;
    }

    EseDListIter *iter = dlist_iter_create(engine->entities);
    void *value = NULL;
    while (dlist_iter_next(iter, &value)) {
        EseEntity *entity = (EseEntity *)value;
        if (!entity) {
            continue;
        }

        if (entity->destroyed) {
            continue;
        }

        if (!include_persistent && entity_get_persistent(entity)) {
            continue;
        }

        if (!_ese_scene_ensure_entity_capacity(scene, scene->entity_count + 1)) {
            // Allocation failure: clean up and abort
            dlist_iter_free(iter);
            ese_scene_destroy(scene);
            return NULL;
        }

        EseSceneEntityDesc *desc = &scene->entities[scene->entity_count++];

        memset(desc, 0, sizeof(*desc));

        desc->active = entity->active;
        desc->visible = entity->visible;
        desc->persistent = entity->persistent;
        desc->draw_order = entity->draw_order;

        desc->x = ese_point_get_x(entity->position);
        desc->y = ese_point_get_y(entity->position);

        _ese_scene_clone_tags_from_entity(desc, entity);

        // Components blueprints are not populated yet; they remain empty until
        // per-component JSON helpers are available.
    }

    dlist_iter_free(iter);

    return scene;
}

void ese_scene_run(EseScene *scene, EseEngine *engine) {
    log_assert("SCENE", scene, "ese_scene_run called with NULL scene");
    log_assert("SCENE", engine, "ese_scene_run called with NULL engine");

    for (size_t i = 0; i < scene->entity_count; i++) {
        EseSceneEntityDesc *desc = &scene->entities[i];

        EseEntity *entity = entity_create(engine->lua_engine);
        if (!entity) {
            log_error("SCENE", "Failed to create entity while running scene");
            continue;
        }

        entity->active = desc->active;
        entity->visible = desc->visible;
        entity->persistent = desc->persistent;
        entity->draw_order = desc->draw_order;

        entity_set_position(entity, desc->x, desc->y);

        for (size_t ti = 0; ti < desc->tag_count; ti++) {
            if (desc->tags[ti]) {
                entity_add_tag(entity, desc->tags[ti]);
            }
        }

        if (!entity_apply_blueprint(entity, desc)) {
            log_error("SCENE", "Failed to apply component blueprint for entity in scene");
        }

        engine_add_entity(engine, entity);
    }
}

void ese_scene_lua_init(EseLuaEngine *engine) {
    log_assert("SCENE", engine, "ese_scene_lua_init called with NULL engine");
    _ese_scene_lua_init(engine);
}
