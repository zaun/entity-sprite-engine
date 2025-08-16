#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utility/log.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_engine.h"


/**
 * @brief Resets a EseLuaValue structure by memory_manager.freeing allocated memory and clearing data.
 * 
 * @details memory_manager.frees string memory for STRING types, recursively memory_manager.frees all table
 *          items for TABLE types including the items array itself, optionally
 *          preserves or memory_manager.frees the name field, resets all counters and pointers,
 *          and sets type back to NIL for reuse.
 * 
 * @param val Pointer to EseLuaValue to reset. Safe to pass NULL.
 * @param keep_name If true, preserves the name field; if false, memory_manager.frees it.
 * 
 * @warning Recursively memory_manager.frees table contents; all references to contained items become invalid.
 * @warning After reset, only the name field (if kept) and structure itself remain valid.
 */
static void _lua_value_reset(EseLuaValue *val, bool keep_name) {
    log_assert("LUA", val, "_lua_value_reset called with NULL val");

    if (val->type == LUA_VAL_USERDATA) {
        val->value.userdata = NULL;
    }

    if (val->type == LUA_VAL_STRING && val->value.string) {
        memory_manager.free(val->value.string);
        val->value.string = NULL;
    }

    if (val->type == LUA_VAL_TABLE && val->value.table.items) {
        for (size_t i = 0; i < val->value.table.count; ++i) {
            lua_value_free(val->value.table.items[i]);
        }
        memory_manager.free(val->value.table.items);
        val->value.table.items = NULL;
        val->value.table.count = 0;
        val->value.table.capacity = 0;
    }

    if (val->name && !keep_name) {
        memory_manager.free(val->name);
        val->name = NULL;
    }
    val->type = LUA_VAL_NIL;
}

EseLuaValue* lua_value_copy(const EseLuaValue *src) {
    log_assert("LUA", src, "_lua_value_reset called with NULL src");
    
    EseLuaValue *copy = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    copy->type = src->type;
    copy->name = src->name ? memory_manager.strdup(src->name, MMTAG_LUA) : NULL;
    
    switch (src->type) {
        case LUA_VAL_STRING:
            copy->value.string = src->value.string ? memory_manager.strdup(src->value.string, MMTAG_LUA) : NULL;
            break;
        case LUA_VAL_TABLE:
            if (src->value.table.count > 0) {
                copy->value.table.items = memory_manager.malloc(src->value.table.count * sizeof(EseLuaValue*), MMTAG_LUA);
                copy->value.table.count = src->value.table.count;
                for (size_t i = 0; i < src->value.table.count; i++) {
                    copy->value.table.items[i] = lua_value_copy(src->value.table.items[i]);
                }
            }
            break;
        default:
            copy->value = src->value;
            break;
    }
    return copy;
}

