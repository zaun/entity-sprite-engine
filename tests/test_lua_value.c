#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <execinfo.h>
#include <signal.h>
#include "test_utils.h"
#include "../src/scripting/lua_value.h"
#include "../src/scripting/lua_engine_private.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"

// Test function declarations
static void test_lua_value_creation();
static void test_lua_value_modification();
static void test_lua_value_access();
static void test_lua_value_tables();
static void test_lua_value_copy();
static void test_lua_value_memory_management();
static void test_lua_value_edge_cases();
static void test_lua_value_logging();

void segfault_handler(int signo, siginfo_t *info, void *context) {
    void *buffer[32];
    int nptrs = backtrace(buffer, 32);
    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        fprintf(stderr, "---- BACKTRACE START ----\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
        fprintf(stderr, "---- BACKTRACE  END  ----\n");
        free(strings);
    }

    signal(signo, SIG_DFL);
    raise(signo);
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error setting SIGSEGV handler");
        return EXIT_FAILURE;
    }
    
    test_suite_begin("🧪 Starting Lua Value Tests");
    
    // Initialize required systems
    log_init();
    
    // Run all test suites
    test_lua_value_creation();
    test_lua_value_modification();
    test_lua_value_access();
    test_lua_value_tables();
    test_lua_value_copy();
    test_lua_value_memory_management();
    test_lua_value_edge_cases();
    test_lua_value_logging();
    
    // Print final summary
    test_suite_end("🎯 Final Test Summary");
    
    return 0;
}

// Test Lua value creation functions
static void test_lua_value_creation() {
    test_begin("Lua Value Creation");
    
    // Test nil creation
    EseLuaValue* nil_val = lua_value_create_nil("test_nil");
    TEST_ASSERT_NOT_NULL(nil_val, "nil value should be created successfully");
    if (nil_val) {
        TEST_ASSERT(nil_val->type == LUA_VAL_NIL, "nil value should have correct type");
        TEST_ASSERT_STRING_EQUAL("test_nil", nil_val->name, "nil value should have correct name");
        lua_value_free(nil_val);
    }
    
    // Test nil creation with NULL name
    EseLuaValue* nil_val_no_name = lua_value_create_nil("no_name");
    TEST_ASSERT_NOT_NULL(nil_val_no_name, "nil value with 'no_name' should be created successfully");
    if (nil_val_no_name) {
        TEST_ASSERT(nil_val_no_name->type == LUA_VAL_NIL, "nil value should have correct type");
        TEST_ASSERT_STRING_EQUAL("no_name", nil_val_no_name->name, "nil value with 'no_name' should have correct name");
        lua_value_free(nil_val_no_name);
    }
    
    // Test boolean creation
    EseLuaValue* bool_val_true = lua_value_create_bool("test_bool_true", true);
    TEST_ASSERT_NOT_NULL(bool_val_true, "boolean true value should be created successfully");
    if (bool_val_true) {
        TEST_ASSERT(bool_val_true->type == LUA_VAL_BOOL, "boolean value should have correct type");
        TEST_ASSERT(bool_val_true->value.boolean == true, "boolean value should have correct value");
        TEST_ASSERT_STRING_EQUAL("test_bool_true", bool_val_true->name, "boolean value should have correct name");
        lua_value_free(bool_val_true);
    }
    
    EseLuaValue* bool_val_false = lua_value_create_bool("test_bool_false", false);
    TEST_ASSERT_NOT_NULL(bool_val_false, "boolean false value should be created successfully");
    if (bool_val_false) {
        TEST_ASSERT(bool_val_false->type == LUA_VAL_BOOL, "boolean value should have correct type");
        TEST_ASSERT(bool_val_false->value.boolean == false, "boolean value should have correct value");
        lua_value_free(bool_val_false);
    }
    
    // Test number creation
    EseLuaValue* num_val = lua_value_create_number("test_number", 42.5);
    TEST_ASSERT_NOT_NULL(num_val, "number value should be created successfully");
    if (num_val) {
        TEST_ASSERT(num_val->type == LUA_VAL_NUMBER, "number value should have correct type");
        TEST_ASSERT_FLOAT_EQUAL(42.5, num_val->value.number, 0.001f, "number value should have correct value");
        TEST_ASSERT_STRING_EQUAL("test_number", num_val->name, "number value should have correct name");
        lua_value_free(num_val);
    }
    
    // Test string creation
    EseLuaValue* str_val = lua_value_create_string("test_string", "hello world");
    TEST_ASSERT_NOT_NULL(str_val, "string value should be created successfully");
    if (str_val) {
        TEST_ASSERT(str_val->type == LUA_VAL_STRING, "string value should have correct type");
        TEST_ASSERT_STRING_EQUAL("hello world", str_val->value.string, "string value should have correct value");
        TEST_ASSERT_STRING_EQUAL("test_string", str_val->name, "string value should have correct name");
        // Note: String content should be the same, but pointers may be different due to duplication
        lua_value_free(str_val);
    }
    
    // Test table creation
    EseLuaValue* table_val = lua_value_create_table("test_table");
    TEST_ASSERT_NOT_NULL(table_val, "table value should be created successfully");
    if (table_val) {
        TEST_ASSERT(table_val->type == LUA_VAL_TABLE, "table value should have correct type");
        TEST_ASSERT(table_val->value.table.items == NULL, "new table should have NULL items");
        TEST_ASSERT_EQUAL(0, table_val->value.table.count, "new table should have zero count");
        TEST_ASSERT_EQUAL(0, table_val->value.table.capacity, "new table should have zero capacity");
        TEST_ASSERT_STRING_EQUAL("test_table", table_val->name, "table value should have correct name");
        lua_value_free(table_val);
    }
    
    // Test reference creation
    EseLuaValue* ref_val = lua_value_create_ref("test_ref", 123);
    TEST_ASSERT_NOT_NULL(ref_val, "reference value should be created successfully");
    if (ref_val) {
        TEST_ASSERT(ref_val->type == LUA_VAL_REF, "reference value should have correct type");
        TEST_ASSERT_EQUAL(123, ref_val->value.lua_ref, "reference value should have correct value");
        TEST_ASSERT_STRING_EQUAL("test_ref", ref_val->name, "reference value should have correct name");
        lua_value_free(ref_val);
    }
    
    // Test userdata creation
    void* test_data = (void*)0x12345678;
    EseLuaValue* userdata_val = lua_value_create_userdata("test_userdata", test_data);
    TEST_ASSERT_NOT_NULL(userdata_val, "userdata value should be created successfully");
    if (userdata_val) {
        TEST_ASSERT(userdata_val->type == LUA_VAL_USERDATA, "userdata value should have correct type");
        TEST_ASSERT_POINTER_EQUAL(test_data, userdata_val->value.userdata, "userdata value should have correct value");
        TEST_ASSERT_STRING_EQUAL("test_userdata", userdata_val->name, "userdata value should have correct name");
        lua_value_free(userdata_val);
    }
    
    test_end("Lua Value Creation");
}

