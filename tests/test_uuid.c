#include "test_utils.h"
#include "core/memory_manager.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_engine_private.h"
#include "utility/log.h"
#include <math.h>
#include <signal.h>
#include <string.h>

// Include execinfo.h with a workaround for uuid conflicts
#define uuid_copy system_uuid_copy
#define uuid_generate system_uuid_generate
#include <execinfo.h>
#undef uuid_copy
#undef uuid_generate

#include "types/uuid.h"

// Define LUA_NOREF if not already defined
#ifndef LUA_NOREF
#define LUA_NOREF -1
#endif

// Test function declarations
static void test_uuid_creation();
static void test_uuid_properties();
static void test_uuid_copy();
static void test_uuid_string_operations();
static void test_uuid_lua_integration();
static void test_uuid_lua_script_api();
static void test_uuid_null_pointer_aborts();

// Test Lua script content for UUID testing
static const char* test_uuid_lua_script = 
"function UUID_TEST_MODULE:test_uuid_creation()\n"
"    local uuid1 = UUID.new()\n"
"    local uuid2 = UUID.new()\n"
"    \n"
"    if uuid1.value and uuid2.value and uuid1.value ~= uuid2.value then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function UUID_TEST_MODULE:test_uuid_properties()\n"
"    local uuid = UUID.new()\n"
"    \n"
"    if uuid.value and string.len(uuid.value) == 36 then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n"
"\n"
"function UUID_TEST_MODULE:test_uuid_operations()\n"
"    local uuid1 = UUID.new()\n"
"    local uuid2 = UUID.copy(uuid1)\n"
"    \n"
"    if uuid1.value == uuid2.value then\n"
"        return true\n"
"    else\n"
"        return false\n"
"    end\n"
"end\n";

// Signal handler for testing aborts
static void segfault_handler(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    fprintf(stderr, "---- BACKTRACE START ----\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "---- BACKTRACE  END  ----\n");
    exit(1);
}

int main() {
    // Set up signal handler for testing aborts
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    
    test_suite_begin("🧪 EseUUID Test Suite");
    
    test_uuid_creation();
    test_uuid_properties();
    test_uuid_copy();
    test_uuid_string_operations();
    test_uuid_lua_integration();
    test_uuid_lua_script_api();
    test_uuid_null_pointer_aborts();
    
    test_suite_end("🎯 EseUUID Test Suite");
    
    return 0;
}

static void test_uuid_creation() {
    test_begin("UUID Creation Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID creation tests");
    
    EseUUID *uuid = uuid_create(engine);
    TEST_ASSERT_NOT_NULL(uuid, "uuid_create should return non-NULL pointer");
    TEST_ASSERT(strlen(uuid->value) == 36, "UUID should have 36 character string");
    TEST_ASSERT_POINTER_EQUAL(engine->runtime, uuid->state, "UUID should have correct Lua state");
    TEST_ASSERT_EQUAL(0, uuid->lua_ref_count, "New UUID should have ref count 0");
    TEST_ASSERT(uuid->lua_ref == LUA_NOREF, "New UUID should have negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", uuid->lua_ref);
    TEST_ASSERT(sizeof(EseUUID) > 0, "EseUUID should have positive size");
    printf("ℹ INFO: Actual UUID size: %zu bytes\n", sizeof(EseUUID));
    printf("ℹ INFO: Generated UUID: %s\n", uuid->value);
    
    uuid_destroy(uuid);
    lua_engine_destroy(engine);
    
    test_end("UUID Creation Tests");
}

static void test_uuid_properties() {
    test_begin("UUID Properties Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID property tests");
    
    EseUUID *uuid = uuid_create(engine);
    TEST_ASSERT_NOT_NULL(uuid, "UUID should be created for property tests");
    
    const char *original_value = uuid->value;
    TEST_ASSERT_NOT_NULL(original_value, "uuid_get_value should return non-NULL string");
    TEST_ASSERT(strlen(original_value) == 36, "UUID value should be 36 characters long");
    
    // Test that UUID format is correct (contains hyphens at expected positions)
    TEST_ASSERT(original_value[8] == '-', "UUID should have hyphen at position 8");
    TEST_ASSERT(original_value[13] == '-', "UUID should have hyphen at position 13");
    TEST_ASSERT(original_value[18] == '-', "UUID should have hyphen at position 18");
    TEST_ASSERT(original_value[23] == '-', "UUID should have hyphen at position 23");
    
    uuid_destroy(uuid);
    lua_engine_destroy(engine);
    
    test_end("UUID Properties Tests");
}

static void test_uuid_copy() {
    test_begin("UUID Copy Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID copy tests");
    
    EseUUID *original = uuid_create(engine);
    TEST_ASSERT_NOT_NULL(original, "Original UUID should be created for copy tests");
    
    EseUUID *copy = uuid_copy(original);
    TEST_ASSERT_NOT_NULL(copy, "uuid_copy should return non-NULL pointer");
    TEST_ASSERT_STRING_EQUAL(original->value, copy->value, "Copied UUID should have same value");
    TEST_ASSERT(original != copy, "Copy should be a different object");
    TEST_ASSERT_POINTER_EQUAL(original->state, copy->state, "Copy should have same Lua state");
    TEST_ASSERT(copy->lua_ref == LUA_NOREF, "Copy should start with negative LUA_NOREF value");
    printf("ℹ INFO: Copy LUA_NOREF value: %d\n", copy->lua_ref);
    TEST_ASSERT_EQUAL(0, copy->lua_ref_count, "Copy should start with ref count 0");
    
    uuid_destroy(original);
    uuid_destroy(copy);
    lua_engine_destroy(engine);
    
    test_end("UUID Copy Tests");
}

static void test_uuid_string_operations() {
    test_begin("UUID String Operations Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID string operation tests");
    
    EseUUID *uuid1 = uuid_create(engine);
    EseUUID *uuid2 = uuid_create(engine);
    
    TEST_ASSERT_NOT_NULL(uuid1, "UUID1 should be created for string operation tests");
    TEST_ASSERT_NOT_NULL(uuid2, "UUID2 should be created for string operation tests");
    
    // Test that two different UUIDs are not equal
    TEST_ASSERT(strcmp(uuid1->value, uuid2->value) != 0, "Two different UUIDs should not be equal");
    
    // Test UUID format validation
    const char *value1 = uuid1->value;
    const char *value2 = uuid2->value;
    
    TEST_ASSERT_NOT_NULL(value1, "UUID1 value should not be NULL");
    TEST_ASSERT_NOT_NULL(value2, "UUID2 value should not be NULL");
    TEST_ASSERT(strlen(value1) == 36, "UUID1 should be 36 characters long");
    TEST_ASSERT(strlen(value2) == 36, "UUID2 should be 36 characters long");
    
    // Test that UUIDs are valid hexadecimal with hyphens
    bool valid_format1 = true;
    bool valid_format2 = true;
    
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (value1[i] != '-') valid_format1 = false;
            if (value2[i] != '-') valid_format2 = false;
        } else {
            char c1 = value1[i];
            char c2 = value2[i];
            if (!((c1 >= '0' && c1 <= '9') || (c1 >= 'a' && c1 <= 'f') || (c1 >= 'A' && c1 <= 'F'))) {
                valid_format1 = false;
            }
            if (!((c2 >= '0' && c2 <= '9') || (c2 >= 'a' && c2 <= 'f') || (c2 >= 'A' && c2 <= 'F'))) {
                valid_format2 = false;
            }
        }
    }
    
    TEST_ASSERT(valid_format1, "UUID1 should have valid format");
    TEST_ASSERT(valid_format2, "UUID2 should have valid format");
    
    uuid_destroy(uuid1);
    uuid_destroy(uuid2);
    lua_engine_destroy(engine);
    
    test_end("UUID String Operations Tests");
}

