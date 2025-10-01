#ifndef ESE_LUA_ENGINE_PRIVATE_H
#define ESE_LUA_ENGINE_PRIVATE_H

#include <stdbool.h>
#include <time.h>
#include <setjmp.h>

// Forward declarations
typedef struct EseHashMap EseHashMap;
typedef struct EseLuaValue EseLuaValue;
typedef struct lua_State lua_State;
typedef struct lua_Debug lua_Debug;

static const char hook_key_sentinel = 0;
#define LUA_HOOK_KEY ((void*)&hook_key_sentinel)
#define LUA_HOOK_FRQ 10000  // Balance between security (frequent enough) and performance (allows JIT compilation)
#define LUA_MAX_ALLOC 1024 * 1024 * 5

/**
 * @brief Represents a Lua value with type safety and memory management.
 * 
 * @details This structure provides a C representation of Lua values including
 *          nil, boolean, number, string, table, reference, and userdata types.
 *          It supports nested tables and provides memory management for strings
 *          and table contents. Each value can have an optional name for debugging.
 */
struct EseLuaValue {
    enum { LUA_VAL_NIL, LUA_VAL_BOOL, LUA_VAL_NUMBER, LUA_VAL_STRING, LUA_VAL_TABLE, LUA_VAL_REF, LUA_VAL_USERDATA } type; /** Type of the Lua value */
    union {
        bool boolean;                /** Boolean value for LUA_VAL_BOOL type */
        double number;               /** Numeric value for LUA_VAL_NUMBER type */
        char *string;                /** String value for LUA_VAL_STRING type */
        int lua_ref;                 /** Lua registry reference for LUA_VAL_REF type */
        void *userdata;              /** User data pointer for LUA_VAL_USERDATA type */
        struct {
            struct EseLuaValue **items; /** Array of table items */
            size_t count;               /** Number of items in the table */
            size_t capacity;            /** Allocated capacity for items array */
        } table;                    /** Table data for LUA_VAL_TABLE type */
    } value;                        /** Union containing the actual value data */
    char *name;                     /** Optional name for debugging and identification */
};

/**
 * @brief Hook structure for monitoring Lua function execution.
 * 
 * @details This structure tracks execution time, instruction count, and call
 *          frequency for Lua functions. It's used for performance monitoring
 *          and enforcing execution limits.
 */
typedef struct LuaFunctionHook {
    clock_t start_time;             /** Start time of function execution */
    clock_t max_execution_time;     /** Maximum allowed execution time */
    size_t max_instruction_count;   /** Maximum allowed instruction count */
    size_t instruction_count;       /** Current instruction count */
    size_t call_count;              /** Number of times function was called */
} LuaFunctionHook;

/**
 * @brief Internal state and configuration for the Lua engine.
 * 
 * @details This structure manages the Lua engine's internal state including
 *          function registry, sandbox environment, memory limits, and execution
 *          constraints for security and performance.
 */
typedef struct EseLuaEngineInternal {
    EseHashMap *functions;          /** Registry of available Lua functions */
    int sandbox_master_ref;         /** Reference to master sandbox environment */
    size_t memory_limit;            /** Maximum memory usage limit */
    size_t memory_used;             /** Current memory usage */
    clock_t max_execution_time;     /** Maximum execution time limit */
    size_t max_instruction_count;   /** Maximum instruction count limit */
} EseLuaEngineInternal;

char* _replace_colon_calls(const char* prefix, const char* script);

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
