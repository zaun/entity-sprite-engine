/*
* test_ese_gui_style.c - Unity-based tests for gui_style functionality
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

#include "../src/types/gui_style.h"
#include "../src/types/color.h"
#include "../src/graphics/gui/gui.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include "../src/vendor/json/cJSON.h"

/**
* C API Test Functions Declarations
*/
static void test_ese_gui_style_sizeof(void);
static void test_ese_gui_style_create_requires_engine(void);
static void test_ese_gui_style_create(void);
static void test_ese_gui_style_background(void);
static void test_ese_gui_style_background_hovered(void);
static void test_ese_gui_style_background_pressed(void);
static void test_ese_gui_style_border(void);
static void test_ese_gui_style_border_hovered(void);
static void test_ese_gui_style_border_pressed(void);
static void test_ese_gui_style_text(void);
static void test_ese_gui_style_text_hovered(void);
static void test_ese_gui_style_text_pressed(void);
static void test_ese_gui_style_border_width(void);
static void test_ese_gui_style_padding_left(void);
static void test_ese_gui_style_padding_top(void);
static void test_ese_gui_style_padding_right(void);
static void test_ese_gui_style_padding_bottom(void);
static void test_ese_gui_style_ref(void);
static void test_ese_gui_style_copy_requires_engine(void);
static void test_ese_gui_style_copy(void);
static void test_ese_gui_style_watcher_system(void);
static void test_ese_gui_style_lua_integration(void);
static void test_ese_gui_style_lua_init(void);
static void test_ese_gui_style_lua_push(void);
static void test_ese_gui_style_lua_get(void);
static void test_ese_gui_style_serialization(void);

/**
* Lua API Test Functions Declarations
*/
static void test_ese_gui_style_lua_new(void);
static void test_ese_gui_style_lua_properties(void);
static void test_ese_gui_style_lua_tostring(void);
static void test_ese_gui_style_lua_gc(void);

/**
* Global test engine
*/
static EseLuaEngine *g_engine = NULL;

/**
* Test setup and teardown
*/
void setUp(void) {
    g_engine = create_test_engine();
    ese_color_lua_init(g_engine);
    ese_gui_style_lua_init(g_engine);
}

void tearDown(void) {
    lua_engine_destroy(g_engine);
}

int main(void) {
    log_init();

    printf("\nEseGuiStyle Tests\n");
    printf("-----------------\n");

    UNITY_BEGIN();

    // RUN_TEST(test_ese_gui_style_sizeof);
    // RUN_TEST(test_ese_gui_style_create_requires_engine);
    // RUN_TEST(test_ese_gui_style_create);
    // RUN_TEST(test_ese_gui_style_background);
    // RUN_TEST(test_ese_gui_style_background_hovered);
    // RUN_TEST(test_ese_gui_style_background_pressed);
    // RUN_TEST(test_ese_gui_style_border);
    // RUN_TEST(test_ese_gui_style_border_hovered);
    // RUN_TEST(test_ese_gui_style_border_pressed);
    // RUN_TEST(test_ese_gui_style_text);
    // RUN_TEST(test_ese_gui_style_text_hovered);
    // RUN_TEST(test_ese_gui_style_text_pressed);
    // RUN_TEST(test_ese_gui_style_border_width);
    // RUN_TEST(test_ese_gui_style_padding_left);
    // RUN_TEST(test_ese_gui_style_padding_top);
    // RUN_TEST(test_ese_gui_style_padding_right);
    // RUN_TEST(test_ese_gui_style_padding_bottom);
    // RUN_TEST(test_ese_gui_style_ref);
    // RUN_TEST(test_ese_gui_style_copy_requires_engine);
    // RUN_TEST(test_ese_gui_style_copy);
    // RUN_TEST(test_ese_gui_style_watcher_system);
    // RUN_TEST(test_ese_gui_style_lua_integration);
    // RUN_TEST(test_ese_gui_style_lua_init);
    RUN_TEST(test_ese_gui_style_lua_push);
    // RUN_TEST(test_ese_gui_style_lua_get);
    // RUN_TEST(test_ese_gui_style_serialization);
    // RUN_TEST(test_ese_gui_style_lua_new);
    // RUN_TEST(test_ese_gui_style_lua_properties);
    // RUN_TEST(test_ese_gui_style_lua_tostring);
    // RUN_TEST(test_ese_gui_style_lua_gc);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* C API Test Functions
*/

static void test_ese_gui_style_sizeof(void) {
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, ese_gui_style_sizeof(), "GuiStyle size should be > 0");
}

static void test_ese_gui_style_create_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_gui_style_create(NULL), "ese_gui_style_create should abort with NULL engine");
}

