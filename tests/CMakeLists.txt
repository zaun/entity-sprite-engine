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

# Link against the main library
target_link_libraries(test_rect entityspriteengine)
target_link_libraries(test_point entityspriteengine)
target_link_libraries(test_lua_engine entityspriteengine)
target_link_libraries(test_lua_engine_benchmark entityspriteengine)

# Include directories
target_include_directories(test_rect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_point PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_lua_engine_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Add tests
add_test(NAME RectTests COMMAND test_rect)
add_test(NAME PointTests COMMAND test_point)
add_test(NAME LuaEngineTests COMMAND test_lua_engine)
add_test(NAME LuaEngineBenchmarkTests COMMAND test_lua_engine_benchmark)

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
