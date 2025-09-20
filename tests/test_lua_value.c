/*
* test_lua_value.c - Unity-based tests for lua_value functionality
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>

#include "testing.h"

#include "../src/scripting/lua_value.h"
#include "../src/scripting/lua_engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

/**
* C API Test Functions Declarations
*/
static void test_lua_value_sizeof(void);
static void test_lua_value_create_nil(void);
static void test_lua_value_create_bool(void);
static void test_lua_value_create_number(void);
static void test_lua_value_create_string(void);
static void test_lua_value_create_table(void);
static void test_lua_value_create_ref(void);
static void test_lua_value_create_userdata(void);
static void test_lua_value_set_nil(void);
static void test_lua_value_set_bool(void);
static void test_lua_value_set_number(void);
static void test_lua_value_set_string(void);
static void test_lua_value_set_table(void);
static void test_lua_value_set_ref(void);
static void test_lua_value_set_userdata(void);
static void test_lua_value_get_name(void);
static void test_lua_value_get_bool(void);
static void test_lua_value_get_number(void);
static void test_lua_value_get_string(void);
static void test_lua_value_get_userdata(void);
static void test_lua_value_push(void);
static void test_lua_value_get_table_prop(void);
static void test_lua_value_copy(void);
static void test_lua_value_destroy(void);
static void test_lua_value_logging(void);
static void test_lua_value_edge_cases(void);
static void test_lua_value_memory_management(void);

/**
* Test suite setup and teardown
*/
static EseLuaEngine *g_engine = NULL;

void setUp(void) {
    g_engine = create_test_engine();
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseLuaValue Tests\n");
    printf("-----------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_lua_value_sizeof);
    RUN_TEST(test_lua_value_create_nil);
    RUN_TEST(test_lua_value_create_bool);
    RUN_TEST(test_lua_value_create_number);
    RUN_TEST(test_lua_value_create_string);
    RUN_TEST(test_lua_value_create_table);
    RUN_TEST(test_lua_value_create_ref);
    RUN_TEST(test_lua_value_create_userdata);
    RUN_TEST(test_lua_value_set_nil);
    RUN_TEST(test_lua_value_set_bool);
    RUN_TEST(test_lua_value_set_number);
    RUN_TEST(test_lua_value_set_string);
    RUN_TEST(test_lua_value_set_table);
    RUN_TEST(test_lua_value_set_ref);
    RUN_TEST(test_lua_value_set_userdata);
    RUN_TEST(test_lua_value_get_name);
    RUN_TEST(test_lua_value_get_bool);
    RUN_TEST(test_lua_value_get_number);
    RUN_TEST(test_lua_value_get_string);
    RUN_TEST(test_lua_value_get_userdata);
    RUN_TEST(test_lua_value_push);
    RUN_TEST(test_lua_value_get_table_prop);
    RUN_TEST(test_lua_value_copy);
    RUN_TEST(test_lua_value_destroy);
    RUN_TEST(test_lua_value_logging);
    RUN_TEST(test_lua_value_edge_cases);
    RUN_TEST(test_lua_value_memory_management);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_lua_value_sizeof(void) {
    // Test that we can create a value and it has reasonable size
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    if (val) {
        lua_value_destroy(val);
    }
}

static void test_lua_value_create_nil(void) {
    // Test nil creation with name
    EseLuaValue *val = lua_value_create_nil("test_nil");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Nil value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_nil", lua_value_get_name(val), "Name should be correct");
        lua_value_destroy(val);
    }

    // Test nil creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_nil(NULL), "lua_value_create_nil should abort with NULL name");
}

static void test_lua_value_create_bool(void) {
    // Test boolean true creation
    EseLuaValue *val_true = lua_value_create_bool("test_true", true);
    TEST_ASSERT_NOT_NULL_MESSAGE(val_true, "Boolean true value should be created");
    if (val_true) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_true", lua_value_get_name(val_true), "Name should be correct");
        TEST_ASSERT_TRUE_MESSAGE(lua_value_get_bool(val_true), "Boolean value should be true");
        lua_value_destroy(val_true);
    }

    // Test boolean false creation
    EseLuaValue *val_false = lua_value_create_bool("test_false", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(val_false, "Boolean false value should be created");
    if (val_false) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_false", lua_value_get_name(val_false), "Name should be correct");
        TEST_ASSERT_FALSE_MESSAGE(lua_value_get_bool(val_false), "Boolean value should be false");
        lua_value_destroy(val_false);
    }

    // Test boolean creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_bool(NULL, true), "lua_value_create_bool should abort with NULL name");
}

