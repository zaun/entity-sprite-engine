/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Map Render System. Collects map components and renders
 * them to the draw list in the LATE phase, converting world coordinates to
 * screen coordinates using the camera.
 *
 * Details:
 * The system maintains a dynamic array of map component pointers for efficient
 * rendering. Components are added/removed via callbacks. During update, maps
 * are rendered with proper camera-relative positioning and map type handling
 * (grid, hex, isometric).
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/map_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_map.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/draw_list.h"
#include "graphics/sprite.h"
#include "types/tileset.h"
#include "types/point.h"
#include "types/rect.h"
#include "utility/log.h"
#include <math.h>
 
// ========================================
// Defines and Structs
// ========================================

/**
* @brief Internal data for the map render system.
*
* Maintains a dynamically-sized array of map component pointers for efficient
* rendering during the LATE phase.
*/
typedef struct {
    EseEntityComponentMap **maps; /** Array of map component pointers */
    size_t count;                 /** Current number of tracked maps */
    size_t capacity;              /** Allocated capacity of the array */
} MapRenderSystemData;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================
 
static bool map_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp);
static void map_render_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp);
static void map_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                    EseEntityComponent *comp);
static void map_render_sys_init(EseSystemManager *self, EseEngine *eng);
static void map_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt);
static void map_render_sys_shutdown(EseSystemManager *self, EseEngine *eng);
static void _map_render_draw(EseEntityComponentMap *component, float screen_x, float screen_y,
                            EntityDrawTextureCallback texCallback, void *callback_user_data);
static void _map_render_draw_grid(EseEntityComponentMap *component, float screen_x, float screen_y,
                                EntityDrawTextureCallback texCallback, void *callback_user_data);
static void _map_render_draw_hex_point_up(EseEntityComponentMap *component, float screen_x,
                                        float screen_y, EntityDrawTextureCallback texCallback,
                                        void *callback_user_data);
static void _map_render_draw_hex_flat_up(EseEntityComponentMap *component, float screen_x,
                                        float screen_y, EntityDrawTextureCallback texCallback,
                                        void *callback_user_data);
static void _map_render_draw_iso(EseEntityComponentMap *component, float screen_x, float screen_y,
                                EntityDrawTextureCallback texCallback, void *callback_user_data);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
* @brief Checks if the system accepts this component type.
*
* @param self System manager instance
* @param comp Component to check
* @return true if component type is ENTITY_COMPONENT_MAP
*/
static bool map_render_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
    (void)self;
    if (!comp) {
        return false;
    }
    return comp->type == ENTITY_COMPONENT_MAP;
}

/**
* @brief Called when a map component is added to an entity.
*
* @param self System manager instance
* @param eng Engine pointer
* @param comp Component that was added
*/
static void map_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                EseEntityComponent *comp) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;

    // Expand array if needed
    if (d->count == d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 64;
        d->maps = memory_manager.realloc(d->maps, sizeof(EseEntityComponentMap *) * d->capacity,
                                        MMTAG_RS_MAP);
    }

    // Add map to tracking array
    d->maps[d->count++] = (EseEntityComponentMap *)comp->data;
}

/**
* @brief Called when a map component is removed from an entity.
*
* @param self System manager instance
* @param eng Engine pointer
* @param comp Component that was removed
*/
static void map_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                    EseEntityComponent *comp) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;
    EseEntityComponentMap *mc = (EseEntityComponentMap *)comp->data;

    // Find and remove map from tracking array (swap with last element)
    for (size_t i = 0; i < d->count; i++) {
        if (d->maps[i] == mc) {
            d->maps[i] = d->maps[--d->count];
            return;
        }
    }
}

/**
* @brief Initialize the map render system.
*
* @param self System manager instance
* @param eng Engine pointer
*/
static void map_render_sys_init(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapRenderSystemData *d = memory_manager.calloc(1, sizeof(MapRenderSystemData), MMTAG_RS_MAP);
    self->data = d;
}

