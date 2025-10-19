#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utility/log.h"
#include "utility/profile.h"
#include "vendor/lua/src/lua.h"
#include "vendor/lua/src/lauxlib.h"
#include "vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_engine.h"
#include "types/rect.h"
#include "types/point.h"
#include "types/map.h"
#include "types/arc.h"
#include "types/camera.h"
#include "types/color.h"
#include "types/display.h"
#include "types/input_state.h"
#include "types/map_cell.h"
#include "types/poly_line.h"
#include "types/ray.h"
#include "types/tileset.h"
#include "types/uuid.h"
#include "types/vector.h"
#include "types/collision_hit.h"


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
    log_assert("LUA_VALUE", val, "_lua_value_reset called with NULL val");

    // PROFILING: Start timing for reset
    profile_start(PROFILE_LUA_VALUE_RESET_OVERALL);

    if (val->type == LUA_VAL_USERDATA) {
        val->value.userdata = NULL;
    }

    if (val->type == LUA_VAL_RECT) {
        val->value.rect = NULL;
    }

    if (val->type == LUA_VAL_MAP) {
        val->value.map = NULL;
    }

    if (val->type == LUA_VAL_ARC) {
        val->value.arc = NULL;
    }

    if (val->type == LUA_VAL_COLOR) {
        val->value.color = NULL;
    }

    if (val->type == LUA_VAL_DISPLAY) {
        val->value.display = NULL;
    }

    if (val->type == LUA_VAL_INPUT_STATE) {
        val->value.input_state = NULL;
    }

    if (val->type == LUA_VAL_MAP_CELL) {
        val->value.map_cell = NULL;
    }

    if (val->type == LUA_VAL_POLY_LINE) {
        val->value.poly_line = NULL;
    }

    if (val->type == LUA_VAL_RAY) {
        val->value.ray = NULL;
    }

    if (val->type == LUA_VAL_TILESET) {
        val->value.tileset = NULL;
    }

    if (val->type == LUA_VAL_UUID) {
        val->value.uuid = NULL;
    }

    if (val->type == LUA_VAL_VECTOR) {
        val->value.vector = NULL;
    }

    if (val->type == LUA_VAL_COLLISION_HIT) {
        val->value.collision_hit = NULL;
    }

    if (val->type == LUA_VAL_CFUNC) {
        val->value.cfunc_data.cfunc = NULL;
        val->value.cfunc_data.upvalue = NULL;
    }

    if (val->type == LUA_VAL_ERROR) {
        val->value.string = NULL;
    }

    if (val->type == LUA_VAL_STRING && val->value.string) {
        profile_start(PROFILE_LUA_VALUE_RESET_SECTION);
        memory_manager.free(val->value.string);
        profile_stop(PROFILE_LUA_VALUE_RESET_SECTION, "lua_value_reset_string_free");
        val->value.string = NULL;
    }

    if (val->type == LUA_VAL_TABLE && val->value.table.items) {
        profile_start(PROFILE_LUA_VALUE_RESET_SECTION);
        for (size_t i = 0; i < val->value.table.count; ++i) {
            lua_value_destroy(val->value.table.items[i]);
        }
        memory_manager.free(val->value.table.items);
        profile_stop(PROFILE_LUA_VALUE_RESET_SECTION, "lua_value_reset_table_free");
        val->value.table.items = NULL;
        val->value.table.count = 0;
        val->value.table.capacity = 0;
    }

    if (val->name && !keep_name) {
        profile_start(PROFILE_LUA_VALUE_RESET_SECTION);
        memory_manager.free(val->name);
        profile_stop(PROFILE_LUA_VALUE_RESET_SECTION, "lua_value_reset_name_free");
        val->name = NULL;
    }
    val->type = LUA_VAL_NIL;

    // PROFILING: Stop overall reset timing
    profile_stop(PROFILE_LUA_VALUE_RESET_OVERALL, "lua_value_reset_overall");
}