// Test Lua value modification functions
static void test_lua_value_modification() {
    test_begin("Lua Value Modification");
    
    // Create a value to modify
    EseLuaValue* val = lua_value_create_nil("test_val");
    TEST_ASSERT_NOT_NULL(val, "test value should be created successfully");
    
    if (val) {
        // Test setting to nil
        lua_value_set_nil(val);
        TEST_ASSERT(val->type == LUA_VAL_NIL, "value should be set to nil type");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        // Test setting to boolean
        lua_value_set_bool(val, true);
        TEST_ASSERT(val->type == LUA_VAL_BOOL, "value should be set to bool type");
        TEST_ASSERT(val->value.boolean == true, "boolean value should be set correctly");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        // Test setting to number
        lua_value_set_number(val, 99.75);
        TEST_ASSERT(val->type == LUA_VAL_NUMBER, "value should be set to number type");
        TEST_ASSERT_FLOAT_EQUAL(99.75, val->value.number, 0.001f, "number value should be set correctly");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        // Test setting to string
        lua_value_set_string(val, "modified string");
        TEST_ASSERT(val->type == LUA_VAL_STRING, "value should be set to string type");
        TEST_ASSERT_STRING_EQUAL("modified string", val->value.string, "string value should be set correctly");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        // Note: The string content should be the same, but the pointer should be different
        // since strdup creates a new copy
        
        // Test setting to table
        lua_value_set_table(val);
        TEST_ASSERT(val->type == LUA_VAL_TABLE, "value should be set to table type");
        TEST_ASSERT(val->value.table.items == NULL, "table should have NULL items");
        TEST_ASSERT_EQUAL(0, val->value.table.count, "table should have zero count");
        TEST_ASSERT_EQUAL(0, val->value.table.capacity, "table should have zero capacity");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        // Test setting to reference
        lua_value_set_ref(val, 456);
        TEST_ASSERT(val->type == LUA_VAL_REF, "value should be set to ref type");
        TEST_ASSERT_EQUAL(456, val->value.lua_ref, "reference value should be set correctly");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        // Test setting to userdata
        void* new_data = (void*)0x87654321;
        lua_value_set_userdata(val, new_data);
        TEST_ASSERT(val->type == LUA_VAL_USERDATA, "value should be set to userdata type");
        TEST_ASSERT_POINTER_EQUAL(new_data, val->value.userdata, "userdata value should be set correctly");
        TEST_ASSERT_STRING_EQUAL("test_val", val->name, "name should be preserved");
        
        lua_value_free(val);
    }
    
    test_end("Lua Value Modification");
}

