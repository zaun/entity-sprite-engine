#include <string.h>
#include "utility/log.h"
#include "utility/profile.h"
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

// VTable wrapper functions
static EseEntityComponent* _text_vtable_copy(EseEntityComponent* component) {
    return _entity_component_text_copy((EseEntityComponentText*)component->data);
}

static void _text_vtable_destroy(EseEntityComponent* component) {
    _entity_component_text_destroy((EseEntityComponentText*)component->data);
}

static void _text_vtable_update(EseEntityComponent* component, EseEntity* entity, float delta_time) {
    _entity_component_text_update((EseEntityComponentText*)component->data, entity, delta_time);
}

static void _text_vtable_draw(EseEntityComponent* component, int screen_x, int screen_y, void* callbacks, void* user_data) {
    EntityDrawCallbacks* draw_callbacks = (EntityDrawCallbacks*)callbacks;
    _entity_component_text_draw((EseEntityComponentText*)component->data, screen_x, screen_y, draw_callbacks->draw_texture, user_data);
}

static bool _text_vtable_run_function(EseEntityComponent* component, EseEntity* entity, const char* func_name, int argc, void* argv[]) {
    // Text components don't support function execution
    return false;
}

static void _text_vtable_collides_component(EseEntityComponent* a, EseEntityComponent* b, EseArray *out_hits) {
    (void)a; (void)b; (void)out_hits;
}

static void _text_vtable_ref(EseEntityComponent* component) {
    EseEntityComponentText *text = (EseEntityComponentText*)component->data;
    log_assert("ENTITY_COMP", text, "text vtable ref called with NULL");
    if (text->base.lua_ref == LUA_NOREF) {
        EseEntityComponentText **ud = (EseEntityComponentText **)lua_newuserdata(text->base.lua->runtime, sizeof(EseEntityComponentText *));
        *ud = text;
        luaL_getmetatable(text->base.lua->runtime, ENTITY_COMPONENT_TEXT_PROXY_META);
        lua_setmetatable(text->base.lua->runtime, -2);
        text->base.lua_ref = luaL_ref(text->base.lua->runtime, LUA_REGISTRYINDEX);
        text->base.lua_ref_count = 1;
    } else {
        text->base.lua_ref_count++;
    }
}