static void test_ese_gui_style_create(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    TEST_ASSERT_NOT_NULL_MESSAGE(style, "GuiStyle should be created");
    TEST_ASSERT_EQUAL_INT_MESSAGE(GUI_STYLE_BORDER_WIDTH_WIDGET_DEFAULT, ese_gui_style_get_border_width(style), "Default border_width should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, ese_gui_style_get_padding_left(style), "Default padding_left should be 4");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, ese_gui_style_get_padding_top(style), "Default padding_top should be 4");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, ese_gui_style_get_padding_right(style), "Default padding_right should be 4");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, ese_gui_style_get_padding_bottom(style), "Default padding_bottom should be 4");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_engine->runtime, ese_gui_style_get_state(style), "GuiStyle should have correct Lua state");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_lua_ref_count(style), "New GuiStyle should have ref count 0");

    // Test that colors are created and have default values
    EseColor *background = ese_gui_style_get_bg(style, GUI_STYLE_VARIANT_LIGHT);
    TEST_ASSERT_NOT_NULL_MESSAGE(background, "Background color should be created");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9725f, ese_color_get_r(background)); // 248/255 ≈ 0.9725
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9765f, ese_color_get_g(background)); // 249/255 ≈ 0.9765
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9804f, ese_color_get_b(background)); // 250/255 ≈ 0.9804
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(background));

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_background(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 255, 0, 0, 255); // Red

    ese_gui_style_set_bg(style, GUI_STYLE_VARIANT_LIGHT, new_color);
    
    EseColor *background = ese_gui_style_get_bg(style, GUI_STYLE_VARIANT_LIGHT);
    TEST_ASSERT_NOT_NULL_MESSAGE(background, "Background color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_r(background));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_g(background));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_b(background));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(background));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_background_hovered(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 0, 255, 0, 255); // Green

    ese_gui_style_set_bg(style, GUI_STYLE_VARIANT_SECONDARY, new_color);
    
    EseColor *background_hovered = ese_gui_style_get_bg(style, GUI_STYLE_VARIANT_SECONDARY);
    TEST_ASSERT_NOT_NULL_MESSAGE(background_hovered, "Background hovered color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_r(background_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_g(background_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_b(background_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(background_hovered));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_background_pressed(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 0, 0, 255, 255); // Blue

    ese_gui_style_set_bg(style, GUI_STYLE_VARIANT_DARK, new_color);
    
    EseColor *background_pressed = ese_gui_style_get_bg(style, GUI_STYLE_VARIANT_DARK);
    TEST_ASSERT_NOT_NULL_MESSAGE(background_pressed, "Background pressed color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_r(background_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_g(background_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_b(background_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(background_pressed));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_border(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 255, 255, 0, 255); // Yellow

    ese_gui_style_set_border(style, GUI_STYLE_VARIANT_PRIMARY, new_color);
    
    EseColor *border = ese_gui_style_get_border(style, GUI_STYLE_VARIANT_PRIMARY);
    TEST_ASSERT_NOT_NULL_MESSAGE(border, "Border color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_r(border));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_g(border));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_b(border));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(border));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_border_hovered(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 255, 0, 255, 255); // Magenta

    ese_gui_style_set_border(style, GUI_STYLE_VARIANT_SECONDARY, new_color);
    
    EseColor *border_hovered = ese_gui_style_get_border(style, GUI_STYLE_VARIANT_SECONDARY);
    TEST_ASSERT_NOT_NULL_MESSAGE(border_hovered, "Border hovered color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_r(border_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_g(border_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_b(border_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(border_hovered));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_border_pressed(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 0, 255, 255, 255); // Cyan

    ese_gui_style_set_border(style, GUI_STYLE_VARIANT_DARK, new_color);
    
    EseColor *border_pressed = ese_gui_style_get_border(style, GUI_STYLE_VARIANT_DARK);
    TEST_ASSERT_NOT_NULL_MESSAGE(border_pressed, "Border pressed color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ese_color_get_r(border_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_g(border_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_b(border_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(border_pressed));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_text(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 128, 128, 128, 255); // Gray

    ese_gui_style_set_text(style, GUI_STYLE_VARIANT_DEFAULT, new_color);
    
    EseColor *text = ese_gui_style_get_text(style, GUI_STYLE_VARIANT_DEFAULT);
    TEST_ASSERT_NOT_NULL_MESSAGE(text, "Text color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ese_color_get_r(text)); // 128/255 ≈ 0.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ese_color_get_g(text));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ese_color_get_b(text));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(text));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_text_hovered(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 64, 64, 64, 255); // Dark gray

    ese_gui_style_set_text(style, GUI_STYLE_VARIANT_DARK, new_color);
    
    EseColor *text_hovered = ese_gui_style_get_text(style, GUI_STYLE_VARIANT_DARK);
    TEST_ASSERT_NOT_NULL_MESSAGE(text_hovered, "Text hovered color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, ese_color_get_r(text_hovered)); // 64/255 ≈ 0.25
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, ese_color_get_g(text_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, ese_color_get_b(text_hovered));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(text_hovered));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_text_pressed(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    EseColor *new_color = ese_color_create(g_engine);
    ese_color_set_byte(new_color, 32, 32, 32, 255); // Very dark gray

    ese_gui_style_set_text(style, GUI_STYLE_VARIANT_WHITE, new_color);
    
    EseColor *text_pressed = ese_gui_style_get_text(style, GUI_STYLE_VARIANT_WHITE);
    TEST_ASSERT_NOT_NULL_MESSAGE(text_pressed, "Text pressed color should exist");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.125f, ese_color_get_r(text_pressed)); // 32/255 ≈ 0.125
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.125f, ese_color_get_g(text_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.125f, ese_color_get_b(text_pressed));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ese_color_get_a(text_pressed));

    ese_color_destroy(new_color);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_border_width(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    ese_gui_style_set_border_width(style, 5);
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, ese_gui_style_get_border_width(style), "Border width should be set to 5");

    ese_gui_style_set_border_width(style, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_border_width(style), "Border width should be set to 0");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_padding_left(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    ese_gui_style_set_padding_left(style, 10);
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, ese_gui_style_get_padding_left(style), "Padding left should be set to 10");

    ese_gui_style_set_padding_left(style, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_padding_left(style), "Padding left should be set to 0");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_padding_top(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    ese_gui_style_set_padding_top(style, 15);
    TEST_ASSERT_EQUAL_INT_MESSAGE(15, ese_gui_style_get_padding_top(style), "Padding top should be set to 15");

    ese_gui_style_set_padding_top(style, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_padding_top(style), "Padding top should be set to 0");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_padding_right(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    ese_gui_style_set_padding_right(style, 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, ese_gui_style_get_padding_right(style), "Padding right should be set to 20");

    ese_gui_style_set_padding_right(style, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_padding_right(style), "Padding right should be set to 0");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_padding_bottom(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    ese_gui_style_set_padding_bottom(style, 25);
    TEST_ASSERT_EQUAL_INT_MESSAGE(25, ese_gui_style_get_padding_bottom(style), "Padding bottom should be set to 25");

    ese_gui_style_set_padding_bottom(style, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_padding_bottom(style), "Padding bottom should be set to 0");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_ref(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_lua_ref_count(style), "Initial ref count should be 0");

    ese_gui_style_ref(style);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, ese_gui_style_get_lua_ref_count(style), "Ref count should be 1 after ref");

    ese_gui_style_unref(style);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ese_gui_style_get_lua_ref_count(style), "Ref count should be 0 after second ref");

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_copy_requires_engine(void) {
    TEST_ASSERT_DEATH(ese_gui_style_copy(NULL), "ese_gui_style_copy should abort with NULL source");
}

static void test_ese_gui_style_copy(void) {
    EseGuiStyle *original = ese_gui_style_create(g_engine);
    ese_gui_style_set_border_width(original, 3);
    ese_gui_style_set_padding_left(original, 10);

    EseColor *test_color = ese_color_create(g_engine);
    ese_color_set_byte(test_color, 255, 0, 0, 255);
    ese_gui_style_set_bg(original, GUI_STYLE_VARIANT_LIGHT, test_color);

    EseGuiStyle *copy = ese_gui_style_copy(original);
    TEST_ASSERT_NOT_NULL_MESSAGE(copy, "Copy should be created");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(original, copy, "Copy should be different object");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_gui_style_get_border_width(original), ese_gui_style_get_border_width(copy), "Border width should be copied");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_gui_style_get_padding_left(original), ese_gui_style_get_padding_left(copy), "Padding left should be copied");

    // Test that colors are copied (not shared)
    EseColor *original_bg = ese_gui_style_get_bg(original, GUI_STYLE_VARIANT_LIGHT);
    EseColor *copy_bg = ese_gui_style_get_bg(copy, GUI_STYLE_VARIANT_LIGHT);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(original_bg, copy_bg, "Color objects should be different");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ese_color_get_r(original_bg), ese_color_get_r(copy_bg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ese_color_get_g(original_bg), ese_color_get_g(copy_bg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ese_color_get_b(original_bg), ese_color_get_b(copy_bg));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ese_color_get_a(original_bg), ese_color_get_a(copy_bg));

    ese_color_destroy(test_color);
    ese_gui_style_destroy(original);
    ese_gui_style_destroy(copy);
}

// Global variables for watcher test
static EseGuiStyle *g_watcher_style = NULL;
static bool g_watcher_called = false;
static void *g_watcher_userdata = NULL;

static void watcher_callback(EseGuiStyle *watched_style, void *data) {
    g_watcher_called = true;
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_watcher_style, watched_style, "Watcher should receive correct style");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(g_watcher_userdata, data, "Watcher should receive correct userdata");
}

static void test_ese_gui_style_watcher_system(void) {
    g_watcher_style = ese_gui_style_create(g_engine);
    g_watcher_called = false;
    g_watcher_userdata = (void*)0x12345678;

    TEST_ASSERT_TRUE_MESSAGE(ese_gui_style_add_watcher(g_watcher_style, watcher_callback, g_watcher_userdata), "Watcher should be added");

    // Change a property to trigger watcher
    ese_gui_style_set_border_width(g_watcher_style, 5);
    TEST_ASSERT_TRUE_MESSAGE(g_watcher_called, "Watcher should be called on property change");

    g_watcher_called = false;
    ese_gui_style_set_border_width(g_watcher_style, 5);
    TEST_ASSERT_TRUE_MESSAGE(g_watcher_called, "Watcher should be called on another property change");

    TEST_ASSERT_TRUE_MESSAGE(ese_gui_style_remove_watcher(g_watcher_style, watcher_callback, &g_watcher_called), "Watcher should be removed");

    g_watcher_called = false;
    ese_gui_style_set_border_width(g_watcher_style, 3);
    TEST_ASSERT_FALSE_MESSAGE(g_watcher_called, "Watcher should not be called after removal");

    ese_gui_style_destroy(g_watcher_style);
    g_watcher_style = NULL;
}

static void test_ese_gui_style_lua_integration(void) {
    EseLuaEngine *engine = create_test_engine();
    EseGuiStyle *style = ese_gui_style_create(engine);

    lua_State *before_state = ese_gui_style_get_state(style);
    TEST_ASSERT_NOT_NULL_MESSAGE(before_state, "Style should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, before_state, "Style state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_gui_style_get_lua_ref(style), "Style should have no Lua reference initially");

    ese_gui_style_ref(style);
    lua_State *after_ref_state = ese_gui_style_get_state(style);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_ref_state, "Style should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_ref_state, "Style state should match engine runtime");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LUA_NOREF, ese_gui_style_get_lua_ref(style), "Style should have a valid Lua reference after ref");

    ese_gui_style_unref(style);
    lua_State *after_unref_state = ese_gui_style_get_state(style);
    TEST_ASSERT_NOT_NULL_MESSAGE(after_unref_state, "Style should have a valid Lua state");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(engine->runtime, after_unref_state, "Style state should match engine runtime");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_NOREF, ese_gui_style_get_lua_ref(style), "Style should have no Lua reference after unref");

    ese_gui_style_destroy(style);
    lua_engine_destroy(engine);
}

static void test_ese_gui_style_lua_init(void) {    
    lua_getglobal(g_engine->runtime, "GuiStyle");
    TEST_ASSERT_TRUE_MESSAGE(lua_istable(g_engine->runtime, -1), "GuiStyle should be in the global table");
    lua_pop(g_engine->runtime, 1);
}

static void test_ese_gui_style_lua_push(void) {
    lua_State *L = g_engine->runtime;
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    
    ese_gui_style_lua_push(style);
    
    EseGuiStyle **ud = (EseGuiStyle **)lua_touserdata(L, -1);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(style, *ud, "The pushed item should be the actual style");
    
    lua_pop(L, 1); 

    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_lua_get(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    lua_State *L = ese_gui_style_get_state(style);
    
    ese_gui_style_lua_push(style);
    EseGuiStyle *retrieved = ese_gui_style_lua_get(L, -1);
    
    TEST_ASSERT_EQUAL_PTR_MESSAGE(style, retrieved, "Retrieved style should match original");
    
    // Test with invalid input
    lua_pushnil(L);
    EseGuiStyle *invalid = ese_gui_style_lua_get(L, -1);
    TEST_ASSERT_NULL_MESSAGE(invalid, "Invalid input should return NULL");
    
    lua_pop(L, 2);
    ese_gui_style_destroy(style);
}

static void test_ese_gui_style_serialization(void) {
    EseGuiStyle *style = ese_gui_style_create(g_engine);
    ese_gui_style_set_border_width(style, 3);
    ese_gui_style_set_font_size(style, 14);
    ese_gui_style_set_padding_left(style, 10);

    cJSON *json = ese_gui_style_serialize(style);
    TEST_ASSERT_NOT_NULL_MESSAGE(json, "Serialization should produce JSON");
    
    // Test deserialization
    EseGuiStyle *deserialized = ese_gui_style_deserialize(g_engine, json);
    TEST_ASSERT_NOT_NULL_MESSAGE(deserialized, "Deserialization should create style");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_gui_style_get_border_width(style), ese_gui_style_get_border_width(deserialized), "Border width should be preserved");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_gui_style_get_padding_left(style), ese_gui_style_get_padding_left(deserialized), "Padding left should be preserved");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ese_gui_style_get_font_size(style), ese_gui_style_get_font_size(deserialized), "Font size should be preserved");

    cJSON_Delete(json);
    ese_gui_style_destroy(style);
    ese_gui_style_destroy(deserialized);
}

/**
* Lua API Test Functions
*/

static void test_ese_gui_style_lua_new(void) {
    lua_State *L = g_engine->runtime;
    
    const char *testC = "return GuiStyle.new()\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testC), "testC Lua code should execute without error");
    EseGuiStyle *extracted_style = ese_gui_style_lua_get(L, -1);
    TEST_ASSERT_NOT_NULL_MESSAGE(extracted_style, "Extracted style should not be NULL");
    lua_pop(L, 1);

    const char *testE = "return GuiStyle.new(\"foo\")\n";
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testE), "string args should error (no arguments required)");
    lua_pop(L, 1);
}