// Test Lua value access functions
static void test_lua_value_access() {
    test_begin("Lua Value Access");
    
    // Test name access
    EseLuaValue* named_val = lua_value_create_nil("test_name");
    TEST_ASSERT_NOT_NULL(named_val, "named value should be created successfully");
    if (named_val) {
        const char* name = lua_value_get_name(named_val);
        TEST_ASSERT_STRING_EQUAL("test_name", name, "get_name should return correct name");
        lua_value_free(named_val);
    }
    
    // Test boolean access
    EseLuaValue* bool_val = lua_value_create_bool("test_bool", true);
    TEST_ASSERT_NOT_NULL(bool_val, "boolean value should be created successfully");
    if (bool_val) {
        bool bool_result = lua_value_get_bool(bool_val);
        TEST_ASSERT(bool_result == true, "get_bool should return correct value");
        lua_value_free(bool_val);
    }
    
    // Test number access
    EseLuaValue* num_val = lua_value_create_number("test_number", 42.5);
    TEST_ASSERT_NOT_NULL(num_val, "number value should be created successfully");
    if (num_val) {
        float num_result = lua_value_get_number(num_val);
        TEST_ASSERT_FLOAT_EQUAL(42.5f, num_result, 0.001f, "get_number should return correct value");
        lua_value_free(num_val);
    }
    
    // Test string access
    EseLuaValue* str_val = lua_value_create_string("test_string", "hello world");
    TEST_ASSERT_NOT_NULL(str_val, "string value should be created successfully");
    if (str_val) {
        const char* str_result = lua_value_get_string(str_val);
        TEST_ASSERT_STRING_EQUAL("hello world", str_result, "get_string should return correct value");
        lua_value_free(str_val);
    }
    
    // Test userdata access
    void* test_data = (void*)0x12345678;
    EseLuaValue* userdata_val = lua_value_create_userdata("test_userdata", test_data);
    TEST_ASSERT_NOT_NULL(userdata_val, "userdata value should be created successfully");
    if (userdata_val) {
        void* userdata_result = lua_value_get_userdata(userdata_val);
        TEST_ASSERT_POINTER_EQUAL(test_data, userdata_result, "get_userdata should return correct value");
        lua_value_free(userdata_val);
    }
    
    test_end("Lua Value Access");
}

