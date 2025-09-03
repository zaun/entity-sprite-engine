#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include "utility/log.h"
#include "utility/profile.h"
#include "utility/hashmap.h"
#include "utility/array.h"
#include "utility/profile.h"
#include "core/memory_manager.h"
#include "platform/time.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_engine.h"

/**
 * @brief Header structure for Lua memory allocations.
 * 
 * @details This structure is prepended to every memory block allocated
 *          by Lua to track the user-visible size requested by Lua scripts.
 *          It's used for memory accounting and debugging purposes.
 */
typedef struct LuaAllocHdr {
    size_t size;                    /**< User-visible size in bytes (requested by Lua) */
} LuaAllocHdr;

char* _replace_colon_calls(const char* prefix, const char* script) {
    size_t script_len = strlen(script);
    size_t prefix_len = strlen(prefix);
    size_t buffer_size = script_len * 2; // Generous buffer
    char* result = memory_manager.malloc(buffer_size, MMTAG_LUA);
    
    const char* src = script;
    char* dst = result;
    size_t remaining = buffer_size - 1;
    
    // Create search pattern "PREFIX:"
    char* search_pattern = memory_manager.malloc(prefix_len + 2, MMTAG_LUA);
    snprintf(search_pattern, prefix_len + 2, "%s:", prefix);
    
    while (*src) {
        // Check if this is a function definition line
        if (strncmp(src, "function", 8) == 0 && (src == script || *(src-1) == '\n' || *(src-1) == ' ' || *(src-1) == '\t')) {
            // Look for the function name pattern
            const char* func_start = src + 8;
            while (*func_start && (*func_start == ' ' || *func_start == '\t')) func_start++;
            
            // Check if this is a function definition with our prefix
            if (strncmp(func_start, search_pattern, prefix_len + 1) == 0) {
                // This is a function definition, don't replace it
                // Copy the entire function definition line
                while (*src && *src != '\n') {
                    if (remaining < 1) break;
                    *dst++ = *src++;
                    remaining--;
                }
                // Copy the newline
                if (*src == '\n' && remaining > 0) {
                    *dst++ = *src++;
                    remaining--;
                }
                continue;
            }
        }
        
        if (strncmp(src, search_pattern, prefix_len + 1) == 0) {
            // Found PREFIX:, replace with PREFIX.
            if (remaining < prefix_len + 1) break;
            memcpy(dst, prefix, prefix_len);
            dst += prefix_len;
            *dst++ = '.';
            remaining -= (prefix_len + 1);
            src += (prefix_len + 1);
            
            // Skip whitespace and find opening paren
            while (*src && (*src == ' ' || *src == '\t')) {
                if (remaining < 1) break;
                *dst++ = *src++;
                remaining--;
            }
            
            // Copy function name until (
            while (*src && *src != '(') {
                if (remaining < 1) break;
                *dst++ = *src++;
                remaining--;
            }
            
            // Add opening paren and self
            if (*src == '(' && remaining >= 6) {
                *dst++ = '(';
                remaining--;
                src++; // Advance past the opening parenthesis
                
                // Skip whitespace after (
                while (*src && (*src == ' ' || *src == '\t')) {
                    if (remaining < 1) break;
                    *dst++ = *src++;
                    remaining--;
                }
                
                // Add self
                if (*src == ')') {
                    // Empty params: PREFIX:func() -> PREFIX.func(self)
                    memcpy(dst, "self", 4);
                    dst += 4;
                    remaining -= 4;
                } else {
                    // Has params: PREFIX:func(x) -> PREFIX.func(self, x)
                    memcpy(dst, "self, ", 6);
                    dst += 6;
                    remaining -= 6;
                }
                
                // Copy the rest of the parameters and closing parenthesis
                while (*src && remaining > 0) {
                    *dst++ = *src++;
                    remaining--;
                }
            }
        } else {
            if (remaining < 1) break;
            *dst++ = *src++;
            remaining--;
        }
    }
    
    *dst = '\0';
    memory_manager.free(search_pattern);
    return result;
}

void _lua_copy_field(lua_State *L, int src_idx, int dst_idx, const char *k) {
    if (src_idx < 0) src_idx = lua_gettop(L) + 1 + src_idx;
    if (dst_idx < 0) dst_idx = lua_gettop(L) + 1 + dst_idx;
    lua_getfield(L, src_idx, k);
    if (!lua_isnil(L, -1)) {
        lua_setfield(L, dst_idx, k);
    } else {
        lua_pop(L, 1);
    }
}