/**
* @brief Render all map components.
*
* Iterates through all tracked map components and submits them to the renderer.
*
* @param self System manager instance
* @param eng Engine pointer
* @param dt Delta time (unused)
*/
static void map_render_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
    (void)dt;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;

    if (!d || d->count == 0) {
        return;
    }

    for (size_t i = 0; i < d->count; i++) {
        EseEntityComponentMap *map = d->maps[i];
        if (!map || !map->base.entity || !map->base.entity->active || !map->base.entity->visible) {
            continue;
        }
        if (!map->map) {
            continue;
        }

        // Get entity world position
        float entity_x = ese_point_get_x(map->base.entity->position);
        float entity_y = ese_point_get_y(map->base.entity->position);

        // Convert world coordinates to screen coordinates using camera
        EseCamera *camera = engine_get_camera(eng);
        EseDisplay *display = engine_get_display(eng);
        float camera_x = ese_point_get_x(camera->position);
        float camera_y = ese_point_get_y(camera->position);
        float view_width = ese_display_get_viewport_width(display);
        float view_height = ese_display_get_viewport_height(display);

        float view_left = camera_x - view_width / 2.0f;
        float view_top = camera_y - view_height / 2.0f;

        float screen_x = entity_x - view_left;
        float screen_y = entity_y - view_top;

        // Render map to draw list
        EseDrawList *draw_list = engine_get_draw_list(eng);
        _map_render_draw(map, screen_x, screen_y, _engine_add_texture_to_draw_list, draw_list);
    }
}

/**
* @brief Shutdown the map render system.
*
* Cleans up allocated resources.
*
* @param self System manager instance
* @param eng Engine pointer
*/
static void map_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
    (void)eng;
    MapRenderSystemData *d = (MapRenderSystemData *)self->data;
    if (d) {
        if (d->maps) {
            memory_manager.free(d->maps);
        }
        memory_manager.free(d);
        self->data = NULL;
    }
}

/**
* @brief Draw dispatcher for map components based on map type.
*
* @param component Map component
* @param screen_x Screen X offset
* @param screen_y Screen Y offset
* @param texCallback Texture draw callback
* @param callback_user_data Draw list pointer
*/
static void _map_render_draw(EseEntityComponentMap *component, float screen_x, float screen_y,
                            EntityDrawTextureCallback texCallback, void *callback_user_data) {
    if (!component || !component->map) {
        log_debug("MAP_RENDER_SYS", "map not set or NULL component");
        return;
    }

    switch (ese_map_get_type(component->map)) {
    case MAP_TYPE_GRID:
        _map_render_draw_grid(component, screen_x, screen_y, texCallback, callback_user_data);
        break;
    case MAP_TYPE_HEX_POINT_UP:
        _map_render_draw_hex_point_up(component, screen_x, screen_y, texCallback,
                                    callback_user_data);
        break;
    case MAP_TYPE_HEX_FLAT_UP:
        _map_render_draw_hex_flat_up(component, screen_x, screen_y, texCallback,
                                    callback_user_data);
        break;
    case MAP_TYPE_ISO:
        _map_render_draw_iso(component, screen_x, screen_y, texCallback, callback_user_data);
        break;
    default:
        log_debug("MAP_RENDER_SYS", "unknown map type");
        break;
    }
}

/**
* @brief Draws a standard grid map.
*/
static void _map_render_draw_grid(EseEntityComponentMap *component, float screen_x, float screen_y,
                                EntityDrawTextureCallback texCallback, void *callback_user_data) {
    EseEngine *engine =
        (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    const int tw = component->size;
    const int th = component->size;
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < (uint32_t)mh; y++) {
        for (uint32_t x = 0; x < (uint32_t)mw; x++) {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * th;

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++) {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id =
                    ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id) {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                uint64_t z_index = component->base.entity->draw_order;
                z_index += ((uint64_t)(i * 2) << DRAW_ORDER_SHIFT);
                z_index += y * mw + x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(sprite, component->sprite_frames[y * mw + x], &texture_id, &x1,
                                &y1, &x2, &y2, &w, &h);

                texCallback(dx, dy, tw, th, z_index, texture_id, x1, y1, x2, y2, w, h,
                            callback_user_data);
            }
        }
    }
}