// Test Lua value table operations
static void test_lua_value_tables() {
    test_begin("Lua Value Tables");
    
    // Create a table
    EseLuaValue* table = lua_value_create_table("test_table");
    TEST_ASSERT_NOT_NULL(table, "table should be created successfully");
    
    if (table) {
        // Test initial table state
        TEST_ASSERT(table->type == LUA_VAL_TABLE, "table should have correct type");
        TEST_ASSERT(table->value.table.items == NULL, "new table should have NULL items");
        TEST_ASSERT_EQUAL(0, table->value.table.count, "new table should have zero count");
        TEST_ASSERT_EQUAL(0, table->value.table.capacity, "new table should have zero capacity");
        
        // Create some items to add
        EseLuaValue* item1 = lua_value_create_number("first", 1.0);
        EseLuaValue* item2 = lua_value_create_string("second", "hello");
        EseLuaValue* item3 = lua_value_create_bool("third", true);
        
        TEST_ASSERT_NOT_NULL(item1, "item1 should be created successfully");
        TEST_ASSERT_NOT_NULL(item2, "item2 should be created successfully");
        TEST_ASSERT_NOT_NULL(item3, "item3 should be created successfully");
        
        if (item1 && item2 && item3) {
            // Test pushing items (taking ownership)
            lua_value_push(table, item1, false);
            TEST_ASSERT_EQUAL(1, table->value.table.count, "table should have one item after push");
            TEST_ASSERT_EQUAL(4, table->value.table.capacity, "table should have capacity 4 after first push");
            
            lua_value_push(table, item2, false);
            TEST_ASSERT_EQUAL(2, table->value.table.count, "table should have two items after push");
            
            lua_value_push(table, item3, false);
            TEST_ASSERT_EQUAL(3, table->value.table.count, "table should have three items after push");
            
            // Test table property access
            EseLuaValue* found_item = lua_value_get_table_prop(table, "first");
            TEST_ASSERT_NOT_NULL(found_item, "should find item with name 'first'");
            if (found_item) {
                TEST_ASSERT(found_item->type == LUA_VAL_NUMBER, "found item should have correct type");
                TEST_ASSERT_FLOAT_EQUAL(1.0f, found_item->value.number, 0.001f, "found item should have correct value");
            }
            
            found_item = lua_value_get_table_prop(table, "second");
            TEST_ASSERT_NOT_NULL(found_item, "should find item with name 'second'");
            if (found_item) {
                TEST_ASSERT(found_item->type == LUA_VAL_STRING, "found item should have correct type");
                TEST_ASSERT_STRING_EQUAL("hello", found_item->value.string, "found item should have correct value");
            }
            
            found_item = lua_value_get_table_prop(table, "third");
            TEST_ASSERT_NOT_NULL(found_item, "should find item with name 'third'");
            if (found_item) {
                TEST_ASSERT(found_item->type == LUA_VAL_BOOL, "found item should have correct type");
                TEST_ASSERT(found_item->value.boolean == true, "found item should have correct value");
            }
            
            // Test non-existent property
            EseLuaValue* not_found = lua_value_get_table_prop(table, "nonexistent");
            TEST_ASSERT(not_found == NULL, "should return NULL for non-existent property");
            
            // Test pushing with copy
            EseLuaValue* item4 = lua_value_create_number("fourth", 4.0);
            TEST_ASSERT_NOT_NULL(item4, "item4 should be created successfully");
            if (item4) {
                lua_value_push(table, item4, true); // Copy the item
                TEST_ASSERT_EQUAL(4, table->value.table.count, "table should have four items after copy push");
                
                // The original item4 should still be valid since we copied it
                TEST_ASSERT(item4->type == LUA_VAL_NUMBER, "original item should still be valid");
                TEST_ASSERT_FLOAT_EQUAL(4.0f, item4->value.number, 0.001f, "original item should have correct value");
                
                lua_value_free(item4); // Free the original
            }
            
            // Test table property access after copy
            found_item = lua_value_get_table_prop(table, "fourth");
            TEST_ASSERT_NOT_NULL(found_item, "should find copied item with name 'fourth'");
            if (found_item) {
                TEST_ASSERT(found_item->type == LUA_VAL_NUMBER, "found copied item should have correct type");
                TEST_ASSERT_FLOAT_EQUAL(4.0f, found_item->value.number, 0.001f, "found copied item should have correct value");
            }
        }
        
        lua_value_free(table);
    }
    
    test_end("Lua Value Tables");
}

