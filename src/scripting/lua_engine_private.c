#include "scripting/lua_engine_private.h"
#include "core/memory_manager.h"
#include "platform/time.h"
#include "scripting/lua_engine.h"
#include "types/types.h"
#include "utility/array.h"
#include "utility/hashmap.h"
#include "utility/log.h"
#include "utility/profile.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t LUA_HDR_MAGIC = 0xD15EA5E5C0FFEE01ULL;
static const uint64_t LUA_TAIL_CANARY = 0xA11C0FFEEA11C0DEULL;

/**
 * @brief Header structure for Lua memory allocations.
 *
 * @details This structure is prepended to every memory block allocated
 *          by Lua to track the user-visible size requested by Lua scripts.
 *          It's used for memory accounting and debugging purposes.
 */
typedef struct LuaAllocHdr {
    size_t size;  /** User-visible size in bytes (requested by Lua) */
    uint64_t pad; /** 8-byte padding to make the header 16 bytes total */
} LuaAllocHdr;

/* static_assert to enforce 16-byte header size at compile time */
_Static_assert(sizeof(LuaAllocHdr) == 16, "LuaAllocHdr must be 16 bytes for alignment");

char *_replace_colon_calls(const char *prefix, const char *script) {
    size_t script_len = strlen(script);
    size_t prefix_len = strlen(prefix);
    size_t buffer_size = script_len * 2; // Generous buffer
    char *result = memory_manager.malloc(buffer_size, MMTAG_LUA);

    const char *src = script;
    char *dst = result;
    size_t remaining = buffer_size - 1;

    // Create search pattern "PREFIX:"
    char *search_pattern = memory_manager.malloc(prefix_len + 2, MMTAG_LUA);
    snprintf(search_pattern, prefix_len + 2, "%s:", prefix);

    while (*src) {
        if (strncmp(src, search_pattern, prefix_len + 1) == 0) {
            // Check if this is a function definition line
            const char *line_start = src;
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
                    if (remaining < 1)
                        break;
                    *dst++ = *src++;
                    remaining--;
                }
            } else {
                // Found PREFIX:, replace with PREFIX.
                if (remaining < prefix_len + 1)
                    break;
                memcpy(dst, prefix, prefix_len);
                dst += prefix_len;
                *dst++ = '.';
                remaining -= (prefix_len + 1);
                src += (prefix_len + 1);

                // Skip whitespace and find opening paren
                while (*src && (*src == ' ' || *src == '\t')) {
                    if (remaining < 1)
                        break;
                    *dst++ = *src++;
                    remaining--;
                }

                // Copy function name until (
                while (*src && *src != '(') {
                    if (remaining < 1)
                        break;
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
                        if (remaining < 1)
                            break;
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

                        // Copy the rest of the parameters and closing
                        // parenthesis
                        while (*src && remaining > 0) {
                            *dst++ = *src++;
                            remaining--;
                        }
                    }
                }
            }
        } else {
            if (remaining < 1)
                break;
            *dst++ = *src++;
            remaining--;
        }
    }

    *dst = '\0';
    memory_manager.free(search_pattern);
    return result;
}

void _lua_copy_field(lua_State *L, int src_idx, int dst_idx, const char *k) {
    if (src_idx < 0)
        src_idx = lua_gettop(L) + 1 + src_idx;
    if (dst_idx < 0)
        dst_idx = lua_gettop(L) + 1 + dst_idx;
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
    log_debug("LUA_ENGINE", "Global variable writes are not allowed (attempted to set '%s')",
              key ? key : "?");
    return 0;
}

static inline LuaAllocHdr *lua_hdr_from_user(void *user_ptr) {
    return (LuaAllocHdr *)((char *)user_ptr - sizeof(LuaAllocHdr));
}

static inline uint64_t *lua_tail_from_hdr(LuaAllocHdr *hdr) {
    return (uint64_t *)((char *)hdr + sizeof(LuaAllocHdr) + hdr->size);
}

static inline int lua_hdr_valid(LuaAllocHdr *hdr, size_t mem_limit) {
    if (!hdr)
        return 0;
    if (hdr->size > mem_limit)
        return 0;
    if (hdr->pad != LUA_HDR_MAGIC)
        return 0; /* pad field used as magic */
    uint64_t *tail = lua_tail_from_hdr(hdr);
    return (*tail == LUA_TAIL_CANARY);
}

