#ifndef ESE_LUA_ENGINE_PRIVATE_H
#define ESE_LUA_ENGINE_PRIVATE_H

#include <stdbool.h>
#include <time.h>

// Forward declarations
typedef struct EseHashMap EseHashMap;
typedef struct EseLuaValue EseLuaValue;
typedef struct lua_State lua_State;
typedef struct lua_Debug lua_Debug;

static const char hook_key_sentinel = 0;
#define LUA_HOOK_KEY ((void*)&hook_key_sentinel)
#define LUA_HOOK_FRQ 100
#define LUA_MAX_ALLOC 1024 * 1024 * 5

struct EseLuaValue {
    enum { LUA_VAL_NIL, LUA_VAL_BOOL, LUA_VAL_NUMBER, LUA_VAL_STRING, LUA_VAL_TABLE, LUA_VAL_REF, LUA_VAL_USERDATA } type;
    union {
        bool boolean;
        double number;
        char *string;
        int lua_ref;
        void *userdata;
        struct {
            struct EseLuaValue **items;
            size_t count;
            size_t capacity;
        } table;
    } value;
    char *name;
};

typedef struct LuaFunctionHook {
    clock_t start_time;
    clock_t max_execution_time;
    size_t max_instruction_count;
    size_t instruction_count;
    size_t call_count;
} LuaFunctionHook;

typedef struct EseLuaEngineInternal {
    EseHashMap *functions;
    int sandbox_master_ref;
    size_t memory_limit;
    size_t memory_used;
    clock_t max_execution_time;
    size_t max_instruction_count;
} EseLuaEngineInternal;

void _lua_copy_field(lua_State *L, int src_idx, int dst_idx, const char *k);

int _lua_global_write_error(lua_State *L);

/**
 * @brief Custom memory allocator for Lua with memory limit enforcement.
 *
 * Tracks and limits memory usage for the Lua engine.
 *
 * @param ud User data (EseLuaEngine pointer).
 * @param ptr Pointer to previously allocated block or NULL.
 * @param osize Size of the old block.
 * @param nsize Size of the new block.
 * @return Pointer to the new block, or NULL if allocation fails or exceeds limit.
 * @internal
 */
void* _lua_engine_limited_alloc(void *ud, void *ptr, size_t osize, size_t nsize);

/**
 * @brief Lua debug hook for instruction and time limits.
 *
 * Aborts script execution if instruction count or time limit is exceeded.
 *
 * @param L Lua state.
 * @param ar Debug information (unused).
 * @internal
 */
void _lua_engine_function_hook(lua_State *L, lua_Debug *ar);

/**
 * @brief Gets a function from a Lua instance table or its metatable.
 *
 * Pushes the function onto the stack if found.
 *
 * @param L Lua state.
 * @param instance_ref Registry reference to the instance table.
 * @param func_name Name of the function to retrieve.
 * @return true if the function was found, false otherwise.
 * @internal
 */
bool _lua_engine_instance_get_function(lua_State *L, int instance_ref, const char *func_name);

/**
 * @brief Pushes a EseLuaValue onto the Lua stack.
 *
 * Handles all supported EseLuaValue types, including nested tables.
 *
 * @param L Lua state.
 * @param arg Pointer to the EseLuaValue to push.
 * @internal
 */
void _lua_engine_push_luavalue(lua_State *L, EseLuaValue *arg);

// Shallow clone script_sandbox_master into a new env, leaves env on top of the stack.
void _lua_engine_build_env_from_master(lua_State *L, int master_ref);

#endif // ESE_LUA_ENGINE_PRIVATE_H
