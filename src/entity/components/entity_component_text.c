#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "entity/entity_private.h"
#include "entity/entity.h"
#include "types/point.h"
#include "graphics/sprite.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_text.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"


// Font constants (matching console font)
#define FONT_CHAR_WIDTH 10
#define FONT_CHAR_HEIGHT 20
#define FONT_SPACING 1

static void _entity_component_text_register(EseEntityComponentText *component, bool is_lua_owned) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_register called with NULL component");
    log_assert("ENTITY_COMP", component->base.lua_ref == LUA_NOREF, "_entity_component_text_register component is already registered");

    lua_newtable(component->base.lua->runtime);
    lua_pushlightuserdata(component->base.lua->runtime, component);
    lua_setfield(component->base.lua->runtime, -2, "__ptr");

    // Store the ownership flag
    lua_pushboolean(component->base.lua->runtime, is_lua_owned);
    lua_setfield(component->base.lua->runtime, -2, "__is_lua_owned");

    luaL_getmetatable(component->base.lua->runtime, TEXT_PROXY_META);
    lua_setmetatable(component->base.lua->runtime, -2);

    // Store a reference to this proxy table in the Lua registry
    component->base.lua_ref = luaL_ref(component->base.lua->runtime, LUA_REGISTRYINDEX);
}

static EseEntityComponent *_entity_component_text_make(EseLuaEngine *engine, const char *text) {
    EseEntityComponentText *component = memory_manager.malloc(sizeof(EseEntityComponentText), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.type = ENTITY_COMPONENT_TEXT;
    
    log_debug("ENTITY_COMP", "Created text component: text='%s', type=%d, active=%s", 
              text ? text : "NULL", component->base.type, component->base.active ? "true" : "false");

    // Initialize text
    if (text != NULL) {
        component->text = memory_manager.strdup(text, MMTAG_ENTITY);
    } else {
        component->text = memory_manager.strdup("", MMTAG_ENTITY);
    }

    // Initialize with default values
    component->justify = TEXT_JUSTIFY_LEFT;
    component->align = TEXT_ALIGN_LEFT;
    component->offset = point_create(engine);

    return &component->base;
}

EseEntityComponent *_entity_component_text_copy(const EseEntityComponentText *src) {
    log_assert("ENTITY_COMP", src, "_entity_component_text_copy called with NULL src");

    EseEntityComponent *copy = _entity_component_text_make(src->base.lua, src->text);
    EseEntityComponentText *text_copy = (EseEntityComponentText *)copy->data;
    
    // Copy properties
    text_copy->justify = src->justify;
    text_copy->align = src->align;
    
    // Copy offset
    text_copy->offset->x = src->offset->x;
    text_copy->offset->y = src->offset->y;

    return copy;
}

void _entity_component_text_destroy(EseEntityComponentText *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_destroy called with NULL src");

    memory_manager.free(component->text);
    point_destroy(component->offset);
    uuid_destroy(component->base.id);
    memory_manager.free(component);
}

void _entity_component_text_update(EseEntityComponentText *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_text_update called with NULL entity");


    // Text components don't need update logic for now
    (void)delta_time;
}

static int _entity_component_text_index(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    if (!component) {
        return 0;
    }

    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }

    if (strcmp(key, "text") == 0) {
        lua_pushstring(L, component->text ? component->text : "");
        return 1;
    } else if (strcmp(key, "justify") == 0) {
        lua_pushinteger(L, (int)component->justify);
        return 1;
    } else if (strcmp(key, "align") == 0) {
        lua_pushinteger(L, (int)component->align);
        return 1;
    } else if (strcmp(key, "offset") == 0) {
        // Push the offset point to Lua
        lua_newtable(L);
        lua_pushlightuserdata(L, component->offset);
        lua_setfield(L, -2, "__ptr");
        lua_pushstring(L, "PointProxyMeta");
        lua_setfield(L, -2, "__name");
        luaL_getmetatable(L, "PointProxyMeta");
        lua_setmetatable(L, -2);
        return 1;
    }

    return 0;
}

static int _entity_component_text_newindex(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    if (!component) {
        return 0;
    }

    const char *key = lua_tostring(L, 2);
    if (!key) {
        return 0;
    }

    if (strcmp(key, "text") == 0) {
        const char *new_text = lua_tostring(L, 3);
        if (new_text) {
            memory_manager.free(component->text);
            component->text = memory_manager.strdup(new_text, MMTAG_ENTITY);
        }
    } else if (strcmp(key, "justify") == 0) {
        if (lua_isinteger(L, 3)) {
            int justify_val = (int)lua_tointeger(L, 3);
            if (justify_val >= 0 && justify_val <= 2) {
                component->justify = (EseTextJustify)justify_val;
            }
        }
    } else if (strcmp(key, "align") == 0) {
        if (lua_isinteger(L, 3)) {
            int align_val = (int)lua_tointeger(L, 3);
            if (align_val >= 0 && align_val <= 2) {
                component->align = (EseTextAlign)align_val;
            }
        }
    } else if (strcmp(key, "offset") == 0) {
        if (lua_istable(L, 3)) {
            // Try to get the point from the table
            lua_getfield(L, 3, "__ptr");
            if (lua_islightuserdata(L, -1)) {
                EsePoint *new_offset = (EsePoint *)lua_touserdata(L, -1);
                if (new_offset) {
                    component->offset->x = new_offset->x;
                    component->offset->y = new_offset->y;
                }
            }
            lua_pop(L, 1);
        }
    }

    return 0;
}