/* Replace your function with this exact version. */
void *_lua_engine_limited_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    uint64_t alloc_start = time_now();
    EseLuaEngine *engine = (EseLuaEngine *)ud;
    EseLuaEngineInternal *internal = engine->internal;

    if (nsize == 0) {
        if (ptr) {
            LuaAllocHdr *hdr = lua_hdr_from_user(ptr);
            if (!lua_hdr_valid(hdr, internal->memory_limit)) {
                log_error("LUA_ALLOC", "free(): header/canary invalid for %p", ptr);
                abort();
            }

            if (internal->memory_used >= hdr->size) {
                internal->memory_used -= hdr->size;
            } else {
                internal->memory_used = 0;
            }
            memory_manager.free((void *)hdr);
        }
        return NULL;
    }

    if (ptr == NULL) {
        profile_start(PROFILE_LUA_ENGINE_ALLOC);

        if (internal->memory_used + nsize > internal->memory_limit) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_limit_exceeded");
            log_error("LUA_ENGINE", "Memory limit exceeded: %zu + %zu > %zu", internal->memory_used,
                      nsize, internal->memory_limit);
            return NULL;
        }

        size_t total_size = sizeof(LuaAllocHdr) + nsize + sizeof(uint64_t);
        LuaAllocHdr *hdr = (LuaAllocHdr *)memory_manager.malloc(total_size, MMTAG_LUA);
        if (!hdr) {
            profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
            profile_count_add("lua_eng_alloc_failed");
            return NULL;
        }

        hdr->size = nsize;
        hdr->pad = LUA_HDR_MAGIC; /* set magic */
        uint64_t *tail = lua_tail_from_hdr(hdr);
        *tail = LUA_TAIL_CANARY;

        internal->memory_used += nsize;

        profile_count_add("lua_eng_alloc_success");
        profile_stop(PROFILE_LUA_ENGINE_ALLOC, "lua_eng_alloc");
        return (char *)hdr + sizeof(LuaAllocHdr);
    }

    /* Realloc path */
    LuaAllocHdr *old_hdr = lua_hdr_from_user(ptr);
    if (!lua_hdr_valid(old_hdr, internal->memory_limit)) {
        log_error("LUA_ALLOC", "realloc(): header/canary invalid for %p", ptr);
        abort();
    }

    size_t old_size = old_hdr->size;

    if (nsize <= old_size) {
        /* Shrink in place: update size and tail canary */
        size_t delta = old_size - nsize;
        if (internal->memory_used >= delta) {
            internal->memory_used -= delta;
        } else {
            internal->memory_used = 0;
        }
        old_hdr->size = nsize;
        uint64_t *new_tail = lua_tail_from_hdr(old_hdr);
        *new_tail = LUA_TAIL_CANARY;
        return ptr;
    }

    /* Grow */
    profile_start(PROFILE_LUA_ENGINE_ALLOC);

    size_t grow = nsize - old_size;
    if (internal->memory_used + grow > internal->memory_limit) {
        log_error("LUA_ALLOC", "realloc limit exceeded: %zu + %zu > %zu", internal->memory_used,
                  grow, internal->memory_limit);
        profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
        profile_count_add("lua_eng_realloc_limit_exceeded");
        return NULL;
    }

    size_t total_size = sizeof(LuaAllocHdr) + nsize + sizeof(uint64_t);
    LuaAllocHdr *new_hdr = (LuaAllocHdr *)memory_manager.malloc(total_size, MMTAG_LUA);
    if (!new_hdr) {
        profile_cancel(PROFILE_LUA_ENGINE_ALLOC);
        profile_count_add("lua_eng_realloc_failed");
        return NULL;
    }

    /* Safe copy: only the old payload size */
    memcpy((char *)new_hdr + sizeof(LuaAllocHdr), ptr, old_size);

    new_hdr->size = nsize;
    new_hdr->pad = LUA_HDR_MAGIC;
    uint64_t *new_tail = lua_tail_from_hdr(new_hdr);
    *new_tail = LUA_TAIL_CANARY;

    internal->memory_used += grow;

    memory_manager.free((void *)old_hdr);

    profile_count_add("lua_eng_realloc_success");
    profile_stop(PROFILE_LUA_ENGINE_ALLOC, "lua_eng_realloc");
    return (char *)new_hdr + sizeof(LuaAllocHdr);
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
    log_debug("LUA",
              "Function '%s' not found directly in instance (found type: %s), "
              "trying metatable",
              func_name, lua_typename(L, lua_type(L, -1)));

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
            lua_pushvalue(L, -3);         // Push instance table
            lua_pushstring(L, func_name); // Push function name

            if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
                if (lua_isfunction(L, -1)) {
                    log_debug("LUA", "Found function via metatable __index");

                    // Clean up: remove metatable and instance, keep function
                    lua_remove(L, -3); // Remove metatable
                    lua_remove(L, -2); // Remove instance

                    profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP,
                                 "lua_eng_inst_get_func_metatable");
                    profile_count_add("lua_eng_inst_get_func_metatable_success");
                    return true;
                } else {
                    log_debug("LUA", "__index returned non-function: %s",
                              lua_typename(L, lua_type(L, -1)));
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

                profile_stop(PROFILE_LUA_ENGINE_FUNCTION_LOOKUP,
                             "lua_eng_inst_get_func_metatable_table");
                profile_count_add("lua_eng_inst_get_func_metatable_table_success");
                return true;
            } else {
                log_debug("LUA", "__index table lookup failed, got: %s",
                          lua_typename(L, lua_type(L, -1)));
                lua_pop(L, 1); // Pop the result
            }
        } else {
            log_debug("LUA", "Metatable __index is not function or table: %s",
                      lua_typename(L, lua_type(L, -1)));
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
    case LUA_VAL_MAP:
        if (arg->value.map) {
            ese_map_lua_push(arg->value.map);
        } else {
            lua_pushnil(L);
        }
        break;
    case LUA_VAL_COLLISION_HIT:
        if (arg->value.collision_hit) {
            ese_collision_hit_lua_push(arg->value.collision_hit);
        } else {
            lua_pushnil(L);
        }
        break;
    case LUA_VAL_MAP_CELL:
        if (arg->value.map_cell) {
            ese_map_cell_lua_push(arg->value.map_cell);
        } else {
            lua_pushnil(L);
        }
        break;
    case LUA_VAL_POINT:
        if (arg->value.point) {
            ese_point_lua_push(arg->value.point);
        } else {
            lua_pushnil(L);
        }
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
        if (arg->value.lua_ref != LUA_NOREF) {
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
    } else {
        profile_stop(PROFILE_LUA_ENGINE_ARG_CONVERSION, "lua_eng_push_luavalue_simple_arg");
    }
}

