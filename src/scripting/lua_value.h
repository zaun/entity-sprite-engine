#ifndef ESE_LUA_VALUE_H
#define ESE_LUA_VALUE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct EseLuaValue EseLuaValue;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseArc EseArc;
typedef struct EseColor EseColor;
typedef struct EseDisplay EseDisplay;
typedef struct EseInputState EseInputState;
typedef struct EseMap EseMap;
typedef struct EseMapCell EseMapCell;
typedef struct EsePolyLine EsePolyLine;
typedef struct EseRay EseRay;
typedef struct EseTileSet EseTileSet;
typedef struct EseUUID EseUUID;
typedef struct EseVector EseVector;
typedef struct EseCollisionHit EseCollisionHit;
typedef struct EsePoint EsePoint;
typedef struct EseRect EseRect;
typedef struct lua_State lua_State;

// Forward declaration for EseLuaCFunction (defined in lua_engine.h)
typedef EseLuaValue *(*EseLuaCFunction)(EseLuaEngine *engine, size_t argc, EseLuaValue *argv[]);

/**
 * @brief Creates a complete deep copy of a EseLuaValue structure.
 *
 * @details Recursively duplicates the entire EseLuaValue including type, name,
 *          and all data; for strings, creates new duplicate strings; for
 * tables, allocates new items array and recursively copies all contained items;
 *          preserves all structure and relationships in the copy.
 *
 * @param src Pointer to the source EseLuaValue to copy.
 *
 * @return Pointer to newly allocated deep copy on success, NULL if src is NULL
 * or allocation fails.
 *
 * @warning Recursive function; deeply nested tables may cause stack overflow or
 * excessive memory use.
 * @warning Returned copy must be memory_manager.freed with lua_value_destroy().
 * @warning Large structures may require significant memory allocation.
 */
EseLuaValue *lua_value_copy(const EseLuaValue *src);

/**
 * @brief Converts a Lua value on the stack to an EseLuaValue structure.
 *
 * @details This function converts the Lua value at the specified stack index
 *          to an EseLuaValue structure. It handles all basic Lua types
 * including nil, boolean, number, string, table, and userdata.
 *
 * @param L Lua state pointer
 * @param index Stack index of the Lua value to convert
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning The caller is responsible for freeing the returned EseLuaValue when
 * done.
 */
EseLuaValue *lua_value_from_stack(lua_State *L, int index);

/**
 * @brief Creates a new EseLuaValue structure initialized to NIL type.
 *
 * @details Allocates memory for a EseLuaValue using calloc, sets type to
 * LUA_VAL_NIL, and optionally duplicates the provided name string.
 *
 * @param name Optional name for the value. Can be NULL. Will be duplicated if
 * provided.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 */
EseLuaValue *lua_value_create_nil(const char *name);

/**
 * @brief Creates a new EseLuaValue structure initialized to BOOL type with
 * given value.
 *
 * @details Allocates memory, sets type to LUA_VAL_BOOL, stores the boolean
 * value, and optionally duplicates the name string.
 *
 * @param name Optional name for the value. Can be NULL. Will be duplicated if
 * provided.
 * @param value Boolean value to store in the EseLuaValue.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 */
EseLuaValue *lua_value_create_bool(const char *name, bool value);

/**
 * @brief Creates a new EseLuaValue structure initialized to NUMBER type with
 * given value.
 *
 * @details Allocates memory, sets type to LUA_VAL_NUMBER, stores the numeric
 * value as a double, and optionally duplicates the name string.
 *
 * @param name Optional name for the value. Can be NULL. Will be duplicated if
 * provided.
 * @param value Numeric value to store in the EseLuaValue.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 */
EseLuaValue *lua_value_create_number(const char *name, double value);

/**
 * @brief Creates a new EseLuaValue structure initialized to STRING type with
 * duplicated string.
 *
 * @details Allocates memory, sets type to LUA_VAL_STRING, creates a duplicate
 * copy of the provided string value, and optionally duplicates the name string.
 *
 * @param name Optional name for the value. Can be NULL. Will be duplicated if
 * provided.
 * @param value String value to store. Must not be NULL. Will be duplicated.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 * @warning The string is duplicated, so the original can be safely freed after
 * this call.
 */
EseLuaValue *lua_value_create_string(const char *name, const char *value);

