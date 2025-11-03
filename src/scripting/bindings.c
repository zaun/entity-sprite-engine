/*
 * Project: Entity Sprite Engine
 *
 * Implementation of generic Lua object binding system for registering C types
 * with instance and object-level fields and methods.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#include <stdlib.h>
#include <string.h>
#include "../vendor/lua/src/lauxlib.h"
#include "../vendor/lua/src/lua.h"
#include "../vendor/lua/src/lualib.h"
#include "core/memory_manager.h"
#include "scripting/bindings.h"

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Finds a field descriptor by name.
 *
 * @details Searches through an array of field descriptors for a field matching
 *          the given key name.
 *
 * @param fields Array of field descriptors to search
 * @param count Number of fields in the array
 * @param key Field name to find
 * @return Pointer to the matching field descriptor, or NULL if not found
 */
static const FieldDesc *_find_field(const FieldDesc *fields, size_t count,
                                     const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(fields[i].name, key) == 0)
            return &fields[i];
    }
    return NULL;
}

/**
 * @brief Finds a method descriptor by name.
 *
 * @details Searches through an array of method descriptors for a method
 * matching the given key name.
 *
 * @param methods Array of method descriptors to search
 * @param count Number of methods in the array
 * @param key Method name to find
 * @return Pointer to the matching method descriptor, or NULL if not found
 */
static const MethodDesc *_find_method(const MethodDesc *methods, size_t count,
                                       const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(methods[i].name, key) == 0)
            return &methods[i];
    }
    return NULL;
}

/**
 * @brief Pushes an instance field value onto the Lua stack.
 *
 * @details Reads the field value from the C object at the specified offset
 *          and pushes it onto the Lua stack according to its type.
 *
 * @param L Lua state pointer
 * @param obj Pointer to the C object
 * @param f Field descriptor describing which field to read
 */
static void push_field(lua_State *L, const void *obj, const FieldDesc *f) {
    const char *p = (const char *)obj + f->offset;
    switch (f->type) {
    case BIND_BOOL:
        lua_pushboolean(L, *(const int *)p);
        break;
    case BIND_INT:
        lua_pushinteger(L, *(const int *)p);
        break;
    case BIND_FLOAT:
        lua_pushnumber(L, *(const float *)p);
        break;
    case BIND_STRING:
        lua_pushstring(L, *(const char *const *)p);
        break;
    case BIND_SIZE_T:
        lua_pushinteger(L, (lua_Integer)*(const size_t *)p);
        break;
    default:
        lua_pushnil(L);
        break;
    }
}

/**
 * @brief Sets an instance field value from a Lua stack value.
 *
 * @details Reads a value from the Lua stack at the specified index and writes
 *          it to the C object field at the specified offset according to its
 *          type.
 *
 * @param L Lua state pointer
 * @param obj Pointer to the C object
 * @param f Field descriptor describing which field to write
 * @param value_index Stack index of the Lua value to read
 */
static void set_field(lua_State *L, void *obj, const FieldDesc *f,
                      int value_index) {
    char *p = (char *)obj + f->offset;
    switch (f->type) {
    case BIND_BOOL:
        *(int *)p = lua_toboolean(L, value_index);
        break;
    case BIND_INT:
        *(int *)p = (int)lua_tointeger(L, value_index);
        break;
    case BIND_FLOAT:
        *(float *)p = (float)lua_tonumber(L, value_index);
        break;
    case BIND_STRING:
        *(const char **)p = lua_tostring(L, value_index);
        break;
    case BIND_SIZE_T:
        *(size_t *)p = (size_t)lua_tointeger(L, value_index);
        break;
    }
}

/**
 * @brief Pushes an object field initial value onto the Lua stack.
 *
 * @details For object fields, the offset can point to static storage containing
 *          an initial value. If offset is 0, uses default values based on type.
 *          This is used during registration to set initial values on the class
 *          table.
 *
 * @param L Lua state pointer
 * @param f Field descriptor describing the object field
 */
