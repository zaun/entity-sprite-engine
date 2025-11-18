cmake_minimum_required(VERSION 3.16)

# Pass SANITIZE option to all examples
if(SANITIZE)
    message(STATUS "Sanitizer enabled for examples")
endif()

add_subdirectory(audio)
add_subdirectory(bounce)
add_subdirectory(breakout)
add_subdirectory(collisions)
add_subdirectory(shapes)
add_subdirectory(simple)