/**
 * @brief Creates a new EseLuaValue structure initialized to ERROR type.
 *
 * @details Allocates memory, sets type to LUA_VAL_ERROR, duplicates the error
 * message string, and sets the name field. The error message will be used by
 * luaL_error.
 *
 * @param name Optional name for the value (can be NULL).
 * @param error_message The error message string to store.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL if allocation
 * fails.
 *
 * @warning Returned value must be freed with lua_value_destroy().
 * @warning The error_message string is duplicated, so the original can be
 * freed.
 */
EseLuaValue *lua_value_create_error(const char *name, const char *error_message);

/**
 * @brief Creates a new empty EseLuaValue structure initialized to TABLE type.
 *
 * @details Allocates memory, sets type to LUA_VAL_TABLE, initializes the items
 * array to NULL with zero count and capacity, and optionally duplicates the
 * name.
 *
 * @param name Optional name for the table. Can be NULL. Will be duplicated if
 * provided.
 *
 * @return Pointer to newly allocated empty EseLuaValue table on success, NULL
 * on failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 * @warning Use lua_value_push() to add items to the table.
 */
EseLuaValue *lua_value_create_table(const char *name);

/**
 * @brief Creates a new EseLuaValue structure initialized to reference type.
 *
 * @details Allocates memory, sets type to LUA_VAL_REF, stores the reference
 * value, and optionally duplicates the name string.
 *
 * @param name Optional name for the value. Can be NULL. Will be duplicated if
 * provided.
 * @param value Int value to store.
 *
 * @return Pointer to newly allocated EseLuaValue on success, NULL on memory
 * allocation failure.
 *
 * @warning Must be freed with lua_value_destroy() to prevent memory leaks.
 */
EseLuaValue *lua_value_create_ref(const char *name, int value);

EseLuaValue *lua_value_create_userdata(const char *name, void *value);

EseLuaValue *lua_value_create_rect(const char *name, struct EseRect *rect);

EseLuaValue *lua_value_create_point(const char *name, EsePoint *point);

EseLuaValue *lua_value_create_map(const char *name, struct EseMap *map);

EseLuaValue *lua_value_create_arc(const char *name, struct EseArc *arc);

EseLuaValue *lua_value_create_color(const char *name, struct EseColor *color);

EseLuaValue *lua_value_create_display(const char *name, struct EseDisplay *display);

EseLuaValue *lua_value_create_input_state(const char *name, struct EseInputState *input_state);

EseLuaValue *lua_value_create_map_cell(const char *name, struct EseMapCell *map_cell);

EseLuaValue *lua_value_create_poly_line(const char *name, struct EsePolyLine *poly_line);

EseLuaValue *lua_value_create_ray(const char *name, struct EseRay *ray);

EseLuaValue *lua_value_create_tileset(const char *name, struct EseTileSet *tileset);

EseLuaValue *lua_value_create_uuid(const char *name, struct EseUUID *uuid);

EseLuaValue *lua_value_create_vector(const char *name, struct EseVector *vector);

EseLuaValue *lua_value_create_collision_hit(const char *name, struct EseCollisionHit *hit);

EseLuaValue *lua_value_create_cfunc(const char *name, EseLuaCFunction cfunc, EseLuaValue *upvalue);

/**
 * @brief Adds an item to a EseLuaValue table, with optional deep copying.
 *
 * @details Validates the table type, grows the internal items array if needed
 *          (doubling capacity), and either stores a pointer to the item
 * directly or creates a deep copy before storing. The array automatically
 * expands starting from capacity 4 and doubling each time.
 *
 * @param val Pointer to a EseLuaValue of type TABLE to add the item to.
 * @param item Pointer to the EseLuaValue to add to the table.
 * @param copy If true, creates a deep copy of item; if false, takes ownership
 * of the pointer.
 *
 * @warning If copy is false, the caller must not free or modify the item after
 * this call.
 * @warning The function silently fails if val is NULL, item is NULL, or val is
 * not a table.
 * @warning Memory allocation failures during array growth will be silently
 * ignored.
 */
void lua_value_push(EseLuaValue *val, EseLuaValue *item, bool copy);