EseLuaValue* lua_value_copy(const EseLuaValue *src) {
    log_assert("LUA_VALUE", src, "_lua_value_reset called with NULL src");
    
    EseLuaValue *copy = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    copy->type = src->type;
    copy->name = src->name ? memory_manager.strdup(src->name, MMTAG_LUA_VALUE) : NULL;
    
    switch (src->type) {
        case LUA_VAL_ERROR:
        case LUA_VAL_STRING:
            copy->value.string = src->value.string ? memory_manager.strdup(src->value.string, MMTAG_LUA_VALUE) : NULL;
            break;
        case LUA_VAL_TABLE:
            if (src->value.table.count > 0) {
                copy->value.table.items = memory_manager.malloc(src->value.table.count * sizeof(EseLuaValue*), MMTAG_LUA_VALUE);
                copy->value.table.count = src->value.table.count;
                copy->value.table.capacity = src->value.table.count; // Initialize capacity
                
                for (size_t i = 0; i < src->value.table.count; i++) {
                    if (src->value.table.items[i]) {
                        copy->value.table.items[i] = lua_value_copy(src->value.table.items[i]);
                        if (!copy->value.table.items[i]) {
                            // If copying an item fails, we need to clean up
                            for (size_t j = 0; j < i; j++) {
                                if (copy->value.table.items[j]) {
                                    lua_value_destroy(copy->value.table.items[j]);
                                }
                            }
                            memory_manager.free(copy->value.table.items);
                            copy->value.table.items = NULL;
                            copy->value.table.count = 0;
                            copy->value.table.capacity = 0;
                            break;
                        }
                    } else {
                        copy->value.table.items[i] = NULL;
                    }
                }
            } else {
                copy->value.table.items = NULL;
                copy->value.table.count = 0;
                copy->value.table.capacity = 0;
            }
            break;
        case LUA_VAL_RECT:
            copy->value.rect = ese_rect_copy(src->value.rect);
            break;
        case LUA_VAL_POINT:
            copy->value.point = ese_point_copy(src->value.point);
            break;
        case LUA_VAL_MAP:
            copy->value.map = src->value.map;  // FIXME - no deep copy function available
            break;
        case LUA_VAL_ARC:
            copy->value.arc = ese_arc_copy(src->value.arc);
            break;
        case LUA_VAL_COLOR:
            copy->value.color = ese_color_copy(src->value.color);
            break;
        case LUA_VAL_DISPLAY:
            copy->value.display = ese_display_copy(src->value.display);
            break;
        case LUA_VAL_INPUT_STATE:
            copy->value.input_state = ese_input_state_copy(src->value.input_state);
            break;
        case LUA_VAL_MAP_CELL:
            copy->value.map_cell = ese_map_cell_copy(src->value.map_cell);
            break;
        case LUA_VAL_POLY_LINE:
            copy->value.poly_line = ese_poly_line_copy(src->value.poly_line);
            break;
        case LUA_VAL_RAY:
            copy->value.ray = ese_ray_copy(src->value.ray);
            break;
        case LUA_VAL_TILESET:
            copy->value.tileset = ese_tileset_copy(src->value.tileset);
            break;
        case LUA_VAL_UUID:
            copy->value.uuid = ese_uuid_copy(src->value.uuid);
            break;
        case LUA_VAL_VECTOR:
            copy->value.vector = ese_vector_copy(src->value.vector);
            break;
        case LUA_VAL_COLLISION_HIT:
            copy->value.collision_hit = ese_collision_hit_copy(src->value.collision_hit);
            break;
        case LUA_VAL_CFUNC:
            copy->value.cfunc_data.cfunc = src->value.cfunc_data.cfunc;  // Function pointers are just copied
            copy->value.cfunc_data.upvalue = src->value.cfunc_data.upvalue;  // Upvalue is just copied (shallow)
            break;
        default:
            copy->value = src->value;
            break;
    }

    return copy;
}

