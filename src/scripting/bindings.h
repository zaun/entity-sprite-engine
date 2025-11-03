/*
 * Project: Entity Sprite Engine
 *
 * Generic Lua object binding system for registering C types with instance and
 * object-level fields and methods. Provides automatic metamethod generation for
 * field access, method calls, and garbage collection.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#ifndef ESE_BINDINGS_H
#define ESE_BINDINGS_H

#include <stddef.h>
#include "../vendor/lua/src/lauxlib.h"
#include "../vendor/lua/src/lua.h"
#include "../vendor/lua/src/lualib.h"

// ========================================
// DEFINES AND STRUCTS
// ========================================

/**
 * @brief Enumeration of supported field types for Lua bindings.
 */
typedef enum {
    BIND_BOOL,   /** Boolean field type */
    BIND_INT,    /** Integer field type */
    BIND_FLOAT,  /** Floating-point field type */
    BIND_STRING, /** String field type */
    BIND_SIZE_T  /** Size_t field type */
} BindType;

/**
 * @brief Descriptor for a field accessible from Lua.
 *
 * @details Describes a single field that can be accessed or modified from Lua
 *          scripts. The offset field is used to calculate the memory location
 *          of the field within a C struct.
 */
typedef struct {
    const char *name;   /** Field name as exposed to Lua */
    BindType type;      /** Type of the field */
    size_t offset;      /** Byte offset into the C struct */
} FieldDesc;

/**
 * @brief Descriptor for a method accessible from Lua.
 *
 * @details Describes a single method that can be called from Lua scripts.
 */
typedef struct {
    const char *name; /** Method name as exposed to Lua */
    lua_CFunction fn; /** C function implementing the method */
} MethodDesc;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Registers a C type with Lua, creating both instance and object-level
 * bindings.
 *
 * @details Creates a metatable for the type and registers instance fields and
 *          methods that operate on instances, as well as object-level fields and
 *          methods accessible on the class table. The class table is created as
 *          a global with a "new" constructor function. Object fields and methods
 *          can be customized from Lua after registration.
 *
 * @param L Lua state pointer
 * @param name Name of the type as exposed to Lua (e.g., "Point")
 * @param fields Array of instance field descriptors
 * @param instance_field_count Number of instance fields
 * @param methods Array of instance method descriptors
 * @param instance_method_count Number of instance methods
 * @param object_fields Array of object-level field descriptors (can be NULL)
 * @param object_field_count Number of object fields (0 if object_fields is NULL)
 * @param object_methods Array of object-level method descriptors (can be NULL)
 * @param object_method_count Number of object methods (0 if object_methods is NULL)
 * @param object_size Size in bytes of the C struct being bound
 */
void lua_bind_register_object(lua_State *L, const char *name,
                               const FieldDesc *instance_fields, size_t instance_field_count,
                               const MethodDesc *instance_methods, size_t instance_method_count,
                               const FieldDesc *object_fields, size_t object_field_count,
                               const MethodDesc *object_methods, size_t object_method_count,
                               size_t object_size);

/**
 * @brief Extracts a C object pointer from a Lua userdata value.
 *
 * @details Retrieves the C object pointer from a Lua userdata at the specified
 *          stack index, verifying it has the correct metatable type.
 *
 * @param L Lua state pointer
 * @param index Stack index of the Lua userdata value
 * @param meta_name Metatable name to verify (e.g., "Point")
 * @return Pointer to the C object, or NULL if the value is not the correct type
 */
void *lua_bind_get_object(lua_State *L, int index, const char *meta_name);

#endif // ESE_BINDINGS_H