static void test_lua_value_create_number(void) {
    // Test number creation
    EseLuaValue *val = lua_value_create_number("test_number", 42.5);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Number value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_number", lua_value_get_name(val), "Name should be correct");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 42.5f, lua_value_get_number(val), "Number value should be correct");
        lua_value_destroy(val);
    }

    // Test number creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_number(NULL, -99.75), "lua_value_create_number should abort with NULL name");
}

static void test_lua_value_create_string(void) {
    // Test string creation
    EseLuaValue *val = lua_value_create_string("test_string", "hello world");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "String value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_string", lua_value_get_name(val), "Name should be correct");
        TEST_ASSERT_EQUAL_STRING_MESSAGE("hello world", lua_value_get_string(val), "String value should be correct");
        lua_value_destroy(val);
    }

    // Test string creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_string(NULL, "test"), "lua_value_create_string should abort with NULL name");
}

static void test_lua_value_create_table(void) {
    // Test table creation
    EseLuaValue *val = lua_value_create_table("test_table");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Table value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_table", lua_value_get_name(val), "Name should be correct");
        lua_value_destroy(val);
    }

    // Test table creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_table(NULL), "lua_value_create_table should abort with NULL name");
}

static void test_lua_value_create_ref(void) {
    // Test reference creation
    EseLuaValue *val = lua_value_create_ref("test_ref", 123);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Reference value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_ref", lua_value_get_name(val), "Name should be correct");
        lua_value_destroy(val);
    }

    // Test reference creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_ref(NULL, 456), "lua_value_create_ref should abort with NULL name");
}

static void test_lua_value_create_userdata(void) {
    // Test userdata creation
    void *test_data = (void*)0x12345678;
    EseLuaValue *val = lua_value_create_userdata("test_userdata", test_data);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Userdata value should be created");
    if (val) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("test_userdata", lua_value_get_name(val), "Name should be correct");
        TEST_ASSERT_EQUAL_PTR_MESSAGE(test_data, lua_value_get_userdata(val), "Userdata value should be correct");
        lua_value_destroy(val);
    }

    // Test userdata creation with NULL name - should assert
    ASSERT_DEATH(lua_value_create_userdata(NULL, test_data), "lua_value_create_userdata should abort with NULL name");
}