/**
* @brief Draws a hex map (point-up orientation).
*/
static void _map_render_draw_hex_point_up(EseEntityComponentMap *component, float screen_x,
                                        float screen_y, EntityDrawTextureCallback texCallback,
                                        void *callback_user_data) {
    EseEngine *engine =
        (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    const int th = component->size;
    const int tw = (int)(th * 0.866025f);
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < (uint32_t)mh; y++) {
        for (uint32_t x = 0; x < (uint32_t)mw; x++) {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * (th * 0.75f);
            if (y % 2 == 1) {
                dx += tw / 2.0f;
            }

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++) {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id =
                    ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id) {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                uint64_t z_index = component->base.entity->draw_order;
                z_index += y * mw + x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(sprite, component->sprite_frames[y * mw + x], &texture_id, &x1,
                                &y1, &x2, &y2, &w, &h);

                texCallback(dx, dy, tw, th, z_index, texture_id, x1, y1, x2, y2, w, h,
                            callback_user_data);
            }
        }
    }
}

/**
* @brief Draws a hex map (flat-up orientation).
*/
static void _map_render_draw_hex_flat_up(EseEntityComponentMap *component, float screen_x,
                                        float screen_y, EntityDrawTextureCallback texCallback,
                                        void *callback_user_data) {
    EseEngine *engine =
        (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    const int th = component->size;
    const int tw = (int)(th * 1.154701f);
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < (uint32_t)mh; y++) {
        for (uint32_t x = 0; x < (uint32_t)mw; x++) {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * (tw * 0.75f);
            float dy = screen_y + (y - cy) * th;
            if (x % 2 == 1) {
                dy += th / 2.0f;
            }

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++) {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id =
                    ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id) {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                uint64_t z_index = component->base.entity->draw_order;
                z_index += y * mw + x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(sprite, component->sprite_frames[y * mw + x], &texture_id, &x1,
                                &y1, &x2, &y2, &w, &h);

                texCallback(dx, dy, tw, th, z_index, texture_id, x1, y1, x2, y2, w, h,
                            callback_user_data);
            }
        }
    }
}

/**
* @brief Draws an isometric map (diamond layout).
*/
static void _map_render_draw_iso(EseEntityComponentMap *component, float screen_x, float screen_y,
                                EntityDrawTextureCallback texCallback, void *callback_user_data) {
    EseEngine *engine =
        (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(ese_map_get_tileset(component->map), component->seed);

    const int th = component->size;
    const int tw = th * 2;
    float cx = ese_point_get_x(component->position);
    float cy = ese_point_get_y(component->position);
    int mw = ese_map_get_width(component->map);
    int mh = ese_map_get_height(component->map);

    for (uint32_t y = 0; y < (uint32_t)mh; y++) {
        for (uint32_t x = 0; x < (uint32_t)mw; x++) {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * (tw / 2.0f) - (y - cy) * (tw / 2.0f);
            float dy = screen_y + (x - cx) * (th / 2.0f) + (y - cy) * (th / 2.0f);

            for (size_t i = 0; i < ese_map_cell_get_layer_count(cell); i++) {
                int tid = ese_map_cell_get_layer(cell, i);
                if (tid == -1 || !component->show_layer[i]) {
                    continue;
                }

                const char *sprite_id =
                    ese_tileset_get_sprite(ese_map_get_tileset(component->map), tid);
                if (!sprite_id) {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                uint64_t z_index = component->base.entity->draw_order;
                z_index += y * mw + x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(sprite, component->sprite_frames[y * mw + x], &texture_id, &x1,
                                &y1, &x2, &y2, &w, &h);

                texCallback(dx, dy, tw, th, z_index, texture_id, x1, y1, x2, y2, w, h,
                            callback_user_data);
            }
        }
    }
}

// ========================================
// VTABLE
// ========================================

static const EseSystemManagerVTable MapRenderSystemVTable = {
    .init = map_render_sys_init,
    .setup = NULL,
    .teardown = NULL,
    .update = map_render_sys_update,
    .accepts = map_render_sys_accepts,
    .on_component_added = map_render_sys_on_add,
    .on_component_removed = map_render_sys_on_remove,
    .shutdown = map_render_sys_shutdown
};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
* @brief Create a map render system.
*
* @return EseSystemManager* Created system
*/
EseSystemManager *map_render_system_create(void) {
    return system_manager_create(&MapRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
* @brief Register the map render system with the engine.
*
* @param eng Engine pointer
*/
void engine_register_map_render_system(EseEngine *eng) {
    log_assert("MAP_RENDER_SYS", eng, "engine_register_map_render_system called with NULL engine");
    EseSystemManager *sys = map_render_system_create();
    engine_add_system(eng, sys);
}