static void push_object_field(lua_State *L, const FieldDesc *f) {
    const void *initial_value = NULL;
    if (f->offset != 0) {
        initial_value = (const void *)f->offset;
    }

    if (initial_value == NULL) {
        switch (f->type) {
        case BIND_BOOL:
            lua_pushboolean(L, 0);
            break;
        case BIND_INT:
            lua_pushinteger(L, 0);
            break;
        case BIND_FLOAT:
            lua_pushnumber(L, 0.0);
            break;
        case BIND_STRING:
            lua_pushstring(L, "");
            break;
        case BIND_SIZE_T:
            lua_pushinteger(L, 0);
            break;
        default:
            lua_pushnil(L);
            break;
        }
    } else {
        const char *p = (const char *)initial_value;
        switch (f->type) {
        case BIND_BOOL:
            lua_pushboolean(L, *(const int *)p);
            break;
        case BIND_INT:
            lua_pushinteger(L, *(const int *)p);
            break;
        case BIND_FLOAT:
            lua_pushnumber(L, *(const float *)p);
            break;
        case BIND_STRING:
            lua_pushstring(L, *(const char *const *)p);
            break;
        case BIND_SIZE_T:
            lua_pushinteger(L, (lua_Integer)*(const size_t *)p);
            break;
        default:
            lua_pushnil(L);
            break;
        }
    }
}

/**
 * @brief Lua __index metamethod for accessing instance and object fields and
 * methods.
 *
 * @details First checks for instance fields and methods. If not found, checks
 *          the class table for object-level fields and methods. This allows
 *          instances to access class-level members.
 *
 * @param L Lua state pointer
 * @return Number of values pushed onto the stack (1 for field/method, 1 for
 * nil)
 */
static int meta_index(lua_State *L) {
    void *obj = *(void **)lua_touserdata(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!obj || !key)
        return 0;

    const FieldDesc *fields =
        (const FieldDesc *)lua_touserdata(L, lua_upvalueindex(1));
    size_t instance_field_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(2));
    const MethodDesc *methods =
        (const MethodDesc *)lua_touserdata(L, lua_upvalueindex(3));
    size_t instance_method_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(4));

    const FieldDesc *f = _find_field(fields, instance_field_count, key);
    if (f) {
        push_field(L, obj, f);
        return 1;
    }

    const MethodDesc *m =
        _find_method(methods, instance_method_count, key);
    if (m) {
        lua_pushcfunction(L, m->fn);
        return 1;
    }

    const FieldDesc *object_fields =
        (const FieldDesc *)lua_touserdata(L, lua_upvalueindex(5));
    size_t object_field_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(6));
    const MethodDesc *object_methods =
        (const MethodDesc *)lua_touserdata(L, lua_upvalueindex(7));
    size_t object_method_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(8));
    const char *class_name =
        (const char *)lua_touserdata(L, lua_upvalueindex(9));

    if (object_fields) {
        const FieldDesc *of =
            _find_field(object_fields, object_field_count, key);
        if (of) {
            lua_getglobal(L, class_name);
            lua_getfield(L, -1, key);
            if (!lua_isnil(L, -1)) {
                lua_remove(L, -2);
                return 1;
            }
            lua_pop(L, 2);
        }
    }

    if (object_methods) {
        const MethodDesc *om =
            _find_method(object_methods, object_method_count, key);
        if (om) {
            lua_getglobal(L, class_name);
            lua_getfield(L, -1, key);
            if (!lua_isnil(L, -1)) {
                lua_remove(L, -2);
                return 1;
            }
            lua_pop(L, 2);
        }
    }

    lua_pushnil(L);
    return 1;
}

/**
 * @brief Lua __newindex metamethod for setting instance field values.
 *
 * @details Allows Lua scripts to set instance field values by writing to the C
 *          object at the specified offset according to the field type. Object
 *          fields are read-only and cannot be set from instances.
 *
 * @param L Lua state pointer
 * @return Number of values pushed onto the stack (always 0)
 */
static int meta_newindex(lua_State *L) {
    void *obj = *(void **)lua_touserdata(L, 1);
    const char *key = lua_tostring(L, 2);
    if (!obj || !key)
        return 0;

    const FieldDesc *object_fields =
        (const FieldDesc *)lua_touserdata(L, lua_upvalueindex(1));
    size_t object_field_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(2));

    if (object_fields) {
        const FieldDesc *of =
            _find_field(object_fields, object_field_count, key);
        if (of) {
            return 0;
        }
    }

    const FieldDesc *fields =
        (const FieldDesc *)lua_touserdata(L, lua_upvalueindex(3));
    size_t instance_field_count =
        (size_t)lua_tointeger(L, lua_upvalueindex(4));

    const FieldDesc *f =
        _find_field(fields, instance_field_count, key);
    if (f) {
        set_field(L, obj, f, 3);
    }

    return 0;
}