static int _entity_component_text_gc(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    if (!component) {
        return 0;
    }

    // Check if this is a Lua-owned component
    lua_getfield(L, 1, "__is_lua_owned");
    bool is_lua_owned = lua_toboolean(L, -1);
    lua_pop(L, 1);
        
    if (is_lua_owned) {
        _entity_component_text_destroy(component);
        log_debug("LUA_GC", "EntityComponentText object (Lua-owned) garbage collected and C memory freed.");
    } else {
        log_debug("LUA_GC", "EntityComponentText object (C-owned) garbage collected, C memory *not* freed.");
    }
    
    return 0;
}

static int _entity_component_text_tostring(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    
    if (!component) {
        lua_pushstring(L, "EntityComponentText: (invalid)");
        return 1;
    }
    
    char buf[256];
    snprintf(buf, sizeof(buf), "EntityComponentText: %p (id=%s active=%s text='%s' justify=%d align=%d)", 
             (void*)component,
             component->base.id->value,
             component->base.active ? "true" : "false",
             component->text ? component->text : "",
             (int)component->justify,
             (int)component->align);
    lua_pushstring(L, buf);
    
    return 1;
}

static int _entity_component_text_new(lua_State *L) {
    const char *text = NULL;

    int n_args = lua_gettop(L);
    if (n_args == 1 && lua_isstring(L, 1)) {
        text = lua_tostring(L, 1);
    } else if (n_args == 1 && !lua_isstring(L, 1)) {
        log_debug("ENTITY_COMP", "Text must be a string, ignored");
    } else if (n_args != 0) {
        log_debug("ENTITY_COMP", "EntityComponentText.new() or EseEntityComponentText.new(String)");
    }

    // Set engine reference
    EseLuaEngine *lua = (EseLuaEngine *)lua_engine_get_registry_key(L, LUA_ENGINE_KEY);

    // Create EseEntityComponent wrapper
    EseEntityComponent *component = _entity_component_text_make(lua, text);

    // Push EseEntityComponent to Lua
    _entity_component_text_register((EseEntityComponentText *)component->data, true);
    entity_component_push(component);
    
    return 1;
}

EseEntityComponentText *_entity_component_text_get(lua_State *L, int idx) {
    if (!lua_istable(L, idx)) {
        return NULL;
    }

    lua_getfield(L, idx, "__ptr");
    if (!lua_islightuserdata(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }

    EseEntityComponentText *component = (EseEntityComponentText *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!component || component->base.type != ENTITY_COMPONENT_TEXT) {
        return NULL;
    }

    return component;
}

void _entity_component_text_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_text_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EntityComponentText metatable
    if (luaL_newmetatable(L, TEXT_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentTextProxyMeta to engine");
        lua_pushcfunction(L, _entity_component_text_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_text_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_text_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_text_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
    
    // Create global EntityComponentText table with constructor
    lua_getglobal(L, "EntityComponentText");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        log_debug("LUA", "Creating global EntityComponentText table");
        lua_newtable(L);
        lua_pushcfunction(L, _entity_component_text_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "EntityComponentText");
    } else {
        lua_pop(L, 1);
    }
}

void _entity_component_text_draw(EseEntityComponentText *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_draw called with NULL component");

    if (!component->text || strlen(component->text) == 0) {
        return;
    }

    // Calculate text dimensions
    int text_width = strlen(component->text) * (FONT_CHAR_WIDTH + FONT_SPACING) - FONT_SPACING;
    int text_height = FONT_CHAR_HEIGHT;

    // Apply offset
    float final_x = screen_x + component->offset->x;
    float final_y = screen_y + component->offset->y;

    // Apply justification (horizontal alignment)
    switch (component->justify) {
        case TEXT_JUSTIFY_CENTER:
            final_x -= text_width / 2.0f;
            break;
        case TEXT_JUSTIFY_RIGHT:
            final_x -= text_width;
            break;
        case TEXT_JUSTIFY_LEFT:
        default:
            break;
    }

    // Apply alignment (vertical alignment)
    switch (component->align) {
        case TEXT_ALIGN_CENTER:
            final_y -= text_height / 2.0f;
            break;
        case TEXT_ALIGN_RIGHT:
            final_y -= text_height;
            break;
        case TEXT_ALIGN_LEFT:
        default:
            break;
    }

    // Draw each character
    float char_x = final_x;
    for (int i = 0; component->text[i]; i++) {
        char c = component->text[i];
        if (c >= 32 && c <= 126) { // Printable ASCII
            char sprite_name[64];
            snprintf(sprite_name, sizeof(sprite_name), "fonts:console_font_10x20_%03d", (int)c);
            
            // Get the sprite from asset manager
            EseEngine *game_engine = (EseEngine *)lua_engine_get_registry_key(component->base.lua->runtime, ENGINE_KEY);
            EseSprite *letter = engine_get_sprite(game_engine, sprite_name);
            
            if (letter) {
                const char *texture_id;
                float x1, y1, x2, y2;
                int w, h;
                
                sprite_get_frame(letter, 0, &texture_id, &x1, &y1, &x2, &y2, &w, &h);
                texCallback(
                    (int)char_x, (int)final_y, w, h,
                    component->base.entity->draw_order,
                    texture_id, x1, y1, x2, y2, w, h,
                    callback_user_data
                );
            }
        }
        char_x += FONT_CHAR_WIDTH + FONT_SPACING;
    }
}

EseEntityComponent *entity_component_text_create(EseLuaEngine *engine, const char *text) {
    log_assert("ENTITY_COMP", engine, "entity_component_text_create called with NULL engine");
    
    EseEntityComponent *component = _entity_component_text_make(engine, text);

    // Push EseEntityComponent to Lua
    _entity_component_text_register((EseEntityComponentText *)component->data, false);

    return component;
}
