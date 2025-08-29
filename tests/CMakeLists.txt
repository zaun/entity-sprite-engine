# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages
find_package(PkgConfig REQUIRED)

# Test executables
add_executable(test_rect test_rect.c)
add_executable(test_point test_point.c)
add_executable(test_lua_engine test_lua_engine.c)
add_executable(test_lua_engine_benchmark test_lua_engine_benchmark.c)
add_executable(test_lua_value test_lua_value.c)
add_executable(test_log_assert test_log_assert.c)
add_executable(test_entity test_entity.c)
add_executable(test_entity_component_lua test_entity_component_lua.c)
add_executable(test_lua_only test_lua_only.c)
add_dependencies(test_lua_only build_lua)

# Link against the main library
target_link_libraries(test_rect entityspriteengine)
target_link_libraries(test_point entityspriteengine)
target_link_libraries(test_lua_engine entityspriteengine)
target_link_libraries(test_lua_engine_benchmark entityspriteengine)
target_link_libraries(test_lua_value entityspriteengine)
target_link_libraries(test_log_assert entityspriteengine)
target_link_libraries(test_entity entityspriteengine)
target_link_libraries(test_entity_component_lua entityspriteengine)
target_link_libraries(test_lua_only "${CMAKE_SOURCE_DIR}/src/vendor/lua/src/libluajit.a")

# Include directories
target_include_directories(test_rect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_point PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_engine_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_value PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_log_assert PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_entity PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_entity_component_lua PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_only PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Add tests
add_test(NAME RectTests COMMAND test_rect)
add_test(NAME PointTests COMMAND test_point)
add_test(NAME LuaEngineTests COMMAND test_lua_engine)
add_test(NAME LuaEngineBenchmarkTests COMMAND test_lua_engine_benchmark)
add_test(NAME LuaValueTests COMMAND test_lua_value)
add_test(NAME LogAssertTests COMMAND test_log_assert)
add_test(NAME EntityTests COMMAND test_entity)
add_test(NAME EntityComponentLuaTests COMMAND test_entity_component_lua)
add_test(NAME LuaOnlyTests COMMAND test_lua_only)

# Set test properties
set_tests_properties(RectTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(PointTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaEngineTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaEngineBenchmarkTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaValueTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LogAssertTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(EntityTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(EntityComponentLuaTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaOnlyTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)