static void test_ese_gui_style_lua_properties(void) {
    // Script that sets/gets properties via Lua and returns boolean
    const char *script =
        "function GS.run()\n"
        "  local s = GuiStyle.new()\n"
        "  s.font_size = 1\n"
        "  s.border_width = 5\n"
        "  return (s.font_size == 1 and s.border_width == 5)\n"
        "end\n";

    TEST_ASSERT_TRUE_MESSAGE(lua_engine_load_script_from_string(g_engine, script, "gui_style_props", "GS"), "Failed to load script");
    int instance_ref = lua_engine_instance_script(g_engine, "gui_style_props");
    TEST_ASSERT_TRUE_MESSAGE(instance_ref > 0, "Failed to instance script");

    EseLuaValue *result = lua_value_create_nil("result");
    bool ok = lua_engine_run_function(g_engine, instance_ref, instance_ref, "run", 0, NULL, result);
    TEST_ASSERT_TRUE_MESSAGE(ok, "Failed to run script function");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_bool(result) && lua_value_get_bool(result), "Script should return true");
    lua_value_destroy(result);
}

static void test_ese_gui_style_lua_tostring(void) {
    // Script that checks tostring(s) returns a string
    const char *script =
        "function GS.run()\n"
        "  local s = GuiStyle.new()\n"
        "  local str = tostring(s)\n"
        "  return type(str) == 'string'\n"
        "end\n";

    TEST_ASSERT_TRUE_MESSAGE(lua_engine_load_script_from_string(g_engine, script, "gui_style_tostring", "GS"), "Failed to load script");
    int instance_ref = lua_engine_instance_script(g_engine, "gui_style_tostring");
    TEST_ASSERT_TRUE_MESSAGE(instance_ref > 0, "Failed to instance script");

    EseLuaValue *result = lua_value_create_nil("result");
    bool ok = lua_engine_run_function(g_engine, instance_ref, instance_ref, "run", 0, NULL, result);
    TEST_ASSERT_TRUE_MESSAGE(ok, "Failed to run script function");
    TEST_ASSERT_TRUE_MESSAGE(lua_value_is_bool(result) && lua_value_get_bool(result), "Script should return true");
    lua_value_destroy(result);
}

static void test_ese_gui_style_lua_gc(void) {
    ese_color_lua_init(g_engine);
    lua_State *L = g_engine->runtime;

    const char *testA = "local s = GuiStyle.new()";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, testA), "Style creation should execute without error");
    
    int collected = lua_gc(L, LUA_GCCOLLECT, 0);
    TEST_ASSERT_TRUE_MESSAGE(collected >= 0, "Garbage collection should collect");

    // Verify GC didn't crash by running another operation
    const char *verify_code = "return 42";
    TEST_ASSERT_EQUAL_INT_MESSAGE(LUA_OK, luaL_dostring(L, verify_code), "Lua should still work after GC");
    int result = (int)lua_tonumber(L, -1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result, "Lua should return correct value after GC");
    lua_pop(L, 1);
}