/**
 * @brief Lua __gc metamethod for cleaning up C objects when Lua garbage
 * collects userdata.
 *
 * @details Frees the C object memory allocated during object creation. Uses
 *          standard free() since these objects are allocated with calloc() for
 *          Lua userdata.
 *
 * @param L Lua state pointer
 * @return Number of values pushed onto the stack (always 0)
 */
static int meta_gc(lua_State *L) {
    void *obj = *(void **)lua_touserdata(L, 1);
    size_t obj_size = (size_t)lua_tointeger(L, lua_upvalueindex(1));
    if (obj) {
        memory_manager.free(obj);
        *(void **)lua_touserdata(L, 1) = NULL;
    }
    return 0;
}

/**
 * @brief Generic constructor function for creating new instances from Lua.
 *
 * @details Allocates memory for a new C object and creates a Lua userdata with
 *          the appropriate metatable. Uses calloc() for zero-initialization,
 *          which is standard for Lua userdata allocation.
 *
 * @param L Lua state pointer
 * @return Number of values pushed onto the stack (1 for the new userdata)
 */
static int generic_new(lua_State *L) {
    const char *meta_name = lua_tostring(L, lua_upvalueindex(1));
    size_t obj_size = (size_t)lua_tointeger(L, lua_upvalueindex(2));
    void *obj = memory_manager.calloc(1, obj_size, MMTAG_LUA);

    void **ud = (void **)lua_newuserdata(L, sizeof(void *));
    *ud = obj;
    luaL_getmetatable(L, meta_name);
    lua_setmetatable(L, -2);
    return 1;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

void lua_bind_register_object(lua_State *L, const char *name,
    const FieldDesc *instance_fields, size_t instance_field_count,
    const MethodDesc *instance_methods, size_t instance_method_count,
    const FieldDesc *object_fields, size_t object_field_count,
    const MethodDesc *object_methods, size_t object_method_count,
    size_t object_size) {
    luaL_newmetatable(L, name);

    lua_pushlightuserdata(L, (void *)instance_fields);
    lua_pushinteger(L, (lua_Integer)instance_field_count);
    lua_pushlightuserdata(L, (void *)instance_methods);
    lua_pushinteger(L, (lua_Integer)instance_method_count);
    lua_pushlightuserdata(L, (void *)object_fields);
    lua_pushinteger(L, (lua_Integer)object_field_count);
    lua_pushlightuserdata(L, (void *)object_methods);
    lua_pushinteger(L, (lua_Integer)object_method_count);
    lua_pushlightuserdata(L, (void *)name);
    lua_pushcclosure(L, meta_index, 9);
    lua_setfield(L, -2, "__index");

    lua_pushlightuserdata(L, (void *)object_fields);
    lua_pushinteger(L, (lua_Integer)object_field_count);
    lua_pushlightuserdata(L, (void *)instance_fields);
    lua_pushinteger(L, (lua_Integer)instance_field_count);
    lua_pushcclosure(L, meta_newindex, 4);
    lua_setfield(L, -2, "__newindex");

    lua_pushinteger(L, (lua_Integer)object_size);
    lua_pushcclosure(L, meta_gc, 1);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushstring(L, name);
    lua_pushinteger(L, (lua_Integer)object_size);
    lua_pushcclosure(L, generic_new, 2);
    lua_setfield(L, -2, "new");

    if (object_fields) {
        for (size_t i = 0; i < object_field_count; i++) {
            push_object_field(L, &object_fields[i]);
            lua_setfield(L, -2, object_fields[i].name);
        }
    }

    if (object_methods) {
        for (size_t i = 0; i < object_method_count; i++) {
            lua_pushcfunction(L, object_methods[i].fn);
            lua_setfield(L, -2, object_methods[i].name);
        }
    }

    lua_setglobal(L, name);
}

void *lua_bind_get_object(lua_State *L, int index, const char *meta_name) {
    void **ud = (void **)luaL_testudata(L, index, meta_name);
    if (!ud)
        return NULL;
    return *ud;
}

