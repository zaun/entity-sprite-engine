#include <string.h>
#include "core/memory_manager.h"
#include "utility/log.h"
#include "scripting/lua_engine.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_map.h"
#include "core/asset_manager.h"
#include "core/engine_private.h"
#include "graphics/sprite.h"
#include "entity/entity_private.h"
#include "types/types.h"

// VTable wrapper functions
static EseEntityComponent* _map_vtable_copy(EseEntityComponent* component) {
    return _entity_component_ese_map_copy((EseEntityComponentMap*)component->data);
}

static void _map_vtable_destroy(EseEntityComponent* component) {
    _entity_component_ese_map_destroy((EseEntityComponentMap*)component->data);
}

static void _map_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    _entity_component_ese_map_update((EseEntityComponentMap*)component->data, entity, delta_time);
}

static void _map_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    EntityDrawCallbacks* draw_callbacks = (EntityDrawCallbacks*)callbacks;
    _entity_component_ese_map_draw((EseEntityComponentMap*)component->data, screen_x, screen_y, draw_callbacks->draw_texture, user_data);
}

static bool _map_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    // Map components don't support function execution
    return false;
}

static void _map_vtable_ref(EseEntityComponent* component) {
}

static void _map_vtable_unref(EseEntityComponent* component) {
}

// Static vtable instance for map components
static const ComponentVTable map_vtable = {
    .copy = _map_vtable_copy,
    .destroy = _map_vtable_destroy,
    .update = _map_vtable_update,
    .draw = _map_vtable_draw,
    .run_function = _map_vtable_run_function,
    .ref = _map_vtable_ref,
    .unref = _map_vtable_unref
};

