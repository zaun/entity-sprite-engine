#ifndef ESE_ENTITY_COMPONENT_MAP_H
#define ESE_ENTITY_COMPONENT_MAP_H

#include <string.h>
#include <stdint.h>
#include "entity/components/entity_component_private.h" // EseEntityComponent

#define ENTITY_COMPONENT_MAP_PROXY_META "EntityComponentMapProxyMeta"

typedef struct EseMap EseMap;
typedef struct EsePoint EsePoint;
typedef struct EseRect EseRect;
typedef struct EseMapCell EseMapCell;
typedef struct lua_State lua_State;

/**
 * @brief Component that provides tile-based map rendering capabilities to an entity.
 * 
 * @details This component manages map display, positioning, and rendering.
 *          It stores the map reference, map position for centering, tile size,
 *          random seed for procedural generation, and sprite frame data.
 *          The map reference is not owned and should not be freed.
 */
typedef struct EseEntityComponentMap
{
    EseEntityComponent base;        /** Base component structure */

    EseMap *map;                    /** Reference to the map to render (not owned) */
    EsePoint *position;             /** Map cell position to center on */
    int size;                       /** Tile size in pixels */
    uint32_t seed;                  /** Random seed for procedural generation */

    char *script;                   /** Filename of the Lua script to execute */
    EseLuaEngine *engine;           /** Reference to Lua engine (not owned) */
    int instance_ref;               /** Lua registry reference to instance table */
    
    EseHashMap *function_cache;     /** Cache of function references for performance */
    EseLuaValue *delta_time_arg;    /** Argument value passed to script functions */
    EseLuaValue *map_arg;           /** Argument value passed to script functions */
    EseLuaValue *cell_arg;          /** Argument value passed to script functions */

    bool *show_layer;               /** Array of boolean values to show/hide layers */
    size_t show_layer_count;        /** Number of layers to show */
    int *sprite_frames;             /** Array of sprite frame indices for tiles */
} EseEntityComponentMap;

EseEntityComponent *_entity_component_map_copy(const EseEntityComponentMap *src);

void _entity_component_map_destroy(EseEntityComponentMap *component);

void _entity_component_map_update(EseEntityComponentMap *component, EseEntity *entity, float delta_time);

EseEntityComponentMap *_entity_component_map_get(lua_State *L, int idx);

void _entity_component_map_init(EseLuaEngine *engine);

void _entity_component_map_draw(EseEntityComponentMap *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data);

EseEntityComponent *entity_component_map_create(EseLuaEngine *engine);

EseRect *entity_component_map_get_cell_rect(EseEntityComponentMap *comp, int x, int y);

#endif // ESE_ENTITY_COMPONENT_MAP_H
