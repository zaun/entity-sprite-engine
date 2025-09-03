#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include "test_utils.h"
#include "utility/profile.h"
#include "core/engine_private.h"
#include "entity/components/entity_component.h"
#include "entity/components/entity_component_lua.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "entity/entity_lua.h"
#include "scripting/lua_engine.h"
#include "scripting/lua_value.h"
#include "platform/time.h"
#include "types/types.h"

#define TOTAL_CALLS 1000

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const char* test_lua_engine_script = 
"function ENTITY:entity_init()\n"
"    -- Set random initial velocity for the ball\n"
"    local angle = math.random() * 2 * math.pi\n"
"    local speed = 200 + math.random() * 200  -- Speed between 200-400 pixels/second\n"
"    self.data.velocity = Vector.new(\n"
"        math.cos(angle) * speed,\n"
"        math.sin(angle) * speed\n"
"    )\n"
"    self.data.size = 16\n"
"end\n"
"\n"
"function ENTITY:entity_update(delta_time)\n"
"    -- Update ball position based on velocity\n"
"    local current_pos = self.position\n"
"    local new_x = current_pos.x + self.data.velocity.x * delta_time\n"
"    local new_y = current_pos.y + self.data.velocity.y * delta_time\n"
"    \n"
"    -- Check screen boundaries and bounce\n"
"    local viewport_width = Display.viewport.width\n"
"    local viewport_height = Display.viewport.height\n"
"    local ball_size = self.data.size\n"
"    \n"
"    -- Bounce off left and right walls\n"
"    if new_x <= 0 then\n"
"        self.data.velocity.x = -self.data.velocity.x\n"
"        new_x = 0\n"
"    elseif new_x >= viewport_width - ball_size then\n"
"        self.data.velocity.x = -self.data.velocity.x\n"
"        new_x = viewport_width - ball_size\n"
"    end\n"
"    \n"
"    -- Bounce off top and bottom walls\n"
"    if new_y <= 0 then\n"
"        self.data.velocity.y = -self.data.velocity.y\n"
"        new_y = 0\n"
"    elseif new_y >= viewport_height - ball_size then\n"
"        self.data.velocity.y = -self.data.velocity.y\n"
"        new_y = viewport_height - ball_size\n"
"    end\n"
"\n"
"    -- Update ball position\n"
"    self.position = Point.new(new_x, new_y)\n"
"end\n"
"\n"
"function ENTITY:entity_collision_enter(entity) \n"
"end\n"
"\n"
"function ENTITY:entity_collision_stay(entity) \n"
"end\n"
"\n"
"function ENTITY:entity_collision_exit(entity) \n"
"end\n";


void segfault_handler(int signo, siginfo_t *info, void *context) {
    printf("\n=== SEGFAULT DETECTED ===\n");
    printf("Signal: %d\n", signo);
    printf("Address: %p\n", info->si_addr);
    printf("Code: %d\n", info->si_code);
    exit(1);
}

