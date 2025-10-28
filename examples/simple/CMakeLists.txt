cmake_minimum_required(VERSION 3.16)

# Platform-specific build configuration
if(PLATFORM STREQUAL "MAC")
    # Create the executable as a macOS application bundle.
    add_executable(simple_demo MACOSX_BUNDLE main.c)

    # Set the Info.plist file.
    set_target_properties(simple_demo PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    )

    find_library(COCOA_LIB Cocoa)
    find_library(METAL_LIB Metal)
    find_library(METALKIT_LIB MetalKit)
    find_library(IOKIT_LIB IOKit)
    find_library(COREVIDEO_LIB CoreVideo)
    find_library(CORE_FOUNDATION_LIB CoreFoundation)
    find_library(QUARTZ_CORE_LIB QuartzCore)
    find_library(IMAGEIO_LIB ImageIO)
    find_library(CORE_GRAPHICS_LIB CoreGraphics)

    target_link_libraries(simple_demo PRIVATE
        entityspriteengine
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

    target_link_options(simple_demo PRIVATE
        "-Wl,-rpath,@executable_path/../Frameworks"
    )

    # Add debugging and sanitization flags if SANITIZE is enabled
    if(SANITIZE)
        target_compile_options(simple_demo PRIVATE
            -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(simple_demo PRIVATE
            -fsanitize=address,undefined
        )
        target_link_libraries(simple_demo PRIVATE glslang-default-resource-limits stdc++)
    endif()

    # Always sync assets at build time so changes are picked up by `make`
    add_custom_target(simple_assets_sync ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simple_demo>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            "$<TARGET_FILE_DIR:simple_demo>/resources/"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
            "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Info.plist"
        COMMENT "Syncing simple_demo assets"
        VERBATIM
    )
    add_dependencies(simple_assets_sync simple_demo)

    # Copy assets to macOS bundle Resources directory
    add_custom_command(
        TARGET simple_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Resources/"
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        "$<TARGET_BUNDLE_CONTENT_DIR:simple_demo>/Info.plist"
        VERBATIM
    )

    # Also copy assets to resources/ directory for cross-platform filesystem access
    add_custom_command(
        TARGET simple_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simple_demo>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        "$<TARGET_FILE_DIR:simple_demo>/resources/"
        VERBATIM
    )

elseif(PLATFORM STREQUAL "LINUX")
    # Create a regular Linux executable
    add_executable(simple_demo main.c)

    # Link with the engine library
    target_link_libraries(simple_demo PRIVATE entityspriteengine)

    # Add debugging and sanitization flags if SANITIZE is enabled
    if(SANITIZE)
        target_compile_options(simple_demo PRIVATE
            -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(simple_demo PRIVATE
            -fsanitize=address,undefined
        )
        target_link_libraries(simple_demo PRIVATE glslang-default-resource-limits stdc++)
    endif()

    # Always sync assets at build time so changes are picked up by `make`
    add_custom_target(simple_assets_sync ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simple_demo>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            "$<TARGET_FILE_DIR:simple_demo>/resources/"
        COMMENT "Syncing simple_demo assets"
        VERBATIM
    )
    add_dependencies(simple_assets_sync simple_demo)

    # Copy assets to a resources directory next to the executable
    add_custom_command(
        TARGET simple_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simple_demo>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        "$<TARGET_FILE_DIR:simple_demo>/resources/"
        VERBATIM
    )

else()
    message(FATAL_ERROR "Unknown platform: ${PLATFORM}. Expected 'MAC' or 'LINUX'")
endif()