int _lua_global_write_error(lua_State *L) {
    log_assert("LUA", L, "_lua_global_write_error called with NULL L");

    const char *key = lua_tostring(L, 2);
    log_debug("LUA_ENGINE", "Global variable writes are not allowed (attempted to set '%s')", key ? key : "?");
    return 0;
}


static inline LuaAllocHdr *alloc_get_hdr(void *user_ptr) {
    return user_ptr ? ((LuaAllocHdr *)user_ptr - 1) : NULL;
}

void* _lua_engine_limited_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    // PROFILING: Start timing for allocation
    uint64_t alloc_start = time_now();
    
    EseLuaEngine *engine = (EseLuaEngine *)ud;
    EseLuaEngineInternal *internal = engine->internal;

    if (nsize == 0) {
        // Free
        if (ptr) {
                    // Remove from tracking
        LuaAllocHdr *hdr = (LuaAllocHdr *)((char *)ptr - sizeof(LuaAllocHdr));
        internal->memory_used -= hdr->size;
        memory_manager.free(hdr);
        }
        return NULL;
    }

    if (ptr == NULL) {
        // New allocation
        profile_start(PROFILE_LUA_ENGINE_ALLOC);
        size_t total_size = nsize + sizeof(LuaAllocHdr);
        
        // Check memory limit
        if (internal->memory_used + nsize > internal->memory_limit) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_limit_exceeded");
            log_error("LUA_ENGINE", "Memory limit exceeded: %zu + %zu > %zu", 
                     internal->memory_used, nsize, internal->memory_limit);
            return NULL;
        }
        
        LuaAllocHdr *hdr = memory_manager.malloc(total_size, MMTAG_LUA);
        if (!hdr) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_failed");
            return NULL;
        }
        
        hdr->size = nsize;
        internal->memory_used += nsize;
        
        profile_count_add("lua_eng_alloc_success");
        profile_stop(PROFILE_LUA_ENGINE_ALLOC, "lua_eng_alloc");
        return (char *)hdr + sizeof(LuaAllocHdr);
    } else {
        // Reallocation
        LuaAllocHdr *old_hdr = (LuaAllocHdr *)((char *)ptr - sizeof(LuaAllocHdr));
        size_t old_size = old_hdr->size;
        
        if (nsize <= old_size) {
            // Shrinking or same size
            internal->memory_used -= (old_size - nsize);
            old_hdr->size = nsize;
            return ptr;
        } else {
            profile_start(PROFILE_LUA_ENGINE_ALLOC);
            // Growing
            size_t total_size = nsize + sizeof(LuaAllocHdr);
            
            // Check memory limit
            if (internal->memory_used + (nsize - old_size) > internal->memory_limit) {
                log_error("LUA_ENGINE", "Memory limit exceeded during realloc: %zu + %zu > %zu", 
                         internal->memory_used, nsize - old_size, internal->memory_limit);
                profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
                profile_count_add("lua_eng_realloc_limit_exceeded");
                return NULL;
            }
            
            LuaAllocHdr *new_hdr = memory_manager.malloc(total_size, MMTAG_LUA);
            if (!new_hdr) {
                profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
                profile_count_add("lua_eng_realloc_failed");
                return NULL;
            }
            
            // Copy data
            memcpy((char *)new_hdr + sizeof(LuaAllocHdr), ptr, old_size);
            
            // Update tracking
            internal->memory_used += (nsize - old_size);
            new_hdr->size = nsize;
            
            // Free old allocation
            memory_manager.free(old_hdr);
            
            profile_count_add("lua_eng_realloc_success");
            profile_stop(PROFILE_LUA_ENGINE_ALLOC, "lua_eng_realloc");
            return (char *)new_hdr + sizeof(LuaAllocHdr);
        }
    }
}