EseLuaValue *lua_value_from_stack(lua_State *L, int index) {
    log_assert("LUA_VALUE", L, "lua_value_from_stack called with NULL L");

    EseLuaValue *result = lua_value_create_nil(NULL);
    int arg_type = lua_type(L, index);
    switch (arg_type) {
        case LUA_TNUMBER:
            lua_value_set_number(result, lua_tonumber(L, index));
            break;
        case LUA_TBOOLEAN:
            lua_value_set_bool(result, lua_toboolean(L, index));
            break;
        case LUA_TSTRING: {
            const char *str = lua_tostring(L, index);
            lua_value_set_string(result, str);
            break;
        }
        case LUA_TUSERDATA: {
            void *udata = lua_touserdata(L, index);
            lua_value_set_userdata(result, udata);
            break;
        }
        case LUA_TTABLE: {
            // Initialize result as a table and fill it directly to avoid shallow-copy bugs
            lua_value_set_table(result);
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                const char *key_str = NULL;
                int key_type = lua_type(L, -2);

                if (key_type == LUA_TSTRING) {
                    key_str = lua_tostring(L, -2);
                } else if (key_type == LUA_TNUMBER) {
                    char num_str[64];
                    snprintf(num_str, sizeof(num_str), "%g", lua_tonumber(L, -2));
                    key_str = num_str;
                }

                EseLuaValue *item_ptr = lua_value_from_stack(L, -1);
                // Transfer ownership of the new item to the result table
                lua_value_push(result, item_ptr, false);
                lua_pop(L, 1);
            }
            break;
        }
        default:
            lua_value_set_nil(result);
            break;
    }

    return result;
}

