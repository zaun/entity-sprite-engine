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
#include "scripting/lua_value.h"
#include "types/rect.h"
#include "types/point.h"
#include "types/map.h"
#include "types/arc.h"

void *_lua_engine_get_registry_key_from_state(struct lua_State *L, const void *key) {
    log_assert("LUA_ENGINE", L, "_lua_engine_get_registry_key_from_state called with NULL L");
    void *result;
    lua_pushlightuserdata(L, (void *)key);  // push registry key
    lua_gettable(L, LUA_REGISTRYINDEX);     // pushes registry[key]
    result = lua_touserdata(L, -1);         // get lightuserdata
    lua_pop(L, 1);                          // pop the value
    return result;
}

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
        if (strncmp(src, search_pattern, prefix_len + 1) == 0) {
            // Check if this is a function definition line
            const char* line_start = src;
            while (line_start > script && *(line_start - 1) != '\n') {
                line_start--;
            }
            
            // Skip whitespace at start of line
            while (line_start < src && (*line_start == ' ' || *line_start == '\t')) {
                line_start++;
            }
            
            if (strncmp(line_start, "function", 8) == 0) {
                // This is a function definition, skip it
                while (*src && *src != '\n') {
                    if (remaining < 1) break;
                    *dst++ = *src++;
                    remaining--;
                }
            } else {
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
                        *dst++ = ')'; // Add closing parenthesis
                        remaining--;
                        src++; // Skip the closing parenthesis
                    } else {
                        // Has params: PREFIX:func(x) -> PREFIX.func(self, x)
                        memcpy(dst, "self, ", 6);
                        dst += 6;
                        remaining -= 6;
                        
                        // Copy the rest of the parameters and closing parenthesis
                        while (*src && remaining > 0) {
                            *dst++ = *src++;
                            remaining--;
                        }
                    }
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

    if (nsize == 0) {
        // Free
        if (ptr) {
                    // Remove from tracking
        LuaAllocHdr *hdr = (LuaAllocHdr *)((char *)ptr - sizeof(LuaAllocHdr));
        engine->memory_used -= hdr->size;
        memory_manager.free(hdr);
        }
        return NULL;
    }

    if (ptr == NULL) {
        // New allocation
        profile_start(PROFILE_LUA_ENGINE_ALLOC);
        size_t total_size = nsize + sizeof(LuaAllocHdr);
        
        // Check memory limit
        if (engine->memory_used + nsize > engine->memory_limit) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_limit_exceeded");
            log_error("LUA_ENGINE", "Memory limit exceeded: %zu + %zu > %zu", 
                     engine->memory_used, nsize, engine->memory_limit);
            return NULL;
        }
        
        LuaAllocHdr *hdr = memory_manager.malloc(total_size, MMTAG_LUA);
        if (!hdr) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_failed");
            return NULL;
        }
        
        hdr->size = nsize;
        engine->memory_used += nsize;
        
        profile_count_add("lua_eng_alloc_success");
        profile_stop(PROFILE_LUA_ENGINE_ALLOC, "lua_eng_alloc");
        return (char *)hdr + sizeof(LuaAllocHdr);
    } else {
        // Reallocation
        LuaAllocHdr *old_hdr = (LuaAllocHdr *)((char *)ptr - sizeof(LuaAllocHdr));
        size_t old_size = old_hdr->size;
        
        if (nsize <= old_size) {
            // Shrinking or same size
            engine->memory_used -= (old_size - nsize);
            old_hdr->size = nsize;
            return ptr;
        } else {
            profile_start(PROFILE_LUA_ENGINE_ALLOC);
            // Growing
            size_t total_size = nsize + sizeof(LuaAllocHdr);
            
            // Check memory limit
            if (engine->memory_used + (nsize - old_size) > engine->memory_limit) {
                log_error("LUA_ENGINE", "Memory limit exceeded during realloc: %zu + %zu > %zu", 
                         engine->memory_used, nsize - old_size, engine->memory_limit);
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
            engine->memory_used += (nsize - old_size);
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
    profile_start(PROFILE_LUA_ENGINE_ARG_CONVERSION);
    uint64_t arg_start = time_now();

    if (!arg) {
        lua_pushnil(L);
        return;
    }

    EseLuaEngine *engine = (EseLuaEngine *)_lua_engine_get_registry_key_from_state(L, LUA_ENGINE_KEY);

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
        case LUA_VAL_RECT:
            ese_rect_lua_push(engine, arg->value.rect);
            break;
        case LUA_VAL_POINT:
            ese_point_lua_push(engine, arg->value.point);
            break;
        case LUA_VAL_MAP:
            ese_map_lua_push(arg->value.map);
            break;
        case LUA_VAL_ARC: {            
            ese_arc_lua_push(engine, arg->value.arc);
            break;
        }
        case LUA_VAL_CFUNC:
            // For LUA_VAL_CFUNC, push the upvalue first, then the function
            if (arg->value.cfunc_data.upvalue) {
                _lua_engine_push_luavalue(L, arg->value.cfunc_data.upvalue);
            } else {
                lua_pushnil(L);
            }
            lua_pushlightuserdata(L, (void*)arg->value.cfunc_data.cfunc);
            lua_pushcclosure(L, _lua_engine_wrapper, 2);  // 2 upvalues: upvalue + function
            break;
        case LUA_VAL_ERROR:
            // LUA_VAL_ERROR is treated exactly like a string
            lua_pushstring(L, arg->value.string);
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
    if (arg->type == LUA_VAL_TABLE || arg->type == LUA_VAL_REF) {
        profile_stop(PROFILE_LUA_ENGINE_ARG_CONVERSION, "lua_eng_push_luavalue_complex_arg");
        profile_count_add("lua_eng_push_luavalue_complex_arg");
    } else  {
        profile_stop(PROFILE_LUA_ENGINE_ARG_CONVERSION, "lua_eng_push_luavalue_simple_arg");
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

/**
 * @brief Converts a Lua value on the stack to an EseLuaValue structure.
 * 
 * @details This function converts the Lua value at the specified stack index
 *          to an EseLuaValue structure. It handles all basic Lua types including
 *          nil, boolean, number, string, table, and userdata.
 * 
 * @param engine EseLuaEngine pointer
 * @param idx Stack index of the Lua value to convert
 * @param out_result Pointer to EseLuaValue to store the converted result
 * 
 * @warning The caller is responsible for freeing the out_result when done.
 */
void _lua_engine_convert_stack_to_luavalue(lua_State *L, int idx, EseLuaValue *out_result) {
    log_assert("LUA_ENGINE", L, "lua_eng_convert_stack_to_luavalue called with invalid L");
    if (!out_result) return;
    
    // Reset the output value
    lua_value_set_nil(out_result);
    
    EseLuaEngine *engine = (EseLuaEngine *)_lua_engine_get_registry_key_from_state(L, LUA_ENGINE_KEY);
    
    // Handle negative indices by converting to positive
    int abs_idx = idx;
    if (idx < 0) {
        abs_idx = lua_gettop(L) + idx + 1;
    }
    
    if (lua_isnil(L, abs_idx)) {
        lua_value_set_nil(out_result);
    } else if (lua_isboolean(L, abs_idx)) {
        lua_value_set_bool(out_result, lua_toboolean(L, abs_idx));
    } else if (lua_isnumber(L, abs_idx)) {
        lua_value_set_number(out_result, lua_tonumber(L, abs_idx));
    } else if (lua_isstring(L, abs_idx)) {
        lua_value_set_string(out_result, lua_tostring(L, abs_idx));
    } else if (lua_istable(L, abs_idx)) {
        lua_value_set_table(out_result);
        
        // Get table size using lua_objlen (Lua 5.1) or lua_rawlen (Lua 5.2+)
        size_t table_size = 0;
        #if LUA_VERSION_NUM >= 502
            table_size = lua_rawlen(L, abs_idx);
        #else
            table_size = lua_objlen(L, abs_idx);
        #endif
        
        // Iterate through table and add items
        lua_pushnil(L); // First key
        while (lua_next(L, abs_idx) != 0) {
            // Key is at index -2, value at index -1
            EseLuaValue *item = lua_value_create_nil(NULL);
            _lua_engine_convert_stack_to_luavalue(L, -1, item);
            
            // Add to table
            lua_value_push(out_result, item, false); // false = take ownership
            
            lua_pop(L, 1); // Remove value, keep key for next iteration
        }
    } else if (lua_isuserdata(L, abs_idx)) {
        EseRect *rect = ese_rect_lua_get(engine, abs_idx);
        if (rect) {
            lua_value_set_rect(out_result, rect);
            return;
        }

        EseMap *map = ese_map_lua_get(L, abs_idx);
        if (map) {
            lua_value_set_map(out_result, map);
            return;
        }

        EseLuaEngine *engine = (EseLuaEngine *)_lua_engine_get_registry_key_from_state(L, LUA_ENGINE_KEY);
        EseArc *arc = ese_arc_lua_get(engine, abs_idx);
        if (arc) {
            lua_value_set_arc(out_result, arc);
            return;
        }

        lua_value_set_userdata(out_result, lua_touserdata(L, abs_idx));
    } else if (lua_isfunction(L, abs_idx)) {
        // For Lua functions, we can't convert back to EseLuaCFunction
        // since we don't know which C function they correspond to
        lua_value_set_nil(out_result);
    } else {
        // For other types (thread, etc.), just set as nil
        lua_value_set_nil(out_result);
    }
}

int lua_isinteger_lj(lua_State *L, int idx) {
    log_assert("LUA_ENGINE", L, "lua_isinteger_lj called with NULL Lua state");
    
    if (!lua_isnumber(L, idx)) return 0;
    lua_Number n = lua_tonumber(L, idx);
    lua_Integer i = (lua_Integer)n;
    return (n == (lua_Number)i);
}

// Unique key for registry storage
static const char *LUA_EXTRASPACE_KEY = "lua_extraspace_lj";
void *lua_getextraspace_lj(lua_State *L) {
    log_assert("LUA_ENGINE", L, "lua_getextraspace_lj called with NULL Lua state");
    
    void *p;

    // Check if already allocated
    lua_pushlightuserdata(L, (void*)LUA_EXTRASPACE_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    p = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!p) {
        // Allocate space (just sizeof(void*), like Lua 5.3 guarantees)
        p = lua_newuserdata(L, sizeof(void*));
        *(void**)p = NULL; // initialize to NULL

        // Store in registry
        lua_pushlightuserdata(L, (void*)LUA_EXTRASPACE_KEY);
        lua_pushvalue(L, -2); // copy userdata
        lua_settable(L, LUA_REGISTRYINDEX);

        lua_pop(L, 1); // pop userdata
    }

    return p;
}

void _lua_engine_add_function(EseLuaEngine *engine, const char *function_name, lua_CFunction func) {
    log_assert("LUA_ENGINE", engine, "lua_eng_add_function called with NULL engine");
    log_assert("LUA_ENGINE", function_name, "lua_eng_add_function called with NULL function_name");
    log_assert("LUA_ENGINE", func, "lua_eng_add_function called with NULL func");
    log_assert("LUA_ENGINE", engine->sandbox_master_ref != LUA_NOREF, "lua_eng_add_function engine->sandbox_master_ref is LUA_NOREF");

    // Add to script_sandbox_master
    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, engine->sandbox_master_ref); // master
    log_assert("LUA_ENGINE", engine->L, "lua_eng_add_function called with invalid engine");
    lua_pushcfunction(engine->L, func);
    lua_setfield(engine->L, -2, function_name);
    lua_pop(engine->L, 1); // master

    log_debug("LUA_ENGINE", "Added C function '%s' to Lua.", function_name);
}

// Internal wrapper function that converts EseLuaEngine* to lua_State*
int _lua_engine_wrapper(lua_State *L) {
    // Get the EseLuaEngine from registry
    EseLuaEngine *engine = (EseLuaEngine *)_lua_engine_get_registry_key_from_state(L, LUA_ENGINE_KEY);
    
    if (!engine) {
        luaL_error(L, "Internal error: engine not found in registry");
        return 0;
    }
    
    // Get the user function from upvalue (stored when creating the wrapper)
    EseLuaCFunction user_func = (EseLuaCFunction)lua_touserdata(L, lua_upvalueindex(2));
    
    if (!user_func) {
        luaL_error(L, "Internal error: user function not found in upvalue");
        return 0;
    }
    
    // Convert Lua stack arguments to EseLuaValue array
    int argc = lua_gettop(L);
    EseLuaValue **argv = NULL;
    
    if (argc > 0) {
        argv = (EseLuaValue**)malloc(argc * sizeof(EseLuaValue*));
        if (!argv) {
            luaL_error(L, "Memory allocation failed for argument array");
            return 0;
        }
        
        for (int i = 0; i < argc; i++) {
            argv[i] = lua_value_create_nil("arg");
            if (!argv[i]) {
                // Clean up previously allocated values
                for (int j = 0; j < i; j++) {
                    lua_value_destroy(argv[j]);
                }
                free(argv);
                luaL_error(L, "Memory allocation failed for argument %d", i);
                return 0;
            }
            _lua_engine_convert_stack_to_luavalue(L, i + 1, argv[i]);
        }
    }
    
    // Call the user function with EseLuaEngine* and arguments
    EseLuaValue *result = user_func(engine, argc, argv);
    
    // Clean up argument array
    if (argv) {
        for (int i = 0; i < argc; i++) {
            lua_value_destroy(argv[i]);
        }
        free(argv);
    }
    
    if (result) {
        // Check if the result is an error
        if (result->type == LUA_VAL_ERROR) {
            // Call luaL_error with the error message
            luaL_error(L, "%s", result->value.string);
            // This will never return, but we need to free the result
            lua_value_destroy(result);
            return 0; // This line will never be reached
        }
        
        // Push the EseLuaValue to the Lua stack
        _lua_engine_push_luavalue(L, result);
        // Free the result since we've pushed it to the stack
        lua_value_destroy(result);
        return 1; // Return 1 value to Lua
    } else {
        // No return value
        return 0;
    }
}
