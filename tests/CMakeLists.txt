# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages
find_package(PkgConfig REQUIRED)

# Test executables
add_executable(test_rect test_rect.c)
add_executable(test_point test_point.c)
add_executable(test_vector test_vector.c)
add_executable(test_ray test_ray.c)
add_executable(test_uuid test_uuid.c)
add_executable(test_arc test_arc.c)
add_executable(test_tileset test_tileset.c)
add_executable(test_camera test_camera.c)
add_executable(test_display test_display.c)
add_executable(test_input_state test_input_state.c)
add_executable(test_map_cell test_map_cell.c)
add_executable(test_map test_map.c)
add_executable(test_lua_engine test_lua_engine.c)
add_executable(test_lua_value test_lua_value.c)
add_executable(test_log_assert test_log_assert.c)
add_executable(test_entity test_entity.c)
add_executable(test_entity_component_lua test_entity_component_lua.c)
add_executable(test_engine test_engine.c)
add_executable(test_engine_collision test_engine_collision.c)
add_executable(test_lua_only test_lua_only.c)
add_executable(test_bottleneck test_bottleneck.c)
add_executable(test_audio test_audio.c)

add_dependencies(test_lua_only build_lua)

# Link against the main library
target_link_libraries(test_rect entityspriteengine)
target_link_libraries(test_point entityspriteengine)
target_link_libraries(test_vector entityspriteengine)
target_link_libraries(test_ray entityspriteengine)
target_link_libraries(test_uuid entityspriteengine)
target_link_libraries(test_arc entityspriteengine)
target_link_libraries(test_tileset entityspriteengine)
target_link_libraries(test_camera entityspriteengine)
target_link_libraries(test_display entityspriteengine)
target_link_libraries(test_input_state entityspriteengine)
target_link_libraries(test_map_cell entityspriteengine)
target_link_libraries(test_map entityspriteengine)
target_link_libraries(test_lua_engine entityspriteengine)
target_link_libraries(test_lua_value entityspriteengine)
target_link_libraries(test_log_assert entityspriteengine)
target_link_libraries(test_entity entityspriteengine)
target_link_libraries(test_entity_component_lua entityspriteengine)
target_link_libraries(test_engine entityspriteengine)
target_link_libraries(test_engine_collision entityspriteengine)
target_link_libraries(test_lua_only "${CMAKE_SOURCE_DIR}/src/vendor/lua/src/libluajit.a" entityspriteengine)
target_link_libraries(test_bottleneck entityspriteengine)
target_link_libraries(test_audio entityspriteengine)


# Include directories
target_include_directories(test_rect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_point PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_vector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_ray PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_uuid PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_arc PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_tileset PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_camera PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_display PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_input_state PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_map_cell PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_map PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_value PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_log_assert PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_entity PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_entity_component_lua PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_engine_collision PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_only PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_bottleneck PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_audio PRIVATE ${CMAKE_SOURCE_DIR}/src)


# Add tests
add_test(NAME RectTests COMMAND test_rect)
add_test(NAME PointTests COMMAND test_point)
add_test(NAME VectorTests COMMAND test_vector)
add_test(NAME RayTests COMMAND test_ray)
add_test(NAME UUIDTests COMMAND test_uuid)
add_test(NAME ArcTests COMMAND test_arc)
add_test(NAME TilesetTests COMMAND test_tileset)
add_test(NAME CameraTests COMMAND test_camera)
add_test(NAME DisplayTests COMMAND test_display)
add_test(NAME InputStateTests COMMAND test_input_state)
add_test(NAME MapCellTests COMMAND test_map_cell)
add_test(NAME MapTests COMMAND test_map)
add_test(NAME LuaEngineTests COMMAND test_lua_engine)
add_test(NAME LuaValueTests COMMAND test_lua_value)
add_test(NAME LogAssertTests COMMAND test_log_assert)
add_test(NAME EntityTests COMMAND test_entity)
add_test(NAME EntityComponentLuaTests COMMAND test_entity_component_lua)
add_test(NAME EngineTests COMMAND test_engine)
add_test(NAME EngineCollisionTests COMMAND test_engine_collision)
add_test(NAME LuaOnlyTests COMMAND test_lua_only)
add_test(NAME BottleneckTests COMMAND test_bottleneck)
add_test(NAME AudioTests COMMAND test_audio)


# Set test properties
set_tests_properties(RectTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(PointTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(VectorTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(RayTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(UUIDTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(ArcTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(TilesetTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(CameraTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(DisplayTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(InputStateTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(MapCellTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(MapTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaEngineTests PROPERTIES
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

set_tests_properties(BottleneckTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(EntityComponentLuaTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(EngineTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(EngineCollisionTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(LuaOnlyTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)

set_tests_properties(AudioTests PROPERTIES
    ENVIRONMENT "ESETEST=1"
)