static void _entity_component_ese_map_register(EseEntityComponentMap *component, bool is_lua_owned)
{
    log_assert("ENTITY_COMP", component, "_entity_component_ese_map_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_ese_map_register component is already registered");

    lua_newtable(component->base.lua->runtime);
    lua_pushlightuserdata(component->base.lua->runtime, component);
    lua_setfield(component->base.lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(component->base.lua->runtime, is_lua_owned);
    lua_setfield(component->base.lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(component->base.lua->runtime, ENTITY_COMPONENT_MAP_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
}

static EseEntityComponent *_entity_component_ese_map_make(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "_entity_component_ese_map_make called with NULL engine");

    EseEntityComponentMap *component = memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    ese_uuid_ref(component->base.id);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.type = ENTITY_COMPONENT_MAP;
    component->base.vtable = &map_vtable;
    
    component->map = NULL;
    component->ese_map_pos = ese_point_create(engine);
    ese_point_ref(component->ese_map_pos);
    component->size = 128;
    component->seed = 1000;
    component->sprite_frames = NULL;

    return &component->base;
}

EseEntityComponent *_entity_component_ese_map_copy(const EseEntityComponentMap *src)
{
    log_assert("ENTITY_COMP", src, "_entity_component_ese_map_copy called with NULL src");

    EseEntityComponentMap *copy = memory_manager.malloc(sizeof(EseEntityComponentMap), MMTAG_ENTITY);
    copy->base.data = copy;
    copy->base.active = true;
    copy->base.id = ese_uuid_create(src->base.lua);
    ese_uuid_ref(copy->base.id);
    copy->base.lua = src->base.lua;
    copy->base.lua_ref = LUA_NOREF;
    copy->base.type = ENTITY_COMPONENT_MAP;

    copy->map = src->map;
    copy->ese_map_pos = ese_point_create(src->base.lua);
    ese_point_ref(copy->ese_map_pos);
    ese_point_set_x(copy->ese_map_pos, ese_point_get_x(src->ese_map_pos));
    ese_point_set_y(copy->ese_map_pos, ese_point_get_y(src->ese_map_pos));

    size_t cells = copy->map->width * copy->map->height;
    copy->sprite_frames = memory_manager.malloc(sizeof(int) * cells, MMTAG_ENTITY);
    memset(copy->sprite_frames, 0, cells);

    return &copy->base;
}

void _entity_component_ese_map_destroy(EseEntityComponentMap *component)
{
    log_assert("ENTITY_COMP", component, "_entity_component_ese_map_destroy called with NULL src");

    // we don't own component->map
    ese_uuid_destroy(component->base.id);
    ese_point_destroy(component->ese_map_pos);

    memory_manager.free(component->sprite_frames);

    memory_manager.free(component);
}

void _entity_component_ese_map_update(EseEntityComponentMap *component, EseEntity *entity, float delta_time)
{
    log_assert("ENTITY_COMP", component, "_entity_component_ese_map_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_ese_map_update called with NULL src");
}

/**
 * @brief Lua function to create a new EseEntityComponentMap object.
 *
 * @details Callable from Lua as EseEntityComponentMap.new().
 *
 * @param L Lua state pointer
 * @return Number of return values (always 1 - the new point object)
 *
 * @warning Items created in Lua are owned by Lua
 */
static int _entity_component_ese_map_new(lua_State *L)
{
    const char *collider_name = NULL;

    int n_args = lua_gettop(L);
    if (n_args != 0)
    {
        log_debug("ENTITY_COMP", "EntityComponentCollider.new()");
        lua_pushnil(L);
        return 1;
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_ese_map_make(lua);

    // Push EseEntityComponent to Lua
    _entity_component_ese_map_register((EseEntityComponentMap *)component->data, true);
    entity_component_push(component);

    return 1;
}

EseEntityComponentMap *_entity_component_ese_map_get(lua_State *L, int idx)
{

    // Check if the value at idx is a table
    if (!lua_istable(L, idx))
    {
        return NULL;
    }

    // Check if it has the correct metatable
    if (!lua_getmetatable(L, idx))
    {
        return NULL; // No metatable
    }

    // Get the expected metatable for comparison
    luaL_getmetatable(L, ENTITY_COMPONENT_MAP_PROXY_META);

    // Compare metatables
    if (!lua_rawequal(L, -1, -2))
    {
        lua_pop(L, 2); // Pop both metatables
        return NULL;   // Wrong metatable
    }

    lua_pop(L, 2); // Pop both metatables

    // Get the __ptr field
    lua_getfield(L, idx, "__ptr");

    // Check if __ptr exists and is light userdata
    if (!lua_islightuserdata(L, -1))
    {
        lua_pop(L, 1); // Pop the __ptr value (or nil)
        return NULL;
    }

    // Extract the pointer
    void *comp = lua_touserdata(L, -1);
    lua_pop(L, 1); // Pop the __ptr value

    return (EseEntityComponentMap *)comp;
}

/**
 * @brief Lua __index metamethod for EseEntityComponentMap objects (getter).
 *
 * @details Handles property access for EseEntityComponentMap objects from Lua.
 *
 * @param L Lua state pointer
 * @return Number of return values pushed to Lua stack (1 for valid properties, 0 otherwise)
 */
static int _entity_component_ese_map_index(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_ese_map_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Return nil for freed components
    if (!component)
    {
        lua_pushnil(L);
        return 1;
    }

    if (!key)
        return 0;

    if (strcmp(key, "active") == 0)
    {
        lua_pushboolean(L, component->base.active);
        return 1;
    }
    else if (strcmp(key, "id") == 0)
    {
        lua_pushstring(L, ese_uuid_get_value(component->base.id));
        return 1;
    }
    else if (strcmp(key, "map") == 0)
    {
        if (component->map)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->map->lua_ref);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }
    else if (strcmp(key, "position") == 0)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ese_point_get_lua_ref(component->ese_map_pos));
        return 1;
    }
    else if (strcmp(key, "size") == 0)
    {
        lua_pushnumber(L, component->size);
        return 1;
    }
    else if (strcmp(key, "seed") == 0)
    {
        lua_pushnumber(L, component->seed);
        return 1;
    }

    return 0;
}

/**
 * @brief Lua __newindex metamethod for EseEntityComponentMap objects (setter).
 *
 * @details Handles property assignment for EseEntityComponentMap objects from Lua.
 *
 * @param L Lua state pointer
 * @return Always returns 0 (no return values) or throws Lua error for invalid operations
 */