// Test Lua value copy functionality
static void test_lua_value_copy() {
    test_begin("Lua Value Copy");
    
    // Test copying nil value
    EseLuaValue* nil_original = lua_value_create_nil("nil_original");
    TEST_ASSERT_NOT_NULL(nil_original, "nil original should be created successfully");
    
    if (nil_original) {
        EseLuaValue* nil_copy = lua_value_copy(nil_original);
        TEST_ASSERT_NOT_NULL(nil_copy, "nil copy should be created successfully");
        if (nil_copy) {
            TEST_ASSERT(nil_copy->type == LUA_VAL_NIL, "nil copy should have correct type");
            TEST_ASSERT_STRING_EQUAL("nil_original", nil_copy->name, "nil copy should have correct name");
            TEST_ASSERT(nil_copy != nil_original, "copy should be different object");
            lua_value_free(nil_copy);
        }
        lua_value_free(nil_original);
    }
    
    // Test copying boolean value
    EseLuaValue* bool_original = lua_value_create_bool("bool_original", true);
    TEST_ASSERT_NOT_NULL(bool_original, "bool original should be created successfully");
    
    if (bool_original) {
        EseLuaValue* bool_copy = lua_value_copy(bool_original);
        TEST_ASSERT_NOT_NULL(bool_copy, "bool copy should be created successfully");
        if (bool_copy) {
            TEST_ASSERT(bool_copy->type == LUA_VAL_BOOL, "bool copy should have correct type");
            TEST_ASSERT(bool_copy->value.boolean == true, "bool copy should have correct value");
            TEST_ASSERT_STRING_EQUAL("bool_original", bool_copy->name, "bool copy should have correct name");
            TEST_ASSERT(bool_copy != bool_original, "copy should be different object");
            lua_value_free(bool_copy);
        }
        lua_value_free(bool_original);
    }
    
    // Test copying number value
    EseLuaValue* num_original = lua_value_create_number("num_original", 42.5);
    TEST_ASSERT_NOT_NULL(num_original, "number original should be created successfully");
    
    if (num_original) {
        EseLuaValue* num_copy = lua_value_copy(num_original);
        TEST_ASSERT_NOT_NULL(num_copy, "number copy should be created successfully");
        if (num_copy) {
            TEST_ASSERT(num_copy->type == LUA_VAL_NUMBER, "number copy should have correct type");
            TEST_ASSERT_FLOAT_EQUAL(42.5, num_copy->value.number, 0.001f, "number copy should have correct value");
            TEST_ASSERT_STRING_EQUAL("num_original", num_copy->name, "number copy should have correct name");
            TEST_ASSERT(num_copy != num_original, "copy should be different object");
            lua_value_free(num_copy);
        }
        lua_value_free(num_original);
    }
    
    // Test copying string value
    EseLuaValue* str_original = lua_value_create_string("str_original", "hello world");
    TEST_ASSERT_NOT_NULL(str_original, "string original should be created successfully");
    
    if (str_original) {
        EseLuaValue* str_copy = lua_value_copy(str_original);
        TEST_ASSERT_NOT_NULL(str_copy, "string copy should be created successfully");
        if (str_copy) {
            TEST_ASSERT(str_copy->type == LUA_VAL_STRING, "string copy should have correct type");
            TEST_ASSERT_STRING_EQUAL("hello world", str_copy->value.string, "string copy should have correct value");
            TEST_ASSERT_STRING_EQUAL("str_original", str_copy->name, "string copy should have correct name");
            TEST_ASSERT(str_copy != str_original, "copy should be different object");
            // Note: The string content should be the same, but the pointer should be different
            // since strdup creates a new copy
            lua_value_free(str_copy);
        }
        lua_value_free(str_original);
    }
    
    // Test copying table value
    EseLuaValue* table_original = lua_value_create_table("table_original");
    TEST_ASSERT_NOT_NULL(table_original, "table original should be created successfully");
    
    if (table_original) {
        // Test copying an empty table first
        EseLuaValue* empty_table_copy = lua_value_copy(table_original);
        TEST_ASSERT_NOT_NULL(empty_table_copy, "empty table copy should be created successfully");
        if (empty_table_copy) {
            TEST_ASSERT(empty_table_copy->type == LUA_VAL_TABLE, "empty table copy should have correct type");
            TEST_ASSERT_STRING_EQUAL("table_original", empty_table_copy->name, "empty table copy should have correct name");
            TEST_ASSERT(empty_table_copy != table_original, "copy should be different object");
            TEST_ASSERT_EQUAL(0, empty_table_copy->value.table.count, "empty table copy should have zero count");
            
            lua_value_free(empty_table_copy);
        }
        
        // Now add an item and test copying
        EseLuaValue* simple_item = lua_value_create_number("simple", 42.0);
        TEST_ASSERT_NOT_NULL(simple_item, "simple_item should be created successfully");
        
        if (simple_item) {
            lua_value_push(table_original, simple_item, true);  // Copy the item
            
            printf("DEBUG: Table has %zu items before copy\n", table_original->value.table.count);
            printf("DEBUG: First item type: %d\n", table_original->value.table.items[0]->type);
            
            EseLuaValue* table_copy = lua_value_copy(table_original);
            TEST_ASSERT_NOT_NULL(table_copy, "table copy should be created successfully");
            if (table_copy) {
                printf("DEBUG: Copy successful, copy has %zu items\n", table_copy->value.table.count);
                TEST_ASSERT(table_copy->type == LUA_VAL_TABLE, "table copy should have correct type");
                TEST_ASSERT_STRING_EQUAL("table_original", table_copy->name, "table copy should have correct name");
                TEST_ASSERT(table_copy != table_original, "copy should be different object");
                TEST_ASSERT_EQUAL(1, table_copy->value.table.count, "table copy should have same count");
                
                lua_value_free(table_copy);
            }
            
            // Now free the original table and the original item
            lua_value_free(table_original);
            lua_value_free(simple_item);
        }
    }
    
    // Test copying NULL - this should trigger log_assert
    TEST_ASSERT_ABORT(
        lua_value_copy(NULL),
        "copying NULL should trigger log_assert"
    );
    
    test_end("Lua Value Copy");
}

