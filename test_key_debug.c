#include <stdio.h>
#include <stdlib.h>
#include "tests/testing.h"
#include "src/types/input_state.h"
#include "src/core/memory_manager.h"
#include "src/utility/log.h"

int main() {
    log_init();
    
    // Create engine and input state
    EseLuaEngine *engine = create_test_engine();
    EseInputState *input = ese_input_state_create(engine);
    
    // Initialize Lua integration
    ese_input_state_lua_init(engine);
    
    lua_State *L = engine->runtime;
    
    // Push input state to Lua
    ese_input_state_lua_push(input);
    lua_setglobal(L, "InputState");
    
    printf("Testing KEY table behavior...\n");
    
    // Test 1: Access KEY table
    const char *test1 = "return InputState.KEY.A";
    int result1 = luaL_dostring(L, test1);
    if (result1 == LUA_OK) {
        int key_a = (int)lua_tointeger(L, -1);
        printf("1. InputState.KEY.A = %d\n", key_a);
        lua_pop(L, 1);
    } else {
        printf("1. Error accessing KEY.A: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    
    // Test 2: Try to modify KEY table
    const char *test2 = "InputState.KEY.A = 999";
    int result2 = luaL_dostring(L, test2);
    printf("2. Modification attempt result: %s\n", result2 == LUA_OK ? "SUCCESS" : "ERROR");
    if (result2 != LUA_OK) {
        printf("   Error message: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    
    // Test 3: Check if modification worked
    const char *test3 = "return InputState.KEY.A";
    int result3 = luaL_dostring(L, test3);
    if (result3 == LUA_OK) {
        int key_a = (int)lua_tointeger(L, -1);
        printf("3. InputState.KEY.A after modification = %d\n", key_a);
        lua_pop(L, 1);
    } else {
        printf("3. Error accessing KEY.A after modification: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    
    // Test 4: Check metatable
    const char *test4 = "local mt = getmetatable(InputState.KEY); return mt and mt.__newindex or 'nil'";
    int result4 = luaL_dostring(L, test4);
    if (result4 == LUA_OK) {
        if (lua_isnil(L, -1)) {
            printf("4. No metatable found\n");
        } else {
            printf("4. Metatable exists, __newindex = %s\n", lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    } else {
        printf("4. Error checking metatable: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    
    // Cleanup
    ese_input_state_destroy(input);
    lua_engine_destroy(engine);
    
    return 0;
}