static void test_lua_value_set_nil(void) {
    EseLuaValue *val = lua_value_create_number("test", 42.0);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_nil(val);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_bool(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_bool(val, true);
    TEST_ASSERT_TRUE_MESSAGE(lua_value_get_bool(val), "Boolean should be set to true");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_set_bool(val, false);
    TEST_ASSERT_FALSE_MESSAGE(lua_value_get_bool(val), "Boolean should be set to false");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_number(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_number(val, 99.75);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 99.75f, lua_value_get_number(val), "Number should be set correctly");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_string(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_string(val, "modified string");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("modified string", lua_value_get_string(val), "String should be set correctly");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_table(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_table(val);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_ref(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    lua_value_set_ref(val, 456);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_set_userdata(void) {
    EseLuaValue *val = lua_value_create_nil("test");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    void *new_data = (void*)0x87654321;
    lua_value_set_userdata(val, new_data);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(new_data, lua_value_get_userdata(val), "Userdata should be set correctly");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test", lua_value_get_name(val), "Name should be preserved");
    
    lua_value_destroy(val);
}

static void test_lua_value_get_name(void) {
    EseLuaValue *val = lua_value_create_nil("test_name");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    const char *name = lua_value_get_name(val);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test_name", name, "Name should be correct");
    
    lua_value_destroy(val);
}

static void test_lua_value_get_bool(void) {
    EseLuaValue *val = lua_value_create_bool("test", true);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    bool result = lua_value_get_bool(val);
    TEST_ASSERT_TRUE_MESSAGE(result, "Boolean should be true");
    
    lua_value_destroy(val);
}

static void test_lua_value_get_number(void) {
    EseLuaValue *val = lua_value_create_number("test", 42.5);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    float result = lua_value_get_number(val);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 42.5f, result, "Number should be correct");
    
    lua_value_destroy(val);
}

static void test_lua_value_get_string(void) {
    EseLuaValue *val = lua_value_create_string("test", "hello world");
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    const char *result = lua_value_get_string(val);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("hello world", result, "String should be correct");
    
    lua_value_destroy(val);
}

static void test_lua_value_get_userdata(void) {
    void *test_data = (void*)0x12345678;
    EseLuaValue *val = lua_value_create_userdata("test", test_data);
    TEST_ASSERT_NOT_NULL_MESSAGE(val, "Value should be created");
    
    void *result = lua_value_get_userdata(val);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(test_data, result, "Userdata should be correct");
    
    lua_value_destroy(val);
}

static void test_lua_value_push(void) {
    EseLuaValue *table = lua_value_create_table("test_table");
    TEST_ASSERT_NOT_NULL_MESSAGE(table, "Table should be created");
    
    EseLuaValue *item1 = lua_value_create_number("first", 1.0);
    EseLuaValue *item2 = lua_value_create_string("second", "hello");
    EseLuaValue *item3 = lua_value_create_bool("third", true);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(item1, "Item1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(item2, "Item2 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(item3, "Item3 should be created");
    
    if (item1 && item2 && item3) {
        // Test pushing items (taking ownership)
        lua_value_push(table, item1, false);
        lua_value_push(table, item2, false);
        lua_value_push(table, item3, false);
        
        // Test pushing with copy
        EseLuaValue *item4 = lua_value_create_number("fourth", 4.0);
        TEST_ASSERT_NOT_NULL_MESSAGE(item4, "Item4 should be created");
        if (item4) {
            lua_value_push(table, item4, true); // Copy the item
            lua_value_destroy(item4); // Free the original
        }
    }
    
    lua_value_destroy(table);
}

static void test_lua_value_get_table_prop(void) {
    EseLuaValue *table = lua_value_create_table("test_table");
    TEST_ASSERT_NOT_NULL_MESSAGE(table, "Table should be created");
    
    EseLuaValue *item1 = lua_value_create_number("first", 1.0);
    EseLuaValue *item2 = lua_value_create_string("second", "hello");
    
    TEST_ASSERT_NOT_NULL_MESSAGE(item1, "Item1 should be created");
    TEST_ASSERT_NOT_NULL_MESSAGE(item2, "Item2 should be created");
    
    if (item1 && item2) {
        lua_value_push(table, item1, false);
        lua_value_push(table, item2, false);
        
        // Test finding properties
        EseLuaValue *found = lua_value_get_table_prop(table, "first");
        TEST_ASSERT_NOT_NULL_MESSAGE(found, "Should find item with name 'first'");
        if (found) {
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 1.0f, lua_value_get_number(found), "Found item should have correct value");
        }
        
        found = lua_value_get_table_prop(table, "second");
        TEST_ASSERT_NOT_NULL_MESSAGE(found, "Should find item with name 'second'");
        if (found) {
            TEST_ASSERT_EQUAL_STRING_MESSAGE("hello", lua_value_get_string(found), "Found item should have correct value");
        }
        
        // Test non-existent property
        EseLuaValue *not_found = lua_value_get_table_prop(table, "nonexistent");
        TEST_ASSERT_NULL_MESSAGE(not_found, "Should return NULL for non-existent property");
    }
    
    lua_value_destroy(table);
}

static void test_lua_value_copy(void) {
    // Test copying nil value
    EseLuaValue *nil_original = lua_value_create_nil("nil_original");
    TEST_ASSERT_NOT_NULL_MESSAGE(nil_original, "Nil original should be created");
    
    if (nil_original) {
        EseLuaValue *nil_copy = lua_value_copy(nil_original);
        TEST_ASSERT_NOT_NULL_MESSAGE(nil_copy, "Nil copy should be created");
        if (nil_copy) {
            TEST_ASSERT_EQUAL_STRING_MESSAGE("nil_original", lua_value_get_name(nil_copy), "Copy should have correct name");
            TEST_ASSERT_TRUE_MESSAGE(nil_copy != nil_original, "Copy should be different object");
            lua_value_destroy(nil_copy);
        }
        lua_value_destroy(nil_original);
    }
    
    // Test copying boolean value
    EseLuaValue *bool_original = lua_value_create_bool("bool_original", true);
    TEST_ASSERT_NOT_NULL_MESSAGE(bool_original, "Bool original should be created");
    
    if (bool_original) {
        EseLuaValue *bool_copy = lua_value_copy(bool_original);
        TEST_ASSERT_NOT_NULL_MESSAGE(bool_copy, "Bool copy should be created");
        if (bool_copy) {
            TEST_ASSERT_TRUE_MESSAGE(lua_value_get_bool(bool_copy), "Bool copy should have correct value");
            TEST_ASSERT_EQUAL_STRING_MESSAGE("bool_original", lua_value_get_name(bool_copy), "Bool copy should have correct name");
            TEST_ASSERT_TRUE_MESSAGE(bool_copy != bool_original, "Copy should be different object");
            lua_value_destroy(bool_copy);
        }
        lua_value_destroy(bool_original);
    }
    
    // Test copying number value
    EseLuaValue *num_original = lua_value_create_number("num_original", 42.5);
    TEST_ASSERT_NOT_NULL_MESSAGE(num_original, "Number original should be created");
    
    if (num_original) {
        EseLuaValue *num_copy = lua_value_copy(num_original);
        TEST_ASSERT_NOT_NULL_MESSAGE(num_copy, "Number copy should be created");
        if (num_copy) {
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 42.5f, lua_value_get_number(num_copy), "Number copy should have correct value");
            TEST_ASSERT_EQUAL_STRING_MESSAGE("num_original", lua_value_get_name(num_copy), "Number copy should have correct name");
            TEST_ASSERT_TRUE_MESSAGE(num_copy != num_original, "Copy should be different object");
            lua_value_destroy(num_copy);
        }
        lua_value_destroy(num_original);
    }
    
    // Test copying string value
    EseLuaValue *str_original = lua_value_create_string("str_original", "hello world");
    TEST_ASSERT_NOT_NULL_MESSAGE(str_original, "String original should be created");
    
    if (str_original) {
        EseLuaValue *str_copy = lua_value_copy(str_original);
        TEST_ASSERT_NOT_NULL_MESSAGE(str_copy, "String copy should be created");
        if (str_copy) {
            TEST_ASSERT_EQUAL_STRING_MESSAGE("hello world", lua_value_get_string(str_copy), "String copy should have correct value");
            TEST_ASSERT_EQUAL_STRING_MESSAGE("str_original", lua_value_get_name(str_copy), "String copy should have correct name");
            TEST_ASSERT_TRUE_MESSAGE(str_copy != str_original, "Copy should be different object");
            lua_value_destroy(str_copy);
        }
        lua_value_destroy(str_original);
    }
    
    // Test copying table value
    EseLuaValue *table_original = lua_value_create_table("table_original");
    TEST_ASSERT_NOT_NULL_MESSAGE(table_original, "Table original should be created");
    
    if (table_original) {
        EseLuaValue *simple_item = lua_value_create_number("simple", 42.0);
        TEST_ASSERT_NOT_NULL_MESSAGE(simple_item, "Simple item should be created");
        
        if (simple_item) {
            lua_value_push(table_original, simple_item, true);  // Copy the item
            
            EseLuaValue *table_copy = lua_value_copy(table_original);
            TEST_ASSERT_NOT_NULL_MESSAGE(table_copy, "Table copy should be created");
            if (table_copy) {
                TEST_ASSERT_EQUAL_STRING_MESSAGE("table_original", lua_value_get_name(table_copy), "Table copy should have correct name");
                TEST_ASSERT_TRUE_MESSAGE(table_copy != table_original, "Copy should be different object");
                lua_value_destroy(table_copy);
            }
            
            lua_value_destroy(simple_item);
        }
        
        lua_value_destroy(table_original);
    }
    
    // Test copying NULL - this should trigger log_assert
    ASSERT_DEATH(lua_value_copy(NULL), "copying NULL should trigger log_assert");
}

static void test_lua_value_destroy(void) {
    // Test that freeing NULL is safe
    lua_value_destroy(NULL);
    
    // Test freeing simple values
    EseLuaValue *simple_val = lua_value_create_number("simple", 42.0);
    TEST_ASSERT_NOT_NULL_MESSAGE(simple_val, "Simple value should be created");
    if (simple_val) {
        lua_value_destroy(simple_val);
    }
    
    // Test freeing string value
    EseLuaValue *str_val = lua_value_create_string("string_test", "test string");
    TEST_ASSERT_NOT_NULL_MESSAGE(str_val, "String value should be created");
    if (str_val) {
        lua_value_destroy(str_val);
    }
    
    // Test freeing table with items
    EseLuaValue *table = lua_value_create_table("table_test");
    TEST_ASSERT_NOT_NULL_MESSAGE(table, "Table should be created");
    
    if (table) {
        EseLuaValue *item1 = lua_value_create_number("item1", 1.0);
        EseLuaValue *item2 = lua_value_create_string("item2", "test");
        TEST_ASSERT_NOT_NULL_MESSAGE(item1, "Item1 should be created");
        TEST_ASSERT_NOT_NULL_MESSAGE(item2, "Item2 should be created");
        
        if (item1 && item2) {
            lua_value_push(table, item1, true);  // Copy the item
            lua_value_push(table, item2, true);  // Copy the item
            
            lua_value_destroy(table);
            
            // Free the original items since we copied them
            lua_value_destroy(item1);
            lua_value_destroy(item2);
        }
    }
}

static void test_lua_value_logging(void) {
    // Test logging NULL
    log_luavalue(NULL);
    
    // Test logging simple values
    EseLuaValue *nil_val = lua_value_create_nil("nil_log");
    if (nil_val) {
        log_luavalue(nil_val);
        lua_value_destroy(nil_val);
    }
    
    EseLuaValue *bool_val = lua_value_create_bool("bool_log", true);
    if (bool_val) {
        log_luavalue(bool_val);
        lua_value_destroy(bool_val);
    }
    
    EseLuaValue *num_val = lua_value_create_number("num_log", 42.5);
    if (num_val) {
        log_luavalue(num_val);
        lua_value_destroy(num_val);
    }
    
    EseLuaValue *str_val = lua_value_create_string("str_log", "test string");
    if (str_val) {
        log_luavalue(str_val);
        lua_value_destroy(str_val);
    }
    
    // Test logging table
    EseLuaValue *table = lua_value_create_table("table_log");
    if (table) {
        EseLuaValue *item1 = lua_value_create_number("item1", 1.0);
        EseLuaValue *item2 = lua_value_create_string("item2", "test");
        if (item1 && item2) {
            lua_value_push(table, item1, false);
            lua_value_push(table, item2, false);
            
            log_luavalue(table);
        }
        lua_value_destroy(table);
    }
}

static void test_lua_value_edge_cases(void) {
    // Test setting values on NULL - these should trigger log_assert
    ASSERT_DEATH(lua_value_set_nil(NULL), "lua_value_set_nil should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_bool(NULL, true), "lua_value_set_bool should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_number(NULL, 42.0), "lua_value_set_number should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_string(NULL, "test"), "lua_value_set_string should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_table(NULL), "lua_value_set_table should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_ref(NULL, 123), "lua_value_set_ref should abort when called with NULL");
    ASSERT_DEATH(lua_value_set_userdata(NULL, (void*)0x12345678), "lua_value_set_userdata should abort when called with NULL");
    
    // Test pushing NULL to table
    EseLuaValue *table = lua_value_create_table("edge_test");
    TEST_ASSERT_NOT_NULL_MESSAGE(table, "Table should be created");
    
    if (table) {
        ASSERT_DEATH(lua_value_push(table, NULL, false), "lua_value_push should abort when called with NULL item");
        lua_value_destroy(table);
    }
    
    // Test pushing to non-table
    EseLuaValue *non_table = lua_value_create_number("non_table", 42.0);
    TEST_ASSERT_NOT_NULL_MESSAGE(non_table, "Non-table value should be created");
    
    if (non_table) {
        EseLuaValue *item = lua_value_create_number("item", 1.0);
        TEST_ASSERT_NOT_NULL_MESSAGE(item, "Item should be created");
        
        if (item) {
            lua_value_push(non_table, item, false);
            lua_value_destroy(item);
        }
        
        lua_value_destroy(non_table);
    }
    
    // Test getting properties from non-table
    EseLuaValue *non_table2 = lua_value_create_string("non_table2", "test");
    TEST_ASSERT_NOT_NULL_MESSAGE(non_table2, "Non-table value should be created");
    
    if (non_table2) {
        EseLuaValue *prop = lua_value_get_table_prop(non_table2, "test");
        TEST_ASSERT_NULL_MESSAGE(prop, "Getting property from non-table should return NULL");
        lua_value_destroy(non_table2);
    }
    
    // Test getting properties with NULL name
    EseLuaValue *table2 = lua_value_create_table("table2");
    TEST_ASSERT_NOT_NULL_MESSAGE(table2, "Table should be created");
    
    if (table2) {
        EseLuaValue *prop = lua_value_get_table_prop(table2, NULL);
        TEST_ASSERT_NULL_MESSAGE(prop, "Getting property with NULL name should return NULL");
        lua_value_destroy(table2);
    }
}

static void test_lua_value_memory_management(void) {
    // Test freeing copied values
    EseLuaValue *original = lua_value_create_string("original", "test");
    TEST_ASSERT_NOT_NULL_MESSAGE(original, "Original should be created");
    
    if (original) {
        EseLuaValue *copy = lua_value_copy(original);
        TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
        
        if (copy) {
            // Free both - they should be independent
            lua_value_destroy(original);
            lua_value_destroy(copy);
        }
    }
    
    // Test complex nested table cleanup
    EseLuaValue *outer_table = lua_value_create_table("outer");
    TEST_ASSERT_NOT_NULL_MESSAGE(outer_table, "Outer table should be created");
    
    if (outer_table) {
        EseLuaValue *inner_table = lua_value_create_table("inner");
        TEST_ASSERT_NOT_NULL_MESSAGE(inner_table, "Inner table should be created");
        
        if (inner_table) {
            EseLuaValue *inner_item = lua_value_create_number("inner_item", 99.0);
            TEST_ASSERT_NOT_NULL_MESSAGE(inner_item, "Inner item should be created");
            
            if (inner_item) {
                lua_value_push(inner_table, inner_item, false);
                lua_value_push(outer_table, inner_table, false);
                
                // Free the outer table - should recursively free everything
                lua_value_destroy(outer_table);
            }
        }
    }
}