// Test Lua value memory management
static void test_lua_value_memory_management() {
    test_begin("Lua Value Memory Management");
    
    // Test that freeing NULL is safe
    lua_value_free(NULL);
    printf("✓ PASS: Freeing NULL value is safe\n");
    
    // Test freeing simple values
    EseLuaValue* simple_val = lua_value_create_number("simple", 42.0);
    TEST_ASSERT_NOT_NULL(simple_val, "simple value should be created successfully");
    if (simple_val) {
        lua_value_free(simple_val);
        printf("✓ PASS: Simple value freed successfully\n");
    }
    
    // Test freeing string value (should free the duplicated string)
    EseLuaValue* str_val = lua_value_create_string("string_test", "test string");
    TEST_ASSERT_NOT_NULL(str_val, "string value should be created successfully");
    if (str_val) {
        lua_value_free(str_val);
        printf("✓ PASS: String value freed successfully\n");
    }
    
    // Test freeing table with items
    EseLuaValue* table = lua_value_create_table("table_test");
    TEST_ASSERT_NOT_NULL(table, "table should be created successfully");
    
    if (table) {
        // Add some items
        EseLuaValue* item1 = lua_value_create_number("item1", 1.0);
        EseLuaValue* item2 = lua_value_create_string("item2", "test");
        TEST_ASSERT_NOT_NULL(item1, "item1 should be created successfully");
        TEST_ASSERT_NOT_NULL(item2, "item2 should be created successfully");
        
        if (item1 && item2) {
            lua_value_push(table, item1, true);  // Copy the item
            lua_value_push(table, item2, true);  // Copy the item
            
            // Free the table (should recursively free all items)
            lua_value_free(table);
            printf("✓ PASS: Table with items freed successfully\n");
            
            // Free the original items since we copied them
            lua_value_free(item1);
            lua_value_free(item2);
        }
    }
    
    // Test freeing copied values
    EseLuaValue* original = lua_value_create_string("original", "test");
    TEST_ASSERT_NOT_NULL(original, "original should be created successfully");
    
    if (original) {
        EseLuaValue* copy = lua_value_copy(original);
        TEST_ASSERT_NOT_NULL(copy, "copy should be created successfully");
        
        if (copy) {
            // Free both - they should be independent
            lua_value_free(original);
            lua_value_free(copy);
            printf("✓ PASS: Copied values freed successfully\n");
        }
    }
    
    test_end("Lua Value Memory Management");
}