/**
 * @brief Resets an existing EseLuaValue to NIL type, preserving the name.
 *
 * @details Calls internal reset function to free any allocated memory (strings,
 *          table items), preserves the existing name field, and sets type to
 * NIL.
 *
 * @param val Pointer to the EseLuaValue to reset. Safe to pass NULL.
 *
 * @warning Any previously stored data (strings, table contents) will be freed
 * and lost.
 */
void lua_value_set_nil(EseLuaValue *val);

/**
 * @brief Resets an existing EseLuaValue to BOOL type with new value, preserving
 * the name.
 *
 * @details Frees any existing allocated memory, preserves the name field,
 *          sets type to LUA_VAL_BOOL, and stores the new boolean value.
 *
 * @param val Pointer to the EseLuaValue to modify. Safe to pass NULL.
 * @param value New boolean value to store.
 *
 * @warning Any previously stored data will be freed and lost.
 */
void lua_value_set_bool(EseLuaValue *val, bool value);

/**
 * @brief Resets an existing EseLuaValue to NUMBER type with new value,
 * preserving the name.
 *
 * @details Frees any existing allocated memory, preserves the name field,
 *          sets type to LUA_VAL_NUMBER, and stores the new numeric value.
 *
 * @param val Pointer to the EseLuaValue to modify. Safe to pass NULL.
 * @param value New numeric value to store.
 *
 * @warning Any previously stored data will be freed and lost.
 */
void lua_value_set_number(EseLuaValue *val, double value);

/**
 * @brief Resets an existing EseLuaValue to STRING type with new duplicated
 * string, preserving the name.
 *
 * @details Frees any existing allocated memory, preserves the name field,
 *          sets type to LUA_VAL_STRING, and stores a duplicate of the provided
 * string.
 *
 * @param val Pointer to the EseLuaValue to modify. Safe to pass NULL.
 * @param value New string value to store. Must not be NULL. Will be duplicated.
 *
 * @warning Any previously stored data will be freed and lost.
 * @warning The string is duplicated, so the original can be safely freed after
 * this call.
 */
void lua_value_set_string(EseLuaValue *val, const char *value);

/**
 * @brief Resets an existing EseLuaValue to empty TABLE type, preserving the
 * name.
 *
 * @details Frees any existing allocated memory including all table items,
 *          preserves the name field, sets type to LUA_VAL_TABLE, and
 * initializes an empty items array with zero count and capacity.
 *
 * @param val Pointer to the EseLuaValue to modify. Safe to pass NULL.
 *
 * @warning Any previously stored data including all table contents will be
 * freed and lost.
 */
void lua_value_set_table(EseLuaValue *val);

/**
 * @brief Resets an existing EseLuaValue to REF type with new value, preserving
 * the name.
 *
 * @details Frees any existing allocated memory, preserves the name field,
 *          sets type to LUA_VAL_REF, and stores the new int value.
 *
 * @param val Pointer to the EseLuaValue to modify. Safe to pass NULL.
 * @param value New int value to store.
 *
 * @warning Any previously stored data will be freed and lost.
 */
void lua_value_set_ref(EseLuaValue *val, int value);

void lua_value_set_userdata(EseLuaValue *val, void *value);

void lua_value_set_rect(EseLuaValue *val, struct EseRect *rect);

void lua_value_set_map(EseLuaValue *val, struct EseMap *map);

void lua_value_set_arc(EseLuaValue *val, struct EseArc *arc);

void lua_value_set_cfunc(EseLuaValue *val, EseLuaCFunction cfunc, EseLuaValue *upvalue);

void lua_value_set_collision_hit(EseLuaValue *val, struct EseCollisionHit *hit);

/**
 * @brief Searches a EseLuaValue table for a property by name and returns a
 * pointer to it.
 *
 * @details Validates the table type, iterates through all items in the table's
 *          items array, compares each item's name field using strcmp, and
 * returns a direct pointer to the first matching item.
 *
 * @param val Pointer to a EseLuaValue of type TABLE to search.
 * @param prop_name Name of the property to find. Must not be NULL.
 *
 * @return Pointer to the EseLuaValue property within the table if found, NULL
 * if not found, if val is NULL, if val is not a table, or if prop_name is NULL.
 *
 * @warning The returned pointer is to the actual item in the table's items
 * array. Do not free it directly; it will be freed when the parent table is
 * freed.
 * @warning Modifying the returned EseLuaValue will directly affect the table's
 * contents.
 */