void dump_globals(lua_State *L) {
    printf("Globals:\n");
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        printf("  %s = %s\n",
               lua_tostring(L, -2),
               lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void dump_registry(lua_State *L) {
    printf("Registry dump:\n");
    lua_pushnil(L);
    while (lua_next(L, LUA_REGISTRYINDEX) != 0) {
        printf("  key: %s, value: %s\n",
               lua_typename(L, lua_type(L, -2)),
               lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
    }

    dump_globals(L);
}

static void test_lua_engine() {
    test_begin("Lua Engine only");

    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Lua Engine only");
        return;
    }

    TEST_ASSERT_NOT_NULL(engine->runtime, "Engine runtime should not be NULL");
    TEST_ASSERT_NOT_NULL(engine->internal, "Engine internal should not be NULL");

    // Add types
    vector_lua_init(engine);
    point_lua_init(engine);
    display_state_lua_init(engine);
    entity_lua_init(engine);

    // Create a display
    EseDisplay *display = display_state_create(engine);
    display->width = 800;
    display->height = 600;
    display->viewport.width = 800;
    display->viewport.height = 600;
    display_state_ref(display);
    lua_engine_add_global(engine, "Display", display->lua_ref);

    // Create a dummy entity
    EseEntity *entity = entity_create(engine);

    // Load the test script
    bool load_result = lua_engine_load_script_from_string(engine, test_lua_engine_script, "benchmark_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    if (!load_result) {
        lua_engine_destroy(engine);
        test_end("Lua Engine only");
        return;
    }

    // Create an instance of the script
    int instance_ref = lua_engine_instance_script(engine, "benchmark_script");
    TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
    if (instance_ref < 0) {
        lua_engine_destroy(engine);
        test_end("Lua Engine only");
        return;
    }

    lua_rawgeti(engine->runtime, LUA_REGISTRYINDEX, instance_ref);
    lua_getfield(engine->runtime, -1, "entity_update");
    int function_ref = luaL_ref(engine->runtime, LUA_REGISTRYINDEX);
    lua_pop(engine->runtime, 1); // pop instance table
    
    lua_engine_run_function(engine, instance_ref, entity->lua_ref, "entity_init", 1, NULL, NULL);

    EseLuaValue *delta_time = lua_value_create_number("delta_time", 0.016);

    dump_registry(engine->runtime);

    // Run the ENITY:entity_update function
    int max_stack_top = 0;  
    for (int i = 0; i < TOTAL_CALLS; i++) {
        max_stack_top = MAX(max_stack_top, lua_gettop(engine->runtime));
        EseLuaValue *args[] = {delta_time};
        bool result = lua_engine_run_function_ref(engine, function_ref, entity->lua_ref, 1, args, NULL);
        if (!result) {
            TEST_ASSERT(result, "Function should run successfully");
            break;
        }
    }

    printf("Stack top: %d\n", max_stack_top);

    lua_engine_destroy(engine);

    profile_display();
    profile_reset_all();
    
    test_end("Lua Engine only");
}

static void test_lua_component() {
    test_begin("Lua Component");

    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Lua Component");
        return;
    }

    TEST_ASSERT_NOT_NULL(engine->runtime, "Engine runtime should not be NULL");
    TEST_ASSERT_NOT_NULL(engine->internal, "Engine internal should not be NULL");

    // Add types
    vector_lua_init(engine);
    point_lua_init(engine);
    display_state_lua_init(engine);
    entity_lua_init(engine);

    // Create a display
    EseDisplay *display = display_state_create(engine);
    display->width = 800;
    display->height = 600;
    display->viewport.width = 800;
    display->viewport.height = 600;
    display_state_ref(display);
    lua_engine_add_global(engine, "Display", display->lua_ref);

    // Create a dummy entity
    EseEntity *entity = entity_create(engine);

    // Load the test script
    bool load_result = lua_engine_load_script_from_string(engine, test_lua_engine_script, "benchmark_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    if (!load_result) {
        lua_engine_destroy(engine);
        test_end("Lua Component");
        return;
    }

    // Create an instance of the script
    int instance_ref = lua_engine_instance_script(engine, "benchmark_script");
    TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
    if (instance_ref < 0) {
        lua_engine_destroy(engine);
        test_end("Lua Component");
        return;
    }

    // Create a component
    EseEntityComponent *component = entity_component_lua_create(engine, "benchmark_script");
    TEST_ASSERT_NOT_NULL(component, "Component should be created");
    if (!component) {
        lua_engine_destroy(engine);
        test_end("Lua Component");
        return;
    }

    lua_engine_run_function(engine, instance_ref, entity->lua_ref, "entity_init", 1, NULL, NULL);

    dump_registry(engine->runtime);

    // Update the component
    int max_stack_top = 0;
    for (int i = 0; i < TOTAL_CALLS; i++) {
        max_stack_top = MAX(max_stack_top, lua_gettop(engine->runtime));
        entity_component_update(component, entity, 0.016);
    }
    printf("Stack top: %d\n", max_stack_top);

    lua_engine_destroy(engine);

    profile_display();
    profile_reset_all();
    
    test_end("Lua Component");
}

static void test_entity_update() {
    test_begin("Entity Update");

    EseLuaEngine *engine = lua_engine_create();
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Entity Update");
        return;
    }

    TEST_ASSERT_NOT_NULL(engine->runtime, "Engine runtime should not be NULL");
    TEST_ASSERT_NOT_NULL(engine->internal, "Engine internal should not be NULL");

    // Add types
    vector_lua_init(engine);
    point_lua_init(engine);
    display_state_lua_init(engine);
    entity_lua_init(engine);

    // Create a display
    EseDisplay *display = display_state_create(engine);
    display->width = 800;
    display->height = 600;
    display->viewport.width = 800;
    display->viewport.height = 600;
    display_state_ref(display);
    lua_engine_add_global(engine, "Display", display->lua_ref);

    // Create an entity
    EseEntity *entity = entity_create(engine);

    // Load the test script
    bool load_result = lua_engine_load_script_from_string(engine, test_lua_engine_script, "benchmark_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    if (!load_result) {
        lua_engine_destroy(engine);
        test_end("Entity Update");
        return;
    }

    // Create an instance of the script
    int instance_ref = lua_engine_instance_script(engine, "benchmark_script");
    TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
    if (instance_ref < 0) {
        lua_engine_destroy(engine);
        test_end("Entity Update");
        return;
    }

    // Create a component
    EseEntityComponent *component = entity_component_lua_create(engine, "benchmark_script");
    TEST_ASSERT_NOT_NULL(component, "Component should be created");
    if (!component) {
        lua_engine_destroy(engine);
        test_end("Entity Update");
        return;
    }
    
    entity_component_add(entity, component);

    lua_engine_run_function(engine, instance_ref, entity->lua_ref, "entity_init", 1, NULL, NULL);

    dump_registry(engine->runtime);

    // Update the component
    int max_stack_top = 0;
    for (int i = 0; i < TOTAL_CALLS; i++) {
        max_stack_top = MAX(max_stack_top, lua_gettop(engine->runtime));
        entity_update(entity, 0.016);
    }
    printf("Stack top: %d\n", max_stack_top);

    lua_engine_destroy(engine);

    profile_display();
    profile_reset_all();
    
    test_end("Entity Update");
}

