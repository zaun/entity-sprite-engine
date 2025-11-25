#include "entity/components/entity_component_text.h"
#include "core/asset_manager.h"
#include "core/engine.h"
#include "core/memory_manager.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/font.h"
#include "graphics/sprite.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/json/cJSON.h"
#include <string.h>

// Forward declarations
static int _entity_component_text_tojson_lua(lua_State *L);

// VTable wrapper functions
static EseEntityComponent *_text_vtable_copy(EseEntityComponent *component) {
    return _entity_component_text_copy((EseEntityComponentText *)component->data);
}

static void _text_vtable_destroy(EseEntityComponent *component) {
    _entity_component_text_destroy((EseEntityComponentText *)component->data);
}

static bool _text_vtable_run_function(EseEntityComponent *component, EseEntity *entity,
                                      const char *func_name, int argc, void *argv[]) {
    // Text components don't support function execution
    return false;
}

static void _text_vtable_collides_component(EseEntityComponent *a, EseEntityComponent *b,
                                            EseArray *out_hits) {
    (void)a;
    (void)b;
    (void)out_hits;
}

static void _text_vtable_ref(EseEntityComponent *component) {
    EseEntityComponentText *text = (EseEntityComponentText *)component->data;
    log_assert("ENTITY_COMP", text, "text vtable ref called with NULL");
    if (text->base.lua_ref == LUA_NOREF) {
        EseEntityComponentText **ud = (EseEntityComponentText **)lua_newuserdata(
            text->base.lua->runtime, sizeof(EseEntityComponentText *));
        *ud = text;
        luaL_getmetatable(text->base.lua->runtime, ENTITY_COMPONENT_TEXT_PROXY_META);
        lua_setmetatable(text->base.lua->runtime, -2);
        text->base.lua_ref = luaL_ref(text->base.lua->runtime, LUA_REGISTRYINDEX);
        text->base.lua_ref_count = 1;
    } else {
        text->base.lua_ref_count++;
    }
}

static void _text_vtable_unref(EseEntityComponent *component) {
    EseEntityComponentText *text = (EseEntityComponentText *)component->data;
    if (!text)
        return;
    if (text->base.lua_ref != LUA_NOREF && text->base.lua_ref_count > 0) {
        text->base.lua_ref_count--;
        if (text->base.lua_ref_count == 0) {
            luaL_unref(text->base.lua->runtime, LUA_REGISTRYINDEX, text->base.lua_ref);
            text->base.lua_ref = LUA_NOREF;
        }
    }
}

// Static vtable instance for text components
static const ComponentVTable text_vtable = {.copy = _text_vtable_copy,
                                            .destroy = _text_vtable_destroy,
                                            .run_function = _text_vtable_run_function,
                                            .collides = _text_vtable_collides_component,
                                            .ref = _text_vtable_ref,
                                            .unref = _text_vtable_unref};

// Font constants (matching console font)
#define FONT_CHAR_WIDTH 10
#define FONT_CHAR_HEIGHT 20
#define FONT_SPACING 1