EseLuaValue *lua_value_get_table_prop(EseLuaValue *val, const char *prop_name);

void lua_value_set_table_prop(EseLuaValue *val, EseLuaValue *prop_value);

const char *lua_value_get_name(EseLuaValue *val);

bool lua_value_get_bool(EseLuaValue *val);

float lua_value_get_number(EseLuaValue *val);

const char *lua_value_get_string(EseLuaValue *val);

void *lua_value_get_userdata(EseLuaValue *val);

struct EseRect *lua_value_get_rect(EseLuaValue *val);

EsePoint *lua_value_get_point(EseLuaValue *val);

struct EseMap *lua_value_get_map(EseLuaValue *val);

struct EseArc *lua_value_get_arc(EseLuaValue *val);

struct EseColor *lua_value_get_color(EseLuaValue *val);

struct EseDisplay *lua_value_get_display(EseLuaValue *val);

struct EseInputState *lua_value_get_input_state(EseLuaValue *val);

struct EseMapCell *lua_value_get_map_cell(EseLuaValue *val);

struct EsePolyLine *lua_value_get_poly_line(EseLuaValue *val);

struct EseRay *lua_value_get_ray(EseLuaValue *val);

struct EseTileSet *lua_value_get_tileset(EseLuaValue *val);

struct EseUUID *lua_value_get_uuid(EseLuaValue *val);

struct EseVector *lua_value_get_vector(EseLuaValue *val);

struct EseCollisionHit *lua_value_get_collision_hit(EseLuaValue *val);

EseLuaCFunction lua_value_get_cfunc(EseLuaValue *val);
EseLuaValue *lua_value_get_cfunc_upvalue(EseLuaValue *val);

// Type checking functions
bool lua_value_is_nil(EseLuaValue *val);
bool lua_value_is_bool(EseLuaValue *val);
bool lua_value_is_number(EseLuaValue *val);
bool lua_value_is_string(EseLuaValue *val);
bool lua_value_is_table(EseLuaValue *val);
bool lua_value_is_ref(EseLuaValue *val);
bool lua_value_is_userdata(EseLuaValue *val);
bool lua_value_is_rect(EseLuaValue *val);
bool lua_value_is_point(EseLuaValue *val);
bool lua_value_is_map(EseLuaValue *val);
bool lua_value_is_arc(EseLuaValue *val);
bool lua_value_is_camera(EseLuaValue *val);
bool lua_value_is_color(EseLuaValue *val);
bool lua_value_is_display(EseLuaValue *val);
bool lua_value_is_input_state(EseLuaValue *val);
bool lua_value_is_map_cell(EseLuaValue *val);
bool lua_value_is_poly_line(EseLuaValue *val);
bool lua_value_is_ray(EseLuaValue *val);
bool lua_value_is_tileset(EseLuaValue *val);
bool lua_value_is_uuid(EseLuaValue *val);
bool lua_value_is_vector(EseLuaValue *val);
bool lua_value_is_collision_hit(EseLuaValue *val);
bool lua_value_is_cfunc(EseLuaValue *val);
bool lua_value_is_error(EseLuaValue *val);

/**
 * @brief Recursively frees a EseLuaValue and all associated memory.
 *
 * @details Calls internal reset function to free strings and recursively free
 *          all table items, frees the name field, and finally frees the
 * EseLuaValue structure itself.
 *
 * @param val Pointer to the EseLuaValue to free. Safe to pass NULL.
 *
 * @warning After calling this function, the pointer becomes invalid and must
 * not be used.
 * @warning For table types, this recursively frees all contained items.
 */
void lua_value_destroy(EseLuaValue *val);

/**
 * @brief Outputs a formatted representation of a EseLuaValue structure to debug
 * log.
 *
 * @details Recursively traverses the EseLuaValue structure, formats all values
 * with proper indentation for nested tables, and outputs the result using the
 * log_debug function with "LUA" category.
 *
 * @param val Pointer to the EseLuaValue to log. Safe to pass NULL.
 *
 * @warning Large or deeply nested structures may produce very long log output.
 * @warning Uses a fixed-size internal buffer; extremely large structures may be
 * truncated.
 */
void log_luavalue(EseLuaValue *val);

#endif // LUA_VALUE_H
