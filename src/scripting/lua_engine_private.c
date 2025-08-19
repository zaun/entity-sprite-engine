#include <string.h>
#include "utility/log.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine_private.h"
#include "scripting/lua_engine.h"

typedef struct LuaAllocHdr {
    size_t size; // user-visible size (bytes requested by Lua)
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
                
                // Skip whitespace after (
                src++;
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
    // Never trust or use osize
    (void)osize;

    log_assert("LUA", ud, "_lua_engine_limited_alloc called with NULL ud");
    EseLuaEngine *engine = (EseLuaEngine *)ud;
    if (!engine) return NULL;

    // Free path
    if (nsize == 0) {
        if (ptr) {
            LuaAllocHdr *hdr = alloc_get_hdr(ptr);
            size_t old_size = hdr ? hdr->size : 0;

            if (engine->internal->memory_used >= old_size) {
                engine->internal->memory_used -= old_size;
            } else {
                engine->internal->memory_used = 0;
            }

            memory_manager.free(hdr);
        }
        return NULL;
    }

    // Reject unreasonably large allocations (user-visible)
    if (nsize > LUA_MAX_ALLOC) {
        return NULL;
    }

    LuaAllocHdr *old_hdr = alloc_get_hdr(ptr);
    size_t old_size = old_hdr ? old_hdr->size : 0;

    // Compute new memory_used with overflow/limit checks
    size_t new_used = engine->internal->memory_used;
    if (nsize >= old_size) {
        size_t growth = nsize - old_size;

        if (growth > 0) {
            if (new_used > SIZE_MAX - growth) {
                return NULL;
            }
            new_used += growth;
            if (new_used > engine->internal->memory_limit) {
                return NULL;
            }
        }
    } else {
        size_t shrink = old_size - nsize;
        if (new_used >= shrink) {
            new_used -= shrink;
        } else {
            new_used = 0;
        }
    }

    // Allocate/reallocate including header
    size_t total_bytes = nsize + sizeof(LuaAllocHdr);
    LuaAllocHdr *new_hdr = NULL;

    if (!ptr) {
        // malloc
        new_hdr = (LuaAllocHdr *)memory_manager.malloc(total_bytes, MMTAG_LUA_SCRIPT);
        if (!new_hdr) return NULL;
    } else {
        // realloc
        new_hdr = (LuaAllocHdr *)memory_manager.realloc(old_hdr, total_bytes, MMTAG_LUA_SCRIPT);
        if (!new_hdr) return NULL;
    }

    // Update header and accounting
    new_hdr->size = nsize;
    engine->internal->memory_used = new_used;

    // Return pointer to user area
    return (void *)(new_hdr + 1);
}

void _lua_engine_function_hook(lua_State *L, lua_Debug *ar) {
    log_assert("LUA", L, "_lua_engine_function_hook called with NULL L");
    log_assert("LUA", ar, "_lua_engine_function_hook called with NULL ar");

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
        log_debug("LUA_ENGINE", "LUA_HOOK_FRQ = %d", LUA_HOOK_FRQ);
        log_debug("LUA_ENGINE", "call count = %d", hook->call_count);
        log_debug("LUA_ENGINE", "Instruction Count... current: %zu max: %zu", hook->instruction_count, hook->max_instruction_count);
        luaL_error(L, "Instruction count limit exceeded");
    }

    if (current - hook->start_time > hook->max_execution_time) {
        luaL_error(L, "Script execution timeout");
    }
}

bool _lua_engine_instance_get_function(lua_State *L, int instance_ref, const char *func_name) {
    log_assert("LUA", L, "_lua_engine_instance_get_function called with NULL L");
    log_assert("LUA", func_name, "_lua_engine_instance_get_function called with NULL func_name");

    // Push the instance table onto the stack
    lua_rawgeti(L, LUA_REGISTRYINDEX, instance_ref);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    // Try to get the function from the instance table
    lua_getfield(L, -1, func_name);

    if (lua_isfunction(L, -1)) {
        lua_remove(L, -2); // remove instance, leave function
        // Stack: [function]
        return true;
    }

    // Not found in instance, try metatable's __index
    lua_pop(L, 1); // pop nil

    if (!lua_getmetatable(L, -1)) { // push metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    lua_getfield(L, -1, "__index"); // push __index

    if (!lua_istable(L, -1)) {
        lua_pop(L, 2); // pop __index, metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    lua_getfield(L, -1, func_name); // push method from __index

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 3); // pop nil, __index, metatable
        lua_pop(L, 1); // pop instance table
        return false;
    }

    // Success: Stack is [instance][metatable][__index][function]
    // Remove __index, metatable, and instance, leave [function]
    lua_remove(L, -2); // remove __index
    lua_remove(L, -2); // remove metatable
    lua_remove(L, -2); // remove instance

    // Stack: [function]
    return true;
}

void _lua_engine_push_luavalue(lua_State *L, EseLuaValue *arg) {
    log_assert("LUA", L, "_lua_engine_push_luavalue called with NULL L");

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