void _lua_engine_function_hook(lua_State *L, lua_Debug *ar) {
    // Start timing for hook execution
    profile_start(PROFILE_LUA_ENGINE_HOOK_SETUP);
    
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_HOOK_KEY);
    LuaFunctionHook *hook = (LuaFunctionHook *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!hook) {
        luaL_error(L, "Internal error: hook data missing");
    }

    clock_t current = clock();
    hook->instruction_count += LUA_HOOK_FRQ;
    hook->call_count++;

    if (hook->instruction_count > hook->max_instruction_count) {
        profile_cancel(PROFILE_LUA_ENGINE_HOOK_SETUP);
        profile_count_add("lua_eng_hook_instruction_limit_exceeded");
        luaL_error(L, "Instruction count limit exceeded");
    }

    if (current - hook->start_time > hook->max_execution_time) {
        profile_cancel(PROFILE_LUA_ENGINE_HOOK_SETUP);
        profile_count_add("lua_eng_hook_timeout_exceeded");
        luaL_error(L, "Script execution timeout");
    }
    
    // Log hook execution timing (only every 1000 calls to avoid spam)
    if (hook->call_count % 1000 == 0) {
        profile_stop(PROFILE_LUA_ENGINE_HOOK_SETUP, "lua_eng_hook_execution");
        profile_count_add("lua_eng_hook_execution_completed");
        log_debug("LUA", "Hook execution (call %d): completed", hook->call_count);
    }
}

