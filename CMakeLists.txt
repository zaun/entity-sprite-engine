cmake_minimum_required(VERSION 3.16)

# Platform Detection
option(FORCE_PLATFORM_LINUX "Force the use of the Linux platform" OFF)
option(FORCE_PLATFORM_MAC "Force the use of the MacOS platform" OFF)

# Check for conflicting options.
if(FORCE_PLATFORM_LINUX AND FORCE_PLATFORM_MAC)
    message(FATAL_ERROR "Cannot force both Linux and MacOS. Please choose one.")
endif()

if(APPLE)
    set(PLATFORM "MAC")
else ()
    set(PLATFORM "LINUX")
endif()
if(FORCE_PLATFORM_LINUX)
    set(PLATFORM "LINUX")
endif()
if(FORCE_PLATFORM_MAC)
    set(PLATFORM "MAC")
endif()

# Set languages based on platform
if(PLATFORM STREQUAL "MAC")
    set(PROJECT_LANGUAGES "C;OBJC;CXX")
else()
    set(PROJECT_LANGUAGES "C;CXX")
endif()

project(entityspriteengine LANGUAGES ${PROJECT_LANGUAGES})

# Enable profiling
# add_definitions(-DESE_PROFILE_ENABLED)

#
# Platform
#

# Conditionally include platform specific code.
if(PLATFORM STREQUAL "MAC")
    message(STATUS "Building with Mac platform")
    add_definitions(-DPLATFORM="MAC")
    find_library(METAL_LIB Metal REQUIRED)
    file(GLOB PLATFORM_SOURCES 
        "src/platform/mac/window.m"
        "src/platform/mac/app_delegate.m"
        "src/platform/mac/filesystem.m"
        "src/platform/mac/renderer.m"
        "src/platform/mac/renderer_delegate.m"
        "src/platform/mac/time.m"
    )
elseif(PLATFORM STREQUAL "LINUX")
    message(STATUS "Building with Linux platform")
    add_definitions(-DPLATFORM="LINUX")
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    file(GLOB PLATFORM_SOURCES 
        "src/platform/glfw/window.c"
        "src/platform/glfw/filesystem.c"
        "src/platform/glfw/renderer.c"
        "src/platform/glfw/time.c"
    )
else()
    # This case should ideally not be reached, but it's good practice to handle it.
    message(FATAL_ERROR "Unknown platform specified: ${PLATFORM}")
endif()

# Find all other non-renderer source files.
# It is best practice to list these explicitly for a robust build,
# but using a non-recursive glob to match the user's intent.
file(GLOB ENGINE_CORE_SOURCES
    "src/vendor/json/*.c"
    "src/core/*.c"
    "src/core/**/*.c"
    "src/entity/*.c"
    "src/entity/**/*.c"
    "src/graphics/*.c"
    "src/graphics/**/*.c"
    "src/graphics/*.cpp"
    "src/graphics/**/*.cpp"
    "src/scripting/*.c"
    "src/scripting/**/*.c"
    "src/types/*.c"
    "src/types/**/*.c"
    "src/utility/*.c"
    "src/utility/**/*.c"
)

# Combine all sources
set(ENGINE_SOURCES ${ENGINE_CORE_SOURCES} ${PLATFORM_SOURCES})

# Create a custom target to build the Lua library.
if(PLATFORM STREQUAL "MAC")
    add_custom_target(build_lua
        COMMAND ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=15.0 make
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src/vendor/lua/src"
        COMMENT "Building Lua library with its native Makefile..."
    )
else()
    add_custom_target(build_lua
        COMMAND ${CMAKE_COMMAND} -E chdir "${PROJECT_SOURCE_DIR}/src/vendor/lua/src" make
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src/vendor/lua/src"
        COMMENT "Building Lua library with its native Makefile..."
    )
endif()

# Add the static library target for the engine.
add_library(entityspriteengine STATIC ${ENGINE_SOURCES})

# Set up the dependencies: the engine library build must wait for Lua to be built.
add_dependencies(entityspriteengine build_lua)

# Set renderer include directories.
if(PLATFORM STREQUAL "MAC")
    target_include_directories(entityspriteengine PUBLIC
        "${PROJECT_SOURCE_DIR}/src/platform/macos"
    )
elseif(PLATFORM STREQUAL "LINUX")
    target_include_directories(entityspriteengine PUBLIC
        "${PROJECT_SOURCE_DIR}/src/platform/glfw"
    )
endif()

# Set common include directories.
target_include_directories(entityspriteengine PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/vendor/lua/src"
)

# Link the engine library to the Lua static library and other system libraries.
target_link_libraries(entityspriteengine PRIVATE
    "${PROJECT_SOURCE_DIR}/src/vendor/lua/src/libluajit.a"
)

# Conditionally link platform-specific frameworks and libraries
if(PLATFORM STREQUAL "MAC")
    find_library(COCOA_LIB Cocoa)
    find_library(METAL_LIB Metal)
    find_library(METALKIT_LIB MetalKit)
    find_library(IOKIT_LIB IOKit)
    find_library(COREVIDEO_LIB CoreVideo)
    find_library(CORE_FOUNDATION_LIB CoreFoundation)
    find_library(QUARTZ_CORE_LIB QuartzCore)
    find_library(IMAGEIO_LIB ImageIO)
    find_library(CORE_GRAPHICS_LIB CoreGraphics)

    target_link_libraries(entityspriteengine PRIVATE
        ${COCOA_LIB}
        ${METAL_LIB}
        ${METALKIT_LIB}
        ${IOKIT_LIB}
        ${COREVIDEO_LIB}
        ${CORE_FOUNDATION_LIB}
        ${QUARTZ_CORE_LIB}
        ${IMAGEIO_LIB}
        ${CORE_GRAPHICS_LIB}
    )
elseif(PLATFORM STREQUAL "LINUX")
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)

    target_include_directories(entityspriteengine PRIVATE
        ${OPENGL_INCLUDE_DIR}
        ${GLFW3_INCLUDE_DIR}
        ${GLEW_INCLUDE_DIRS}
    )

    target_link_libraries(entityspriteengine PRIVATE
        OpenGL::GL
        glfw
        GLEW::GLEW
    )
endif()

include(FetchContent)

FetchContent_Declare(
  spirv-headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG main
)
FetchContent_MakeAvailable(spirv-headers)

FetchContent_Declare(
  spirv-tools
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
  GIT_TAG main
)
set(SPIRV_HEADERS_SOURCE_DIR ${spirv-headers_SOURCE_DIR} CACHE PATH "" FORCE)
set(ENABLE_OPT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spirv-tools)

FetchContent_Declare(
  glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG main
)
set(SPIRV_TOOLS_ROOT ${spirv-tools_SOURCE_DIR} CACHE PATH "" FORCE)
set(ENABLE_OPT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glslang)

FetchContent_Declare(
  spirv-cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG main  # Use main to avoid tag errors; switch to stable tag later if needed
)
FetchContent_MakeAvailable(spirv-cross)

message(STATUS "SPIRV-Cross source dir: ${spirv-cross_SOURCE_DIR}")

target_include_directories(entityspriteengine PRIVATE
  ${glslang_SOURCE_DIR}/glslang/Public
  ${glslang_SOURCE_DIR}/SPIRV
  ${spirv-cross_SOURCE_DIR}
  ${spirv-tools_SOURCE_DIR}/include
)

target_link_libraries(entityspriteengine PRIVATE glslang spirv-cross-c)

# Add the examples subdirectory.
add_subdirectory(examples)

# Add the tests subdirectory.
add_subdirectory(tests)