static int _entity_component_ese_map_newindex(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_ese_map_get(L, 1);
    const char *key = lua_tostring(L, 2);

    // SAFETY: Silently ignore writes to freed components
    if (!component)
    {
        return 0;
    }

    if (!key)
        return 0;

    if (strcmp(key, "active") == 0)
    {
        if (!lua_isboolean(L, 3))
        {
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        lua_pushboolean(L, component->base.active);
        return 1;
    }
    else if (strcmp(key, "id") == 0)
    {
        return luaL_error(L, "id is read-only");
    }
    else if (strcmp(key, "map") == 0)
    {
        if (component->map) {
            ese_map_unref(component->map);
        }
        printf("Setting map\n");
        component->map = ese_map_lua_get(L, 3);
        if (!component->map) {
            return luaL_error(L, "map must be a Map object");
        }

        ese_map_ref(component->map);

        if (component->sprite_frames) {
            memory_manager.free(component->sprite_frames);
        }

        size_t cells = component->map->width * component->map->height;
        component->sprite_frames = memory_manager.malloc(sizeof(int) * cells, MMTAG_ENTITY);
        memset(component->sprite_frames, 0, cells);

        return 0;
    }
    else if (strcmp(key, "position") == 0)
    {
        EsePoint *new_position_point = ese_point_lua_get(L, 3);
        if (!new_position_point)
        {
            return luaL_error(L, "Entity position must be a EsePoint object");
        }
        // Copy values, don't copy reference (ownership safety)
        ese_point_set_x(component->ese_map_pos, ese_point_get_x(new_position_point));
        ese_point_set_y(component->ese_map_pos, ese_point_get_y(new_position_point));
        return 0;
    }
    else if (strcmp(key, "size") == 0)
    {
        if (!lua_isinteger_lj(L, 3))
        {
            return luaL_error(L, "size must be a number");
        }

        int new_size = lua_tointeger(L, 3);

        if (new_size < 0) {
            new_size = 0;
        }
        component->size = new_size;
        return 0;
    }
    else if (strcmp(key, "seed") == 0)
    {
        if (!lua_isnumber(L, 3))
        {
            return luaL_error(L, "seed must be a number");
        }

        uint32_t new_seed = (uint32_t)lua_tointeger(L, 3);

        if (new_seed < 0) {
            new_seed = 0;
        }
        component->seed = new_seed;
        return 0;
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

/**
 * @brief Lua __gc metamethod for EseEntityComponentMap objects.
 *
 * @details Checks the '__is_lua_owned' flag in the proxy table. If true,
 * it means this EseEntityComponentMap's memory was allocated by Lua and should be freed.
 * If false, the EseEntityComponentMap's memory is managed externally (by C) and is not freed here.
 *
 * @param L Lua state pointer
 * @return Always 0 (no return values)
 */
static int _entity_component_ese_map_gc(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_ese_map_get(L, 1);

    if (component)
    {
        lua_getfield(L, 1, "__is_lua_owned");
        bool is_lua_owned = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (is_lua_owned)
        {
            _entity_component_ese_map_destroy(component);
            log_debug("LUA_GC", "EseEntityComponentMap object (Lua-owned) garbage collected and C memory freed.");
        }
        else
        {
            log_debug("LUA_GC", "EseEntityComponentMap object (C-owned) garbage collected, C memory *not* freed.");
        }
    }

    return 0;
}

static int _entity_component_ese_map_tostring(lua_State *L)
{
    EseEntityComponentMap *component = _entity_component_ese_map_get(L, 1);

    if (!component)
    {
        lua_pushstring(L, "EseEntityComponentMap: (invalid)");
        return 1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "EseEntityComponentMap: %p (id=%s active=%s ma[]=%p)",
             (void *)component,
             ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false",
             component->map);
    lua_pushstring(L, buf);

    return 1;
}

void _entity_component_ese_map_init(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "_entity_component_ese_map_init called with NULL engine");

    lua_State *L = engine->runtime;

    // Register EntityComponentMap metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_MAP_PROXY_META))
    {
        log_debug("LUA", "Adding %s to engine", ENTITY_COMPONENT_MAP_PROXY_META);
        lua_pushstring(L, ENTITY_COMPONENT_MAP_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_ese_map_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_ese_map_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_ese_map_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_ese_map_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);

    // Create global EntityComponentMap table with constructor
    lua_getglobal(L, "EntityComponentMap");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EntityComponentMap table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_ese_map_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentMap");
    }
    else
    {
        lua_pop(L, 1);
    }
}

void _entity_component_ese_map_draw_grid(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{

    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(component->map->tileset, component->seed);

    // tile display size
    const int tw = component->size, th = component->size;

    // center cell
    float cx = ese_point_get_x(component->ese_map_pos);
    float cy = ese_point_get_y(component->ese_map_pos);

    for (uint32_t y = 0; y < component->map->height; y++)
    {
        for (uint32_t x = 0; x < component->map->width; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * th;

            for (size_t i = 0; i < ese_mapcell_get_layer_count(cell); i++)
            {
                uint8_t tid = ese_mapcell_get_layer(cell, i);
                const char *sprite_id = ese_tileset_get_sprite(component->map->tileset, tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * component->map->width;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * component->map->width + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_ese_map_draw_hex_point_up(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(component->map->tileset, component->seed);

    // For hex point up: width = height * sqrt(3) / 2
    const int th = component->size;
    const int tw = (int)(th * 0.866025f); // sqrt(3) / 2 ≈ 0.866025

    // center cell
    float cx = ese_point_get_x(component->ese_map_pos);
    float cy = ese_point_get_y(component->ese_map_pos);

    for (uint32_t y = 0; y < component->map->height; y++)
    {
        for (uint32_t x = 0; x < component->map->width; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Hex point up positioning: offset every other row
            float dx = screen_x + (x - cx) * tw;
            float dy = screen_y + (y - cy) * (th * 0.75f); // 3/4 of height for vertical spacing
            
            // Offset odd rows by half width
            if (y % 2 == 1) {
                dx += tw / 2.0f;
            }

            for (size_t i = 0; i < ese_mapcell_get_layer_count(cell); i++)
            {
                uint8_t tid = ese_mapcell_get_layer(cell, i);
                const char *sprite_id = ese_tileset_get_sprite(component->map->tileset, tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * component->map->width;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * component->map->width + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_ese_map_draw_hex_flat_up(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(component->map->tileset, component->seed);

    // For hex flat up: width = height * 2 / sqrt(3)
    const int th = component->size;
    const int tw = (int)(th * 1.154701f); // 2 / sqrt(3) ≈ 1.154701

    // center cell
    float cx = ese_point_get_x(component->ese_map_pos);
    float cy = ese_point_get_y(component->ese_map_pos);

    for (uint32_t y = 0; y < component->map->height; y++)
    {
        for (uint32_t x = 0; x < component->map->width; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Hex flat up positioning: offset every other column
            float dx = screen_x + (x - cx) * (tw * 0.75f); // 3/4 of width for horizontal spacing
            float dy = screen_y + (y - cy) * th;
            
            // Offset odd columns by half height
            if (x % 2 == 1) {
                dy += th / 2.0f;
            }

            for (size_t i = 0; i < ese_mapcell_get_layer_count(cell); i++)
            {
                uint8_t tid = ese_mapcell_get_layer(cell, i);
                const char *sprite_id = ese_tileset_get_sprite(component->map->tileset, tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * component->map->width;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * component->map->width + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_ese_map_draw_iso(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    EseEngine *engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
    ese_tileset_set_seed(component->map->tileset, component->seed);

    // For isometric: width = height * 2 (standard 2:1 isometric ratio)
    const int th = component->size;
    const int tw = th * 2;

    // center cell
    float cx = ese_point_get_x(component->ese_map_pos);
    float cy = ese_point_get_y(component->ese_map_pos);

    for (uint32_t y = 0; y < component->map->height; y++)
    {
        for (uint32_t x = 0; x < component->map->width; x++)
        {
            EseMapCell *cell = ese_map_get_cell(component->map, x, y);
            
            // Isometric positioning: diamond-shaped grid
            float dx = screen_x + (x - cx) * (tw / 2.0f) - (y - cy) * (tw / 2.0f);
            float dy = screen_y + (x - cx) * (th / 2.0f) + (y - cy) * (th / 2.0f);

            for (size_t i = 0; i < ese_mapcell_get_layer_count(cell); i++)
            {
                uint8_t tid = ese_mapcell_get_layer(cell, i);
                const char *sprite_id = ese_tileset_get_sprite(component->map->tileset, tid);
                if (!sprite_id)
                {
                    continue;
                }

                EseSprite *sprite = engine_get_sprite(engine, sprite_id);
                if (!sprite) {
                    continue;
                }

                int z_index = component->base.entity->draw_order;
                z_index += y * component->map->width;
                z_index += x;

                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                sprite_get_frame(
                    sprite, component->sprite_frames[y * component->map->width + x],
                    &texture_id, &x1, &y1, &x2, &y2, &w, &h
                );

                texCallback(
                    dx, dy, tw, th,
                    z_index,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data);
            }
        }
    }
}

void _entity_component_ese_map_draw(
    EseEntityComponentMap *component,
    float screen_x, float screen_y,
    EntityDrawTextureCallback texCallback,
    void *callback_user_data)
{
    if (!component->map) {
        log_debug("ENTITY_COMP_MAP", "map not set");
        return;
    }

    switch(component->map->type)
    {
        case MAP_TYPE_GRID:
            _entity_component_ese_map_draw_grid(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_HEX_POINT_UP:
            _entity_component_ese_map_draw_hex_point_up(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_HEX_FLAT_UP:
            _entity_component_ese_map_draw_hex_flat_up(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        case MAP_TYPE_ISO:
            _entity_component_ese_map_draw_iso(component, screen_x, screen_y, texCallback, callback_user_data);
            break;
        default:
            log_debug("ENTITY_COMP_MAP", "map type not supported");
            break;
    }
}

EseEntityComponent *entity_component_ese_map_create(EseLuaEngine *engine)
{
    log_assert("ENTITY_COMP", engine, "entity_component_ese_map_create called with NULL engine");

    EseEntityComponent *component = _entity_component_ese_map_make(engine);

    // Push EseEntityComponent to Lua
    _entity_component_ese_map_register((EseEntityComponentMap *)component->data, false);

    return component;
}