EseLuaValue *lua_value_create_nil(const char *name) {
    log_assert("LUA", name, "lua_value_create_nil called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_NIL;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_bool(const char *name, bool value) {
    log_assert("LUA", name, "lua_value_create_bool called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_BOOL;
    v->value.boolean = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_number(const char *name, double value) {
    log_assert("LUA", name, "lua_value_create_number called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_NUMBER;
    v->value.number = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_string(const char *name, const char *value) {
    log_assert("LUA", name, "lua_value_create_string called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_STRING;
    v->value.string = memory_manager.strdup(value, MMTAG_LUA);
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_table(const char *name) {
    log_assert("LUA", name, "lua_value_create_table called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_TABLE;
    v->value.table.items = NULL;
    v->value.table.count = 0;
    v->value.table.capacity = 0;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_ref(const char *name, int value) {
    log_assert("LUA", name, "lua_value_create_number called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_REF;
    v->value.lua_ref = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

EseLuaValue *lua_value_create_userdata(const char *name, void* value) {
    log_assert("LUA", name, "lua_value_create_number called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA);
    v->type = LUA_VAL_USERDATA;
    v->value.userdata = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA);
    return v;
}

void lua_value_push(EseLuaValue *val, EseLuaValue *item, bool copy) {
    log_assert("LUA", val, "lua_value_push called with NULL val");
    log_assert("LUA", item, "lua_value_push called with NULL item");

    if (val->type != LUA_VAL_TABLE) {
        log_error("LUA_ENGINE", "lua_value_push item is not a table");
        return;
    }

    // Grow array if needed
    if (val->value.table.count >= val->value.table.capacity) {
        size_t new_capacity = val->value.table.capacity == 0 ? 4 : val->value.table.capacity * 2;
        EseLuaValue **new_items = memory_manager.realloc(val->value.table.items, new_capacity * sizeof(EseLuaValue*), MMTAG_LUA);  // ← Array of pointers
        if (!new_items) return;
        val->value.table.items = new_items;
        val->value.table.capacity = new_capacity;
    }

    if (copy) {
        // Deep copy the item
        val->value.table.items[val->value.table.count] = lua_value_copy(item);
    } else {
        // Take ownership of the pointer
        val->value.table.items[val->value.table.count] = item;  // ← Store the POINTER
    }
    val->value.table.count++;
}

void lua_value_set_nil(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_set_nil called with NULL val");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_NIL;
}

void lua_value_set_bool(EseLuaValue *val, bool value) {
    log_assert("LUA", val, "lua_value_set_bool called with NULL val");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_BOOL;
    val->value.boolean = value;
}

void lua_value_set_number(EseLuaValue *val, double value) {
    log_assert("LUA", val, "lua_value_set_number called with NULL val");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_NUMBER;
    val->value.number = value;
}

void lua_value_set_string(EseLuaValue *val, const char *value) {
    log_assert("LUA", val, "lua_value_set_string called with NULL val");
    log_assert("LUA", value, "lua_value_set_string called with NULL value");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_STRING;
    val->value.string = memory_manager.strdup(value, MMTAG_LUA);
}

void lua_value_set_table(EseLuaValue *val) {
    if (!val) return;

    _lua_value_reset(val, true);

    val->type = LUA_VAL_TABLE;
}

void lua_value_set_ref(EseLuaValue *val, int value) {
    log_assert("LUA", val, "lua_value_set_ref called with NULL val");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_REF;
    val->value.lua_ref = value;
}

void lua_value_set_userdata(EseLuaValue *val, void* value) {
    log_assert("LUA", val, "lua_value_set_ref called with NULL val");

    _lua_value_reset(val, true);

    val->type = LUA_VAL_USERDATA;
    val->value.userdata = value;
}

EseLuaValue *lua_value_get_table_prop(EseLuaValue *val, const char *prop_name) {
    log_assert("LUA", val, "lua_value_get_table_prop called with NULL val");

    if (val->type != LUA_VAL_TABLE || !prop_name) return NULL;

    for (size_t i = 0; i < val->value.table.count; ++i) {
        EseLuaValue *item = val->value.table.items[i];
        if (item->name && strcmp(item->name, prop_name) == 0) {
            return item;
        }
    }
    return NULL;
}

const char *lua_value_get_name(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_get_name called with NULL val");

    return val->name;
}

bool lua_value_get_bool(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_get_bool called with NULL val");

    return val->value.boolean;
}

float lua_value_get_number(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_get_number called with NULL val");

    return (float)val->value.number;
}

const char *lua_value_get_string(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_get_string called with NULL val");

    return val->value.string;
}

void *lua_value_get_userdata(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_get_userdata called with NULL val");

    return val->value.userdata;
}

void lua_value_free(EseLuaValue *val) {
    log_assert("LUA", val, "lua_value_free called with NULL val");

    _lua_value_reset(val, false);
    memory_manager.free(val);
}


// This probably doesnt belong here but I'm trying
// to keep all the EseLuaValue stuff in this
// one file...


// Adjust as needed for your environment
#define LOG_LUAVALUE_MAXLEN 4096

static void _log_luavalue_rec(
    EseLuaValue *val,
    char *buf,
    size_t buflen,
    size_t *offset,
    int indent
) {
    if (!val || !buf || !offset) return;

    // Indentation
    for (int i = 0; i < indent; ++i) {
        if (*offset < buflen - 1) buf[(*offset)++] = ' ';
        if (*offset < buflen - 1) buf[(*offset)++] = ' ';
    }

    // Name (if present)
    if (val->name && val->name[0]) {
        int n = snprintf(buf + *offset, buflen - *offset, "%s: ", val->name);
        *offset += (n > 0) ? n : 0;
    }

    switch (val->type) {
        case LUA_VAL_NIL:
            *offset += snprintf(buf + *offset, buflen - *offset, "nil\n");
            break;
        case LUA_VAL_BOOL:
            *offset += snprintf(buf + *offset, buflen - *offset, "%s\n", val->value.boolean ? "true" : "false");
            break;
        case LUA_VAL_NUMBER:
            *offset += snprintf(buf + *offset, buflen - *offset, "Number: %g\n", val->value.number);
            break;
        case LUA_VAL_STRING:
            *offset += snprintf(buf + *offset, buflen - *offset, "String: %s\n", val->value.string ? val->value.string : "");
            break;
        case LUA_VAL_TABLE: {
            *offset += snprintf(buf + *offset, buflen - *offset, "Table:\n");
            for (size_t i = 0; i < val->value.table.count; ++i) {
                _log_luavalue_rec(val->value.table.items[i], buf, buflen, offset, indent + 2);
            }
            break;
        }
        default:
            *offset += snprintf(buf + *offset, buflen - *offset, "Unknown\n");
            break;
    }
}

void log_luavalue(EseLuaValue *val) {
    if (!val) {
        log_debug("LUA", "log_luavalue: (null)");
        return;
    }
    char buf[LOG_LUAVALUE_MAXLEN];
    size_t offset = 0;
    buf[0] = '\0';

    _log_luavalue_rec(val, buf, sizeof(buf), &offset, 0);

    // Ensure null-termination
    buf[sizeof(buf) - 1] = '\0';

    log_debug("LUA", "\n%s", buf);
}