static EseEntityComponent *_entity_component_text_make(EseLuaEngine *engine, const char *text) {
    EseEntityComponentText *component =
        memory_manager.malloc(sizeof(EseEntityComponentText), MMTAG_ENTITY);
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

void _entity_component_ese_text_cleanup(EseEntityComponentText *component) {
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

cJSON *entity_component_text_serialize(const EseEntityComponentText *component) {
    log_assert("ENTITY_COMP", component,
               "entity_component_text_serialize called with NULL component");

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        log_error("ENTITY_COMP", "Text serialize: failed to create JSON object");
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "type", "ENTITY_COMPONENT_TEXT")) {
        log_error("ENTITY_COMP", "Text serialize: failed to add type");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_AddBoolToObject(json, "active", component->base.active)) {
        log_error("ENTITY_COMP", "Text serialize: failed to add active");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_AddStringToObject(json, "text", component->text ? component->text : "")) {
        log_error("ENTITY_COMP", "Text serialize: failed to add text");
        cJSON_Delete(json);
        return NULL;
    }

    if (!cJSON_AddNumberToObject(json, "justify", (double)component->justify) ||
        !cJSON_AddNumberToObject(json, "align", (double)component->align)) {
        log_error("ENTITY_COMP", "Text serialize: failed to add justify/align");
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *offset = cJSON_CreateObject();
    if (!offset) {
        log_error("ENTITY_COMP", "Text serialize: failed to create offset object");
        cJSON_Delete(json);
        return NULL;
    }
    if (!cJSON_AddNumberToObject(offset, "x", (double)ese_point_get_x(component->offset)) ||
        !cJSON_AddNumberToObject(offset, "y", (double)ese_point_get_y(component->offset)) ||
        !cJSON_AddItemToObject(json, "offset", offset)) {
        log_error("ENTITY_COMP", "Text serialize: failed to add offset");
        cJSON_Delete(offset);
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

EseEntityComponent *entity_component_text_deserialize(EseLuaEngine *engine,
                                                      const cJSON *data) {
    log_assert("ENTITY_COMP", engine,
               "entity_component_text_deserialize called with NULL engine");
    log_assert("ENTITY_COMP", data, "entity_component_text_deserialize called with NULL data");

    if (!cJSON_IsObject(data)) {
        log_error("ENTITY_COMP", "Text deserialize: data is not an object");
        return NULL;
    }

    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type_item) || strcmp(type_item->valuestring, "ENTITY_COMPONENT_TEXT") != 0) {
        log_error("ENTITY_COMP", "Text deserialize: invalid or missing type");
        return NULL;
    }

    const cJSON *active_item = cJSON_GetObjectItemCaseSensitive(data, "active");
    if (!cJSON_IsBool(active_item)) {
        log_error("ENTITY_COMP", "Text deserialize: missing active field");
        return NULL;
    }

    const cJSON *text_item = cJSON_GetObjectItemCaseSensitive(data, "text");
    const char *text_str = cJSON_IsString(text_item) ? text_item->valuestring : "";

    const cJSON *justify_item = cJSON_GetObjectItemCaseSensitive(data, "justify");
    const cJSON *align_item = cJSON_GetObjectItemCaseSensitive(data, "align");

    const cJSON *offset_item = cJSON_GetObjectItemCaseSensitive(data, "offset");
    const cJSON *off_x = offset_item ? cJSON_GetObjectItemCaseSensitive(offset_item, "x") : NULL;
    const cJSON *off_y = offset_item ? cJSON_GetObjectItemCaseSensitive(offset_item, "y") : NULL;

    EseEntityComponent *base = entity_component_text_create(engine, text_str);
    if (!base) {
        log_error("ENTITY_COMP", "Text deserialize: failed to create component");
        return NULL;
    }

    EseEntityComponentText *comp = (EseEntityComponentText *)base->data;
    comp->base.active = cJSON_IsTrue(active_item);

    if (cJSON_IsNumber(justify_item)) {
        int j = (int)justify_item->valuedouble;
        if (j >= TEXT_JUSTIFY_LEFT && j <= TEXT_JUSTIFY_RIGHT) {
            comp->justify = (EseTextJustify)j;
        }
    }
    if (cJSON_IsNumber(align_item)) {
        int a = (int)align_item->valuedouble;
        if (a >= TEXT_ALIGN_TOP && a <= TEXT_ALIGN_BOTTOM) {
            comp->align = (EseTextAlign)a;
        }
    }

    if (off_x && cJSON_IsNumber(off_x) && off_y && cJSON_IsNumber(off_y)) {
        ese_point_set_x(comp->offset, (float)off_x->valuedouble);
        ese_point_set_y(comp->offset, (float)off_y->valuedouble);
    }

    return base;
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
    } else if (strcmp(key, "toJSON") == 0) {
        lua_pushcfunction(L, _entity_component_text_tojson_lua);
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
    EseEntityComponentText **ud =
        (EseEntityComponentText **)luaL_testudata(L, 1, ENTITY_COMPONENT_TEXT_PROXY_META);
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
    snprintf(buf, sizeof(buf),
             "EntityComponentText: %p (id=%s active=%s text='%s' justify=%d "
             "align=%d)",
             (void *)component, ese_uuid_get_value(component->base.id),
             component->base.active ? "true" : "false", component->text ? component->text : "",
             (int)component->justify, (int)component->align);
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

    // For Lua-created components, create userdata without storing a persistent
    // ref
    EseEntityComponentText **ud =
        (EseEntityComponentText **)lua_newuserdata(L, sizeof(EseEntityComponentText *));
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
    EseEntityComponentText **ud =
        (EseEntityComponentText **)luaL_testudata(L, idx, ENTITY_COMPONENT_TEXT_PROXY_META);
    if (!ud) {
        return NULL; // Wrong metatable or not userdata
    }

    return *ud;
}

void _entity_component_text_init(EseLuaEngine *engine) {
    log_assert("ENTITY_COMP", engine, "_entity_component_text_init called with NULL engine");

    // Create metatable
    lua_engine_new_object_meta(engine, ENTITY_COMPONENT_TEXT_PROXY_META,
                               _entity_component_text_index, _entity_component_text_newindex,
                               _entity_component_text_gc, _entity_component_text_tostring);

    // Create global EntityComponentText table with functions and constants
    lua_State *L = engine->runtime;
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

EseEntityComponent *entity_component_text_create(EseLuaEngine *engine, const char *text) {
    log_assert("ENTITY_COMP", engine, "entity_component_text_create called with NULL engine");

    EseEntityComponent *component = _entity_component_text_make(engine, text);

    // Register with Lua using ref system
    component->vtable->ref(component);

    return component;
}

static int _entity_component_text_tojson_lua(lua_State *L) {
    EseEntityComponentText *self = _entity_component_text_get(L, 1);
    if (!self) {
        return luaL_error(L, "EntityComponentText:toJSON() called on invalid component");
    }
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "EntityComponentText:toJSON() takes 0 arguments");
    }
    cJSON *json = entity_component_text_serialize(self);
    if (!json) {
        return luaL_error(L, "EntityComponentText:toJSON() failed to serialize");
    }
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        return luaL_error(L, "EntityComponentText:toJSON() failed to stringify");
    }
    lua_pushstring(L, json_str);
    free(json_str);
    return 1;
}
