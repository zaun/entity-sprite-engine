cmake_minimum_required(VERSION 3.16)

project(entitysprite_tests LANGUAGES C)

enable_testing()
include(FetchContent)

option(BUILD_COVERAGE "Build tests with coverage flags" OFF)

if (BUILD_COVERAGE)
    message(STATUS "Building tests with coverage instrumentation")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage flags requested but unsupported for this compiler")
    endif()
endif()

# === Fetch Unity (ThrowTheSwitch) ===
set(UNITY_TAG "v2.5.2") # change tag if you want a different release

FetchContent_Declare(
  unity_src
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG        ${UNITY_TAG}
)

FetchContent_MakeAvailable(unity_src)

# Unity upstream may already create a target named "unity".
# Only add our own target if one doesn't already exist.
if (NOT TARGET unity)
    add_library(unity STATIC "${unity_src_SOURCE_DIR}/src/unity.c")
    target_include_directories(unity PUBLIC "${unity_src_SOURCE_DIR}/src")
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_CONFIG_H=0)
else()
    # Ensure include dir is available (some Unity CMake targets may already set it)
    get_target_property(_unity_inc unity INTERFACE_INCLUDE_DIRECTORIES)
    if (NOT _unity_inc)
        target_include_directories(unity PUBLIC "${unity_src_SOURCE_DIR}/src")
    endif()
endif()

# === Locate main project library and Lua static lib ===
set(PROJECT_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(LUA_STATIC_LIB "${CMAKE_SOURCE_DIR}/src/vendor/lua/src/libluajit.a")

# === Discover test sources (test_*.c in this directory) ===
file(GLOB TEST_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "test_*.c")

if (TEST_SOURCES STREQUAL "")
    message(WARNING "No test_*.c sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${test_src}" NAME_WE)
    add_executable(${test_name} "${test_src}")

    target_include_directories(${test_name} PRIVATE "${PROJECT_SRC_DIR}")
    target_link_libraries(${test_name} PRIVATE unity entityspriteengine)

    add_test(NAME ${test_name} COMMAND ${test_name})

    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "ESETEST=1"
    )
endforeach()

# Convenience target to run tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES}
)

# === Coverage target (uses lcov + genhtml if available) ===
if (BUILD_COVERAGE)
    find_program(LCOV_EXEC lcov)
    find_program(GENHTML_EXEC genhtml)

    if (NOT LCOV_EXEC)
        message(WARNING "lcov not found; coverage target will run tests but cannot produce HTML report")
    endif()
    if (NOT GENHTML_EXEC)
        message(WARNING "genhtml not found; coverage HTML report won't be generated")
    endif()

    set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
    set(COVERAGE_INFO "${COVERAGE_OUTPUT_DIR}/coverage.info")
    set(COVERAGE_FILTERED "${COVERAGE_OUTPUT_DIR}/coverage.filtered.info")
    set(COVERAGE_HTML_DIR "${COVERAGE_OUTPUT_DIR}/html")

    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
        # zero counters
        COMMAND ${LCOV_EXEC} --directory . --zerocounters
        # capture coverage
        COMMAND ${LCOV_EXEC} --directory . --capture --output-file ${COVERAGE_INFO}
        # remove system and test runner files
        COMMAND ${LCOV_EXEC} --remove ${COVERAGE_INFO} '/usr/*' '${CMAKE_BINARY_DIR}/*' --output-file ${COVERAGE_FILTERED}
        # optionally generate HTML
        COMMAND ${GENHTML_EXEC} ${COVERAGE_FILTERED} --output-directory ${COVERAGE_HTML_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report (if lcov/genhtml available)"
        VERBATIM
        DEPENDS ${TEST_SOURCES}
    )
endif()