EseLuaValue *lua_value_create_nil(const char *name) {
    log_assert("LUA_VALUE", name, "lua_value_create_nil called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_NIL;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_bool(const char *name, bool value) {
    log_assert("LUA_VALUE", name, "lua_value_create_bool called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_BOOL;
    v->value.boolean = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_number(const char *name, double value) {
    log_assert("LUA_VALUE", name, "lua_value_create_number called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_NUMBER;
    v->value.number = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_string(const char *name, const char *value) {
    log_assert("LUA_VALUE", name, "lua_value_create_string called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_STRING;
    v->value.string = memory_manager.strdup(value, MMTAG_LUA_VALUE);
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_error(const char *name, const char *error_message) {
    log_assert("LUA_VALUE", name, "lua_value_create_error called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_ERROR;
    v->value.string = memory_manager.strdup(error_message, MMTAG_LUA_VALUE);
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_table(const char *name) {
    log_assert("LUA_VALUE", name, "lua_value_create_table called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_TABLE;
    v->value.table.items = NULL;
    v->value.table.count = 0;
    v->value.table.capacity = 0;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_ref(const char *name, int value) {
    log_assert("LUA_VALUE", name, "lua_value_create_ref called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_REF;
    v->value.lua_ref = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_userdata(const char *name, void* value) {
    log_assert("LUA_VALUE", name, "lua_value_create_number called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_USERDATA;
    v->value.userdata = value;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_rect(const char *name, struct EseRect* rect) {
    log_assert("LUA_VALUE", name, "lua_value_create_rect called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_RECT;
    v->value.rect = rect;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_point(const char *name, struct EsePoint* point) {
    log_assert("LUA_VALUE", name, "lua_value_create_point called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_POINT;
    v->value.point = point;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_map(const char *name, struct EseMap* map) {
    log_assert("LUA_VALUE", name, "lua_value_create_map called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_MAP;
    v->value.map = map;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_arc(const char *name, struct EseArc* arc) {
    log_assert("LUA_VALUE", name, "lua_value_create_arc called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_ARC;
    v->value.arc = arc;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_color(const char *name, struct EseColor* color) {
    log_assert("LUA_VALUE", name, "lua_value_create_color called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_COLOR;
    v->value.color = color;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_display(const char *name, struct EseDisplay* display) {
    log_assert("LUA_VALUE", name, "lua_value_create_display called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_DISPLAY;
    v->value.display = display;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_input_state(const char *name, struct EseInputState* input_state) {
    log_assert("LUA_VALUE", name, "lua_value_create_input_state called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_INPUT_STATE;
    v->value.input_state = input_state;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_map_cell(const char *name, struct EseMapCell* map_cell) {
    log_assert("LUA_VALUE", name, "lua_value_create_map_cell called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_MAP_CELL;
    v->value.map_cell = map_cell;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_poly_line(const char *name, struct EsePolyLine* poly_line) {
    log_assert("LUA_VALUE", name, "lua_value_create_poly_line called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_POLY_LINE;
    v->value.poly_line = poly_line;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_ray(const char *name, struct EseRay* ray) {
    log_assert("LUA_VALUE", name, "lua_value_create_ray called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_RAY;
    v->value.ray = ray;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_tileset(const char *name, struct EseTileSet* tileset) {
    log_assert("LUA_VALUE", name, "lua_value_create_tileset called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_TILESET;
    v->value.tileset = tileset;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_uuid(const char *name, struct EseUUID* uuid) {
    log_assert("LUA_VALUE", name, "lua_value_create_uuid called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_UUID;
    v->value.uuid = uuid;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_vector(const char *name, struct EseVector* vector) {
    log_assert("LUA_VALUE", name, "lua_value_create_vector called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_VECTOR;
    v->value.vector = vector;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_collision_hit(const char *name, struct EseCollisionHit* hit) {
    log_assert("LUA_VALUE", name, "lua_value_create_collision_hit called with NULL name");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_COLLISION_HIT;
    v->value.collision_hit = hit;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

EseLuaValue *lua_value_create_cfunc(const char *name, EseLuaCFunction cfunc, EseLuaValue *upvalue) {
    log_assert("LUA_VALUE", name, "lua_value_create_cfunc called with NULL name");
    log_assert("LUA_VALUE", cfunc, "lua_value_create_cfunc called with NULL cfunc");

    EseLuaValue *v = memory_manager.calloc(1, sizeof(EseLuaValue), MMTAG_LUA_VALUE);
    v->type = LUA_VAL_CFUNC;
    v->value.cfunc_data.cfunc = cfunc;
    v->value.cfunc_data.upvalue = upvalue;
    if (name) v->name = memory_manager.strdup(name, MMTAG_LUA_VALUE);
    return v;
}

void lua_value_push(EseLuaValue *val, EseLuaValue *item, bool copy) {
    log_assert("LUA_VALUE", val, "lua_value_push called with NULL val");
    log_assert("LUA_VALUE", item, "lua_value_push called with NULL item");

    if (val->type != LUA_VAL_TABLE) {
        log_error("LUA_ENGINE", "lua_value_push item is not a table");
        return;
    }

    // Grow array if needed
    if (val->value.table.count >= val->value.table.capacity) {
        size_t new_capacity = val->value.table.capacity == 0 ? 4 : val->value.table.capacity * 2;
        EseLuaValue **new_items = memory_manager.realloc(val->value.table.items, new_capacity * sizeof(EseLuaValue*), MMTAG_LUA_VALUE);  // ← Array of pointers
        if (!new_items) return;
        val->value.table.items = new_items;
        val->value.table.capacity = new_capacity;
    }

    if (copy) {
        // Deep copy the item
        EseLuaValue* copied_item = lua_value_copy(item);
        val->value.table.items[val->value.table.count] = copied_item;
    } else {
        // Take ownership of the pointer
        val->value.table.items[val->value.table.count] = item;  // ← Store the POINTER
    }
    val->value.table.count++;
}

void lua_value_set_nil(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_set_nil called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_NIL;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_nil");
}

void lua_value_set_bool(EseLuaValue *val, bool value) {
    log_assert("LUA_VALUE", val, "lua_value_set_bool called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_BOOL;
    val->value.boolean = value;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_bool");
}

void lua_value_set_number(EseLuaValue *val, double value) {
    log_assert("LUA_VALUE", val, "lua_value_set_number called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_NUMBER;
    val->value.number = value;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_number");
}

void lua_value_set_string(EseLuaValue *val, const char *value) {
    log_assert("LUA_VALUE", val, "lua_value_set_string called with NULL val");
    log_assert("LUA_VALUE", value, "lua_value_set_string called with NULL value");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_STRING;
    val->value.string = memory_manager.strdup(value, MMTAG_LUA_VALUE);

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_string");
}

void lua_value_set_table(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_set_table called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_TABLE;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_table");
}

void lua_value_set_ref(EseLuaValue *val, int value) {
    log_assert("LUA_VALUE", val, "lua_value_set_ref called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_REF;
    val->value.lua_ref = value;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_ref");
}

void lua_value_set_userdata(EseLuaValue *val, void* value) {
    log_assert("LUA_VALUE", val, "lua_value_set_userdata called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_USERDATA;
    val->value.userdata = value;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_userdata");
}

void lua_value_set_rect(EseLuaValue *val, struct EseRect* rect) {
    log_assert("LUA_VALUE", val, "lua_value_set_rect called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_RECT;
    val->value.rect = rect;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_rect");
}

void lua_value_set_map(EseLuaValue *val, struct EseMap* map) {
    log_assert("LUA_VALUE", val, "lua_value_set_map called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_MAP;
    val->value.map = map;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_map");
}

void lua_value_set_arc(EseLuaValue *val, struct EseArc* arc) {
    log_assert("LUA_VALUE", val, "lua_value_set_arc called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_ARC;
    val->value.arc = arc;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_arc");
}

void lua_value_set_cfunc(EseLuaValue *val, EseLuaCFunction cfunc, EseLuaValue *upvalue) {
    log_assert("LUA_VALUE", val, "lua_value_set_cfunc called with NULL val");
    log_assert("LUA_VALUE", cfunc, "lua_value_set_cfunc called with NULL cfunc");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_CFUNC;
    val->value.cfunc_data.cfunc = cfunc;
    val->value.cfunc_data.upvalue = upvalue;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_cfunc");
}

void lua_value_set_collision_hit(EseLuaValue *val, struct EseCollisionHit* hit) {
    log_assert("LUA_VALUE", val, "lua_value_set_collision_hit called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    _lua_value_reset(val, true);

    val->type = LUA_VAL_COLLISION_HIT;
    val->value.collision_hit = hit;

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_set_collision_hit");
}

EseLuaValue *lua_value_get_table_prop(EseLuaValue *val, const char *prop_name) {
    log_assert("LUA_VALUE", val, "lua_value_get_table_prop called with NULL val");

    profile_start(PROFILE_LUA_VALUE_SET);

    if (val->type != LUA_VAL_TABLE || !prop_name) return NULL;

    for (size_t i = 0; i < val->value.table.count; ++i) {
        EseLuaValue *item = val->value.table.items[i];
        if (item->name && strcmp(item->name, prop_name) == 0) {
            return item;
        }
    }

    profile_stop(PROFILE_LUA_VALUE_SET, "lua_value_get_table_prop");

    return NULL;
}

void lua_value_set_table_prop(EseLuaValue *val, EseLuaValue *prop_value) {
    log_assert("LUA_VALUE", val, "lua_value_set_table_prop called with NULL val");
    log_assert("LUA_VALUE", prop_value, "lua_value_set_table_prop called with NULL prop_value");
    log_assert("LUA_VALUE", val->type == LUA_VAL_TABLE, "lua_value_set_table_prop called on non-table value");
    log_assert("LUA_VALUE", prop_value->name, "lua_value_set_table_prop called with prop_value that has no name");

    // First check if property already exists
    for (size_t i = 0; i < val->value.table.count; ++i) {
        EseLuaValue *item = val->value.table.items[i];
        if (item->name && strcmp(item->name, prop_value->name) == 0) {
            // Property exists, replace it
            lua_value_destroy(item);
            val->value.table.items[i] = lua_value_copy(prop_value);
            return;
        }
    }
    
    // Property doesn't exist, add it
    EseLuaValue *new_item = lua_value_copy(prop_value);
    lua_value_push(val, new_item, false); // Don't copy since we already copied it
}

const char *lua_value_get_name(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_name called with NULL val");

    return val->name;
}

bool lua_value_get_bool(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_bool called with NULL val");

    return val->value.boolean;
}

float lua_value_get_number(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_number called with NULL val");

    return (float)val->value.number;
}

const char *lua_value_get_string(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_string called with NULL val");

    return val->value.string;
}

void *lua_value_get_userdata(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_userdata called with NULL val");

    return val->value.userdata;
}

struct EseRect* lua_value_get_rect(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_rect called with NULL val");

    return val->value.rect;
}

struct EsePoint* lua_value_get_point(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_point called with NULL val");

    return val->value.point;
}

struct EseMap* lua_value_get_map(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_map called with NULL val");

    return val->value.map;
}

struct EseArc* lua_value_get_arc(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_arc called with NULL val");

    return val->value.arc;
}

EseColor* lua_value_get_color(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_color called with NULL val");

    return val->value.color;
}

EseDisplay* lua_value_get_display(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_display called with NULL val");

    return val->value.display;
}

EseInputState* lua_value_get_input_state(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_input_state called with NULL val");

    return val->value.input_state;
}

EseMapCell* lua_value_get_map_cell(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_map_cell called with NULL val");

    return val->value.map_cell;
}

EsePolyLine* lua_value_get_poly_line(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_poly_line called with NULL val");

    return val->value.poly_line;
}

EseRay* lua_value_get_ray(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_ray called with NULL val");

    return val->value.ray;
}

EseTileSet* lua_value_get_tileset(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_tileset called with NULL val");

    return val->value.tileset;
}

EseUUID* lua_value_get_uuid(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_uuid called with NULL val");

    return val->value.uuid;
}

EseVector* lua_value_get_vector(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_vector called with NULL val");

    return val->value.vector;
}

EseCollisionHit* lua_value_get_collision_hit(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_collision_hit called with NULL val");

    return val->value.collision_hit;
}

EseLuaCFunction lua_value_get_cfunc(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_cfunc called with NULL val");

    return val->value.cfunc_data.cfunc;
}

EseLuaValue* lua_value_get_cfunc_upvalue(EseLuaValue *val) {
    log_assert("LUA_VALUE", val, "lua_value_get_cfunc_upvalue called with NULL val");

    return val->value.cfunc_data.upvalue;
}

// Type checking functions
bool lua_value_is_nil(EseLuaValue *val) {
    return val && val->type == LUA_VAL_NIL;
}

bool lua_value_is_bool(EseLuaValue *val) {
    return val && val->type == LUA_VAL_BOOL;
}

bool lua_value_is_number(EseLuaValue *val) {
    return val && val->type == LUA_VAL_NUMBER;
}

bool lua_value_is_string(EseLuaValue *val) {
    return val && val->type == LUA_VAL_STRING;
}

bool lua_value_is_table(EseLuaValue *val) {
    return val && val->type == LUA_VAL_TABLE;
}

bool lua_value_is_ref(EseLuaValue *val) {
    return val && val->type == LUA_VAL_REF;
}

bool lua_value_is_userdata(EseLuaValue *val) {
    return val && val->type == LUA_VAL_USERDATA;
}

bool lua_value_is_rect(EseLuaValue *val) {
    return val && val->type == LUA_VAL_RECT;
}

bool lua_value_is_point(EseLuaValue *val) {
    return val && val->type == LUA_VAL_POINT;
}

bool lua_value_is_map(EseLuaValue *val) {
    return val && val->type == LUA_VAL_MAP;
}

bool lua_value_is_arc(EseLuaValue *val) {
    return val && val->type == LUA_VAL_ARC;
}

bool lua_value_is_color(EseLuaValue *val) {
    return val && val->type == LUA_VAL_COLOR;
}

bool lua_value_is_display(EseLuaValue *val) {
    return val && val->type == LUA_VAL_DISPLAY;
}

bool lua_value_is_input_state(EseLuaValue *val) {
    return val && val->type == LUA_VAL_INPUT_STATE;
}

bool lua_value_is_map_cell(EseLuaValue *val) {
    return val && val->type == LUA_VAL_MAP_CELL;
}

bool lua_value_is_poly_line(EseLuaValue *val) {
    return val && val->type == LUA_VAL_POLY_LINE;
}

bool lua_value_is_ray(EseLuaValue *val) {
    return val && val->type == LUA_VAL_RAY;
}

bool lua_value_is_tileset(EseLuaValue *val) {
    return val && val->type == LUA_VAL_TILESET;
}

bool lua_value_is_uuid(EseLuaValue *val) {
    return val && val->type == LUA_VAL_UUID;
}

bool lua_value_is_vector(EseLuaValue *val) {
    return val && val->type == LUA_VAL_VECTOR;
}

bool lua_value_is_collision_hit(EseLuaValue *val) {
    return val && val->type == LUA_VAL_COLLISION_HIT;
}

bool lua_value_is_cfunc(EseLuaValue *val) {
    return val && val->type == LUA_VAL_CFUNC;
}

bool lua_value_is_error(EseLuaValue *val) {
    return val && val->type == LUA_VAL_ERROR;
}

void lua_value_destroy(EseLuaValue *val) {
    if (!val) return;

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
        case LUA_VAL_RECT:
            *offset += snprintf(buf + *offset, buflen - *offset, "Rect: %p\n", val->value.rect);
            break;
        case LUA_VAL_POINT:
            *offset += snprintf(buf + *offset, buflen - *offset, "Point: %p\n", val->value.point);
            break;
        case LUA_VAL_MAP:
            *offset += snprintf(buf + *offset, buflen - *offset, "Map: %p\n", val->value.map);
            break;
        case LUA_VAL_ARC:
            *offset += snprintf(buf + *offset, buflen - *offset, "Arc: %p\n", val->value.arc);
            break;
        case LUA_VAL_COLOR:
            *offset += snprintf(buf + *offset, buflen - *offset, "Color: %p\n", val->value.color);
            break;
        case LUA_VAL_DISPLAY:
            *offset += snprintf(buf + *offset, buflen - *offset, "Display: %p\n", val->value.display);
            break;
        case LUA_VAL_INPUT_STATE:
            *offset += snprintf(buf + *offset, buflen - *offset, "InputState: %p\n", val->value.input_state);
            break;
        case LUA_VAL_MAP_CELL:
            *offset += snprintf(buf + *offset, buflen - *offset, "MapCell: %p\n", val->value.map_cell);
            break;
        case LUA_VAL_POLY_LINE:
            *offset += snprintf(buf + *offset, buflen - *offset, "PolyLine: %p\n", val->value.poly_line);
            break;
        case LUA_VAL_RAY:
            *offset += snprintf(buf + *offset, buflen - *offset, "Ray: %p\n", val->value.ray);
            break;
        case LUA_VAL_TILESET:
            *offset += snprintf(buf + *offset, buflen - *offset, "Tileset: %p\n", val->value.tileset);
            break;
        case LUA_VAL_UUID:
            *offset += snprintf(buf + *offset, buflen - *offset, "Uuid: %p\n", val->value.uuid);
            break;
        case LUA_VAL_VECTOR:
            *offset += snprintf(buf + *offset, buflen - *offset, "Vector: %p\n", val->value.vector);
            break;
        case LUA_VAL_CFUNC:
            *offset += snprintf(buf + *offset, buflen - *offset, "CFunc: %p (upvalue: %p)\n", val->value.cfunc_data.cfunc, val->value.cfunc_data.upvalue);
            break;
        case LUA_VAL_COLLISION_HIT:
            *offset += snprintf(buf + *offset, buflen - *offset, "CollisionHit: %p\n", val->value.collision_hit);
            break;
        case LUA_VAL_ERROR:
            *offset += snprintf(buf + *offset, buflen - *offset, "Error: %s\n", val->value.string);
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
        log_debug("LUA_VALUE", "log_luavalue: (null)");
        return;
    }
    char buf[LOG_LUAVALUE_MAXLEN];
    size_t offset = 0;
    buf[0] = '\0';

    _log_luavalue_rec(val, buf, sizeof(buf), &offset, 0);

    // Ensure null-termination
    buf[sizeof(buf) - 1] = '\0';

    log_debug("LUA_VALUE", "\n%s", buf);
}