bool _lua_engine_instance_get_function(lua_State *L, int instance_ref, const char *func_name) {
    // Start timing for function lookup
    profile_start(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
    
    log_debug("LUA", "=== Function lookup debug for '%s' ===", func_name);
    log_debug("LUA", "Stack size before lookup: %d", lua_gettop(L));
    
    // Push the instance table onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, instance_ref);
    log_debug("LUA", "Stack size after pushing instance: %d", lua_gettop(L));
    log_debug("LUA", "Instance type: %s", lua_typename(L, lua_type(L, -1)));

    if (!lua_istable(L, -1)) {
        log_debug("LUA", "Instance is not a table (type: %s)", lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
        profile_cancel(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
        profile_count_add("lua_eng_inst_get_func_not_table");
        return false;
    }

    // Try to get the function from the instance table
    log_debug("LUA", "Looking for function '%s' in instance table", func_name);
    lua_getfield(L, -1, func_name);
    log_debug("LUA", "Stack size after lua_getfield: %d", lua_gettop(L));
    log_debug("LUA", "Found value type: %s", lua_typename(L, lua_type(L, -1)));

    if (lua_isfunction(L, -1)) {
        log_debug("LUA", "Found function directly in instance table");
        log_debug("LUA", "Function value type: %s", lua_typename(L, lua_type(L, -1)));
        
        // The stack is [instance][function]
        // We want to keep [function] and remove [instance]
        lua_remove(L, -2); // Remove instance table, leaving function on top
        
        // Verify we have the function on top
        log_debug("LUA", "After removing instance, stack size: %d", lua_gettop(L));
        log_debug("LUA", "Top of stack type: %s", lua_typename(L, lua_type(L, -1)));
        
        // Log successful direct lookup timing
        profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_inst_get_func_direct");
        profile_count_add("lua_eng_inst_get_func_direct_success");
        return true;
    }

    // Not found in instance, try metatable's __index
    log_debug("LUA", "Function '%s' not found directly in instance (found type: %s), trying metatable", func_name, lua_typename(L, lua_type(L, -1)));
    
    // Log the actual value found for debugging
    if (lua_isstring(L, -1)) {
        log_debug("LUA", "Found string value: %s", lua_tostring(L, -1));
    } else if (lua_isnumber(L, -1)) {
        log_debug("LUA", "Found number value: %g", lua_tonumber(L, -1));
    } else if (lua_istable(L, -1)) {
        log_debug("LUA", "Found table value");
    } else if (lua_isboolean(L, -1)) {
        log_debug("LUA", "Found boolean value: %s", lua_toboolean(L, -1) ? "true" : "false");
    } else if (lua_isnil(L, -1)) {
        log_debug("LUA", "Found nil value");
    } else {
        log_debug("LUA", "Found other value type: %s", lua_typename(L, lua_type(L, -1)));
    }
    
    // Pop the value we found
    lua_pop(L, 1);
    
    // Try to get the metatable
    if (lua_getmetatable(L, -1)) {
        log_debug("LUA", "Instance has metatable, checking __index");
        
        // Check if metatable has __index
        lua_getfield(L, -1, "__index");
        if (lua_isfunction(L, -1)) {
            log_debug("LUA", "Metatable has __index function");
            
            // Call the __index function with instance and function name
            lua_pushvalue(L, -3); // Push instance table
            lua_pushstring(L, func_name); // Push function name
            
            if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
                if (lua_isfunction(L, -1)) {
                    log_debug("LUA", "Found function via metatable __index");
                    
                    // Clean up: remove metatable and instance, keep function
                    lua_remove(L, -3); // Remove metatable
                    lua_remove(L, -2); // Remove instance
                    
                    profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_inst_get_func_metatable");
                    profile_count_add("lua_eng_inst_get_func_metatable_success");
                    return true;
                } else {
                    log_debug("LUA", "__index returned non-function: %s", lua_typename(L, lua_type(L, -1)));
                    lua_pop(L, 1); // Pop the result
                }
            } else {
                log_debug("LUA", "Error calling __index: %s", lua_tostring(L, -1));
                lua_pop(L, 1); // Pop error message
            }
        } else if (lua_istable(L, -1)) {
            log_debug("LUA", "Metatable has __index table");
            
            // Try to get the function from the __index table
            lua_pushstring(L, func_name);
            lua_gettable(L, -2);
            
            if (lua_isfunction(L, -1)) {
                log_debug("LUA", "Found function via metatable __index table");
                
                // Clean up: remove metatable and instance, keep function
                lua_remove(L, -3); // Remove metatable
                lua_remove(L, -2); // Remove instance
                
                profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP, "lua_eng_inst_get_func_metatable_table");
                profile_count_add("lua_eng_inst_get_func_metatable_table_success");
                return true;
            } else {
                log_debug("LUA", "__index table lookup failed, got: %s", lua_typename(L, lua_type(L, -1)));
                lua_pop(L, 1); // Pop the result
            }
        } else {
            log_debug("LUA", "Metatable __index is not function or table: %s", lua_typename(L, lua_type(L, -1)));
        }
        
        // Clean up metatable
        lua_pop(L, 1);
    } else {
        log_debug("LUA", "Instance has no metatable");
    }
    
    // Clean up instance table
    lua_pop(L, 1);
    
    log_debug("LUA", "Function '%s' not found in instance or metatable", func_name);
    profile_cancel(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP);
    profile_count_add("lua_eng_inst_get_func_not_found");
    return false;
}

void _lua_engine_push_luavalue(lua_State *L, EseLuaValue *arg) {
    log_assert("LUA", L, "_lua_engine_push_luavalue called with NULL L");

    // PROFILING: Start timing for this argument
    uint64_t arg_start = time_now();

    if (!arg) {
        lua_pushnil(L);
        return;
    }

    switch (arg->type) {
        case LUA_VAL_NIL:
            lua_pushnil(L);
            break;
        case LUA_VAL_BOOL:
            lua_pushboolean(L, arg->value.boolean);
            break;
        case LUA_VAL_NUMBER:
            lua_pushnumber(L, arg->value.number);
            break;
        case LUA_VAL_STRING:
            lua_pushstring(L, arg->value.string ? arg->value.string : "");
            break;
        case LUA_VAL_USERDATA:
            lua_pushlightuserdata(L, arg->value.userdata);
            break;
        case LUA_VAL_TABLE: {
            lua_newtable(L);
            if (arg->value.table.items && arg->value.table.count > 0) {
                int array_index = 1;
                for (size_t i = 0; i < arg->value.table.count; ++i) {
                    EseLuaValue *field = arg->value.table.items[i];
                    // No need to check for NULL unless you allow NULL slots
                    _lua_engine_push_luavalue(L, field);
                    if (field->name && field->name[0] != '\0') {
                        lua_setfield(L, -2, field->name);
                    } else {
                        lua_rawseti(L, -2, array_index++);
                    }
                }
            }
            break;
        }
        case LUA_VAL_REF:
            if(arg->value.lua_ref != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, arg->value.lua_ref);    
            }
            break;
        default:
            lua_pushnil(L);
            break;
    }

    // PROFILING: Log timing for complex types
    uint64_t arg_time = time_now() - arg_start;
    if (arg->type == LUA_VAL_TABLE || arg->type == LUA_VAL_REF) {
        log_debug("LUA", "Complex arg conversion (type %d): %.3fμs", arg->type, (double)arg_time / 1000.0);
    }
}

// Shallow clone script_sandbox_master into a new env, leaves env on top of the stack.
void _lua_engine_build_env_from_master(lua_State *L, int master_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, master_ref); // [..., master]
    int master_idx = lua_gettop(L);

    lua_newtable(L);                               // [..., master, env]
    int env_idx = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, master_idx) != 0) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_settable(L, env_idx);
        lua_pop(L, 1);
    }

    // env._G = env
    lua_pushvalue(L, env_idx);
    lua_setfield(L, env_idx, "_G");

    lua_remove(L, master_idx);                      // [..., env]
}