static void test_engine_update() {
    test_begin("Engine Update");

    EseEngine *engine = engine_create(NULL);
    TEST_ASSERT_NOT_NULL(engine, "Engine should not be NULL");
    if (!engine) {
        test_end("Engine Update");
        return;
    }
    engine_start(engine);

    display_state_set_dimensions(engine->display_state, 800, 600);
    display_state_set_viewport(engine->display_state, 800, 600);

    EseInputState input_state;
    memset(&input_state, 0, sizeof(EseInputState));

    // Create an entity
    EseEntity *entity = entity_create(engine->lua_engine);

    // Load the test script
    bool load_result = lua_engine_load_script_from_string(engine->lua_engine, test_lua_engine_script, "benchmark_script", "ENTITY");
    TEST_ASSERT(load_result, "Test script should load successfully");
    if (!load_result) {
        engine_destroy(engine);
        test_end("Engine Update");
        return;
    }

    // Create an instance of the script
    int instance_ref = lua_engine_instance_script(engine->lua_engine, "benchmark_script");
    TEST_ASSERT(instance_ref > 0, "Script instance should be created successfully");
    if (instance_ref < 0) {
        engine_destroy(engine);
        test_end("Engine Update");
        return;
    }

    // Create a component
    EseEntityComponent *component = entity_component_lua_create(engine->lua_engine, "benchmark_script");
    TEST_ASSERT_NOT_NULL(component, "Component should be created");
    if (!component) {
        engine_destroy(engine);
        test_end("Engine Update");
        return;
    }
    
    entity_component_add(entity, component);
    engine_add_entity(engine, entity);

    lua_engine_run_function(engine->lua_engine, instance_ref, entity->lua_ref, "entity_init", 1, NULL, NULL);

    dump_registry(engine->lua_engine->runtime);

    // Update the component
    int max_stack_top = 0;
    for (int i = 0; i < TOTAL_CALLS; i++) {
        max_stack_top = MAX(max_stack_top, lua_gettop(engine->lua_engine->runtime));
        engine_update(engine, 0.016, &input_state);
    }
    printf("Stack top: %d\n", max_stack_top);
    engine_destroy(engine);

    profile_display();
    profile_reset_all();
    
    test_end("Engine Update");
}

int main() {
    // Set up signal handler for debugging
    struct sigaction sa;
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    test_suite_begin("🧪 Starting Bottleneck Tests");
    
    // Test the entity_update function
    test_lua_engine();
    test_lua_component();
    test_entity_update();
    test_engine_update();

    test_suite_end("🧪 Starting Bottleneck Tests");
        
    return 0;
}