static void _text_vtable_unref(EseEntityComponent* component) {
    EseEntityComponentText *text = (EseEntityComponentText*)component->data;
    if (!text) return;
    if (text->base.lua_ref != LUA_NOREF && text->base.lua_ref_count > 0) {
        text->base.lua_ref_count--;
        if (text->base.lua_ref_count == 0) {
            luaL_unref(text->base.lua->runtime, LUA_REGISTRYINDEX, text->base.lua_ref);
            text->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for text components
static const ComponentVTable text_vtable = {
    .copy = _text_vtable_copy,
    .destroy = _text_vtable_destroy,
    .update = _text_vtable_update,
    .draw = _text_vtable_draw,
    .run_function = _text_vtable_run_function,
    .collides = _text_vtable_collides_component,
    .ref = _text_vtable_ref,
    .unref = _text_vtable_unref
};

// Font constants (matching console font)
#define FONT_CHAR_WIDTH 10
#define FONT_CHAR_HEIGHT 20
#define FONT_SPACING 1

static EseEntityComponent *_entity_component_text_make(EseLuaEngine *engine, const char *text) {
    EseEntityComponentText *component = memory_manager.malloc(sizeof(EseEntityComponentText), MMTAG_ENTITY);
    component->base.data = component;
    component->base.active = true;
    component->base.id = ese_uuid_create(engine);
    component->base.lua = engine;
    component->base.lua_ref = LUA_NOREF;
    component->base.lua_ref_count = 0;
    component->base.type = ENTITY_COMPONENT_TEXT;
    component->base.vtable = &text_vtable;
    
    component->justify = TEXT_JUSTIFY_LEFT;
    component->align = TEXT_ALIGN_TOP;
    component->offset = ese_point_create(engine);
    ese_point_ref(component->offset);

    // Initialize text
    if (text != NULL) {
        component->text = memory_manager.strdup(text, MMTAG_ENTITY);
    } else {
        component->text = memory_manager.strdup("", MMTAG_ENTITY);
    }

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
    ese_point_set_x(text_copy->offset, ese_point_get_x(src->offset));
    ese_point_set_y(text_copy->offset, ese_point_get_y(src->offset));

    return copy;
}

void _entity_component_ese_text_cleanup(EseEntityComponentText *component)
{
    memory_manager.free(component->text);
    ese_point_unref(component->offset);
    ese_point_destroy(component->offset);
    ese_uuid_destroy(component->base.id);
    memory_manager.free(component);
    profile_count_add("entity_comp_text_destroy_count");
}

void _entity_component_text_destroy(EseEntityComponentText *component) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_destroy called with NULL src");

    // Unref the component to clean up Lua references
    if (component->base.lua_ref != LUA_NOREF && component->base.lua_ref_count > 0) {
        component->base.lua_ref_count--;
        if (component->base.lua_ref_count == 0) {
            luaL_unref(component->base.lua->runtime, LUA_REGISTRYINDEX, component->base.lua_ref);
            component->base.lua_ref = LUA_NOREF;
            _entity_component_ese_text_cleanup(component);
        } else {
            // We dont "own" the offset so dont free it
            return;
        }
    } else if (component->base.lua_ref == LUA_NOREF) {
        _entity_component_ese_text_cleanup(component);
    }
}

void _entity_component_text_update(EseEntityComponentText *component, EseEntity *entity, float delta_time) {
    log_assert("ENTITY_COMP", component, "_entity_component_text_update called with NULL component");
    log_assert("ENTITY_COMP", entity, "_entity_component_text_update called with NULL entity");


    // Text components don't need update logic for now
    (void)delta_time;
}

static int _entity_component_text_index(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Return nil for freed components
    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    
    if (!key) {
        return 0;
    }

    if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        lua_pushstring(L, ese_uuid_get_value(component->base.id));
        return 1;
    } else if (strcmp(key, "text") == 0) {
        lua_pushstring(L, component->text ? component->text : "");
        return 1;
    } else if (strcmp(key, "justify") == 0) {
        lua_pushinteger(L, (int)component->justify);
        return 1;
    } else if (strcmp(key, "align") == 0) {
        lua_pushinteger(L, (int)component->align);
        return 1;
    } else if (strcmp(key, "offset") == 0) {
        ese_point_lua_push(component->offset);
        return 1;
    }

    return 0;
}

static int _entity_component_text_newindex(lua_State *L) {
    EseEntityComponentText *component = _entity_component_text_get(L, 1);
    const char *key = lua_tostring(L, 2);
    
    // SAFETY: Silently ignore writes to freed components
    if (!component) {
        return 0;
    }
    
    if (!key) {
        return 0;
    }

    if (strcmp(key, "active") == 0) {
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "active must be a boolean");
        }
        component->base.active = lua_toboolean(L, 3);
        lua_pushboolean(L, component->base.active);
        return 1;
    } else if (strcmp(key, "id") == 0) {
        return luaL_error(L, "id is read-only");
    } else if (strcmp(key, "text") == 0) {
        const char *new_text = lua_tostring(L, 3);
        if (new_text) {
            memory_manager.free(component->text);
            component->text = memory_manager.strdup(new_text, MMTAG_ENTITY);
        }
        return 0;
    } else if (strcmp(key, "justify") == 0) {
        if (lua_isinteger_lj(L, 3)) {
            int justify_val = (int)lua_tointeger(L, 3);
            if (justify_val >= 0 && justify_val <= 2) {
                component->justify = (EseTextJustify)justify_val;
            }
        }
        return 0;
    } else if (strcmp(key, "align") == 0) {
        if (lua_isinteger_lj(L, 3)) {
            int align_val = (int)lua_tointeger(L, 3);
            if (align_val >= 0 && align_val <= 2) {
                component->align = (EseTextAlign)align_val;
            }
        }
        return 0;
    } else if (strcmp(key, "offset") == 0) {
        EsePoint *new_offset = ese_point_lua_get(L, 3);
        if (!new_offset) {
            return luaL_error(L, "offset must be a Point object");
        }
        ese_point_set_x(component->offset, ese_point_get_x(new_offset));
        ese_point_set_y(component->offset, ese_point_get_y(new_offset));
        return 0;
    }

    return luaL_error(L, "unknown or unassignable property '%s'", key);
}