// Shallow clone script_sandbox_master into a new env, leaves env on top of the
// stack.
void _lua_engine_build_env_from_master(lua_State *L, int master_ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, master_ref); // [..., master]
    int master_idx = lua_gettop(L);

    lua_newtable(L); // [..., master, env]
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

    lua_remove(L, master_idx); // [..., env]
}

int _lua_engine_class_method_normalize(lua_State     *L,
                                       const char    *type_name,
                                       EseLuaClassFn  do_work) {
    int argc = lua_gettop(L);

    // If first arg is a table, treat it as the class/receiver (colon syntax)
    if (argc > 0 && lua_istable(L, 1)) {
        // Optional: sanity check that it's actually the expected type table.
        // You can add a check here if you want to be strict.
        lua_remove(L, 1); // [TypeTable, a, b, ...] -> [a, b, ...]
    }

    // Now, in both syntaxes, logical args start at index 1
    return do_work(L);
}

int _lua_engine_class_method_trampoline(lua_State *L) {
    EseLuaClassFn do_work =
        (EseLuaClassFn)lua_touserdata(L, lua_upvalueindex(1));
    const char *type_name = lua_tostring(L, lua_upvalueindex(2));

    if (!do_work) {
        return luaL_error(L, "internal error: null class method");
    }

    return _lua_engine_class_method_normalize(L, type_name, do_work);
}