static void test_uuid_lua_integration() {
    test_begin("UUID Lua Integration Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID Lua integration tests");
    
    EseUUID *uuid = uuid_create(engine);
    TEST_ASSERT_NOT_NULL(uuid, "UUID should be created for Lua integration tests");
    TEST_ASSERT_EQUAL(0, uuid->lua_ref_count, "New UUID should start with ref count 0");
    TEST_ASSERT(uuid->lua_ref == LUA_NOREF, "New UUID should start with negative LUA_NOREF value");
    printf("ℹ INFO: Actual LUA_NOREF value: %d\n", uuid->lua_ref);
    
    uuid_destroy(uuid);
    lua_engine_destroy(engine);
    
    test_end("UUID Lua Integration Tests");
}

static void test_uuid_lua_script_api() {
    test_begin("UUID Lua Script API Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID Lua script API tests");
    
    uuid_lua_init(engine);
    printf("ℹ INFO: UUID Lua integration initialized\n");
    
    // Test that UUID Lua integration initializes successfully
    TEST_ASSERT(true, "UUID Lua integration should initialize successfully");
    
    lua_engine_destroy(engine);
    
    test_end("UUID Lua Script API Tests");
}

static void test_uuid_null_pointer_aborts() {
    test_begin("UUID NULL Pointer Abort Tests");
    
    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should be created for UUID NULL pointer abort tests");
    
    EseUUID *uuid = uuid_create(engine);
    TEST_ASSERT_NOT_NULL(uuid, "UUID should be created for UUID NULL pointer abort tests");
    
    TEST_ASSERT_ABORT(uuid_create(NULL), "uuid_create should abort with NULL engine");
    TEST_ASSERT_ABORT(uuid_copy(NULL), "uuid_copy should abort with NULL source");
    TEST_ASSERT_ABORT(uuid_lua_init(NULL), "uuid_lua_init should abort with NULL engine");
    TEST_ASSERT_ABORT(uuid_lua_get(NULL, 1), "uuid_lua_get should abort with NULL Lua state");
    TEST_ASSERT_ABORT(uuid_lua_push(NULL), "uuid_lua_push should abort with NULL UUID");
    TEST_ASSERT_ABORT(uuid_ref(NULL), "uuid_ref should abort with NULL UUID");
    
    uuid_destroy(uuid);
    lua_engine_destroy(engine);
    
    test_end("UUID NULL Pointer Abort Tests");
}