static int _entity_component_text_gc(lua_State *L) {
    // Get from userdata
    EseEntityComponentText **ud = (EseEntityComponentText **)luaL_testudata(L, 1, ENTITY_COMPONENT_TEXT_PROXY_META);
    if (!ud) {
        return 0; // Not our userdata
    }
    
    EseEntityComponentText *component = *ud;
    if (component) {
        if (component->base.lua_ref == LUA_NOREF) {
            _entity_component_text_destroy(component);
            *ud = NULL;
        }
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
             ese_uuid_get_value(component->base.id),
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

    // For Lua-created components, create userdata without storing a persistent ref
    EseEntityComponentText **ud = (EseEntityComponentText **)lua_newuserdata(L, sizeof(EseEntityComponentText *));
    *ud = (EseEntityComponentText *)component->data;
    luaL_getmetatable(L, ENTITY_COMPONENT_TEXT_PROXY_META);
    lua_setmetatable(L, -2);

    return 1;
}

EseEntityComponentText *_entity_component_text_get(lua_State *L, int idx) {
    // Check if it's userdata
    if (!lua_isuserdata(L, idx)) {
        return NULL;
    }
    
    // Get the userdata and check metatable
    EseEntityComponentText **ud = (EseEntityComponentText **)luaL_testudata(L, idx, ENTITY_COMPONENT_TEXT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }
    
    return *ud;
}

void _entity_component_text_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_text_init called with NULL engine");

    lua_State *L = engine->runtime;
    
    // Register EntityComponentText metatable
    if (luaL_newmetatable(L, ENTITY_COMPONENT_TEXT_PROXY_META)) {
        log_debug("LUA", "Adding EntityComponentTextProxyMeta to engine");  
        lua_pushstring(L, ENTITY_COMPONENT_TEXT_PROXY_META);
        lua_setfield(L, -2, "__name");
        lua_pushcfunction(L, _entity_component_text_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _entity_component_text_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, _entity_component_text_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _entity_component_text_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
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
        
        // Add JUSTIFY constants
        lua_newtable(L);
        lua_pushinteger(L, TEXT_JUSTIFY_LEFT);
        lua_setfield(L, -2, "LEFT");
        lua_pushinteger(L, TEXT_JUSTIFY_CENTER);
        lua_setfield(L, -2, "CENTER");
        lua_pushinteger(L, TEXT_JUSTIFY_RIGHT);
        lua_setfield(L, -2, "RIGHT");
        lua_setfield(L, -2, "JUSTIFY");
        
        // Add ALIGN constants
        lua_newtable(L);
        lua_pushinteger(L, TEXT_ALIGN_TOP);
        lua_setfield(L, -2, "TOP");
        lua_pushinteger(L, TEXT_ALIGN_CENTER);
        lua_setfield(L, -2, "CENTER");
        lua_pushinteger(L, TEXT_ALIGN_BOTTOM);
        lua_setfield(L, -2, "BOTTOM");
        lua_setfield(L, -2, "ALIGN");
        
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
    float final_x = screen_x + ese_point_get_x(component->offset);
    float final_y = screen_y + ese_point_get_y(component->offset);

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
        case TEXT_ALIGN_BOTTOM:
            final_y -= text_height;
            break;
        case TEXT_ALIGN_TOP:
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

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}
