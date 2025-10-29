# CMakeTools.cmake - Additional CMake targets for development tools

# Find clang-format
find_program(CLANG_FORMAT clang-format)

if(CLANG_FORMAT)
    # Define source file patterns
    set(FORMAT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/*.c
        ${PROJECT_SOURCE_DIR}/src/*.cpp
        ${PROJECT_SOURCE_DIR}/src/*.m
        ${PROJECT_SOURCE_DIR}/src/*.h
    )
    
    # Collect all source files matching the patterns
    file(GLOB_RECURSE ALL_SOURCES ${FORMAT_SOURCES})
    
    # Filter out vendor directory files
    set(FORMAT_SOURCES_FILTERED)
    foreach(SOURCE_FILE ${ALL_SOURCES})
        string(FIND ${SOURCE_FILE} "/vendor/" VENDOR_POS)
        if(VENDOR_POS EQUAL -1)
            list(APPEND FORMAT_SOURCES_FILTERED ${SOURCE_FILE})
        endif()
    endforeach()
    
    # Add format target
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_SOURCES_FILTERED}
        COMMENT "Formatting source files with clang-format"
        VERBATIM
    )
    
    message(STATUS "clang-format found: ${CLANG_FORMAT}")
    message(STATUS "Format target added - use 'make format' to format source files")
else()
    message(WARNING "clang-format not found - format target not available")
endif()