// Test Lua value edge cases
static void test_lua_value_edge_cases() {
    test_begin("Lua Value Edge Cases");
    
    // Test setting values on NULL - these should trigger log_assert
    TEST_ASSERT_ABORT(
        lua_value_set_nil(NULL),
        "lua_value_set_nil should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_bool(NULL, true),
        "lua_value_set_bool should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_number(NULL, 42.0),
        "lua_value_set_number should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_string(NULL, "test"),
        "lua_value_set_string should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_table(NULL),
        "lua_value_set_table should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_ref(NULL, 123),
        "lua_value_set_ref should abort when called with NULL"
    );
    
    TEST_ASSERT_ABORT(
        lua_value_set_userdata(NULL, (void*)0x12345678),
        "lua_value_set_userdata should abort when called with NULL"
    );
    
    // Test pushing NULL to table
    EseLuaValue* table = lua_value_create_table("edge_test");
    TEST_ASSERT_NOT_NULL(table, "table should be created successfully");
    
    if (table) {
        TEST_ASSERT_ABORT(
            lua_value_push(table, NULL, false),
            "lua_value_push should abort when called with NULL item"
        );
        lua_value_free(table);
    }
    
    // Test pushing to non-table
    EseLuaValue* non_table = lua_value_create_number("non_table", 42.0);
    TEST_ASSERT_NOT_NULL(non_table, "non-table value should be created successfully");
    
    if (non_table) {
        EseLuaValue* item = lua_value_create_number("item", 1.0);
        TEST_ASSERT_NOT_NULL(item, "item should be created successfully");
        
        if (item) {
            lua_value_push(non_table, item, false);
            printf("✓ PASS: Pushing to non-table is safe (silently fails)\n");
            lua_value_free(item);
        }
        
        lua_value_free(non_table);
    }
    
    // Test getting properties from non-table
    EseLuaValue* non_table2 = lua_value_create_string("non_table2", "test");
    TEST_ASSERT_NOT_NULL(non_table2, "non-table value should be created successfully");
    
    if (non_table2) {
        EseLuaValue* prop = lua_value_get_table_prop(non_table2, "test");
        TEST_ASSERT(prop == NULL, "getting property from non-table should return NULL");
        printf("✓ PASS: Getting properties from non-table returns NULL\n");
        lua_value_free(non_table2);
    }
    
    // Test getting properties with NULL name
    EseLuaValue* table2 = lua_value_create_table("table2");
    TEST_ASSERT_NOT_NULL(table2, "table should be created successfully");
    
    if (table2) {
        EseLuaValue* prop = lua_value_get_table_prop(table2, NULL);
        TEST_ASSERT(prop == NULL, "getting property with NULL name should return NULL");
        printf("✓ PASS: Getting properties with NULL name returns NULL\n");
        lua_value_free(table2);
    }
    
    test_end("Lua Value Edge Cases");
}

// Test Lua value logging
static void test_lua_value_logging() {
    test_begin("Lua Value Logging");
    
    // Test logging NULL
    log_luavalue(NULL);
    printf("✓ PASS: Logging NULL value is safe\n");
    
    // Test logging simple values
    EseLuaValue* nil_val = lua_value_create_nil("nil_log");
    if (nil_val) {
        log_luavalue(nil_val);
        printf("✓ PASS: Logging nil value works\n");
        lua_value_free(nil_val);
    }
    
    EseLuaValue* bool_val = lua_value_create_bool("bool_log", true);
    if (bool_val) {
        log_luavalue(bool_val);
        printf("✓ PASS: Logging boolean value works\n");
        lua_value_free(bool_val);
    }
    
    EseLuaValue* num_val = lua_value_create_number("num_log", 42.5);
    if (num_val) {
        log_luavalue(num_val);
        printf("✓ PASS: Logging number value works\n");
        lua_value_free(num_val);
    }
    
    EseLuaValue* str_val = lua_value_create_string("str_log", "test string");
    if (str_val) {
        log_luavalue(str_val);
        printf("✓ PASS: Logging string value works\n");
        lua_value_free(str_val);
    }
    
    // Test logging table
    EseLuaValue* table = lua_value_create_table("table_log");
    if (table) {
        // Add some items
        EseLuaValue* item1 = lua_value_create_number("item1", 1.0);
        EseLuaValue* item2 = lua_value_create_string("item2", "test");
        if (item1 && item2) {
            lua_value_push(table, item1, false);
            lua_value_push(table, item2, false);
            
            log_luavalue(table);
            printf("✓ PASS: Logging table value works\n");
        }
        lua_value_free(table);
    }
    
    // Test logging nested table
    EseLuaValue* nested_table = lua_value_create_table("nested_log");
    if (nested_table) {
        EseLuaValue* inner_table = lua_value_create_table("inner");
        if (inner_table) {
            EseLuaValue* inner_item = lua_value_create_number("inner_item", 99.0);
            if (inner_item) {
                lua_value_push(inner_table, inner_item, false);
                lua_value_push(nested_table, inner_table, false);
                
                log_luavalue(nested_table);
                printf("✓ PASS: Logging nested table works\n");
            }
        }
        lua_value_free(nested_table);
    }
    
    test_end("Lua Value Logging");
}
