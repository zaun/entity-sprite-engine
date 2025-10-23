#ifndef ESE_TESTING_H
#define ESE_TESTING_H

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "unity.h"
#include "../src/scripting/lua_engine.h"

/*
  TEST_ASSERT_DEATH(expr, msg);

  Notes:
  - Message comes last to match Unity style.
  - If expr contains commas (e.g. fn(a,b)) you MUST wrap expr in parentheses:
      TEST_ASSERT_DEATH((fn(a,b)), "message");
  - POSIX-only (uses fork/wait/signals).
*/

#define TEST_ASSERT_DEATH(expr, msg) do {                                \
    pid_t _pid = fork();                                            \
    TEST_ASSERT_MESSAGE(_pid != -1, "fork failed for death test");  \
    if (_pid == 0) {                                                \
        /* child: evaluate the expression */                        \
        expr;                                                       \
        _exit(0);                                                   \
    } else {                                                        \
        int _status = 0;                                            \
        waitpid(_pid, &_status, 0);                                 \
        if (WIFSIGNALED(_status)) {                                 \
            int _sig = WTERMSIG(_status);                           \
            if (!(_sig == SIGABRT || _sig == SIGSEGV)) {            \
                TEST_FAIL_MESSAGE(msg);                             \
            }                                                       \
        } else {                                                    \
            /* child exited normally or non-signal => fail */       \
            TEST_FAIL_MESSAGE(msg);                                 \
        }                                                           \
    }                                                               \
} while (0)

#define TEST_ASSERT_LUA(L, code, message) do {                           \
    int result = luaL_dostring(L, code);                            \
    if (result != LUA_OK) {                                         \
        const char *error_msg = lua_tostring(L, -1);                \
        printf("Lua error: %s\n", error_msg ? error_msg : "unknown error"); \
        lua_pop(L, 1);                                              \
    }                                                               \
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, result, message);         \
} while (0)

/* Helper function to create and initialize engine */
EseLuaEngine* create_test_engine(void) {
    EseLuaEngine *engine = lua_engine_create();
    if (engine) {
        lua_engine_add_registry_key(engine->runtime, LUA_ENGINE_KEY, engine);
    }
    return engine;
}

#endif /* ESE_TESTING_H */
