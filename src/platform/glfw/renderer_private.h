#ifndef ESE_GL_RENDERER_PRIVATE_H
#define ESE_GL_RENDERER_PRIVATE_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "utility/hashmap.h"
#include "core/memory_manager.h"
#include "graphics/render_list.h"
#include "platform/renderer.h"

/**
 * @brief Platform-specific renderer structure for OpenGL implementation.
 * 
 * @details This structure manages OpenGL-specific rendering resources including
 *          shader programs, vertex arrays, vertex buffers, and uniform buffers.
 *          It provides the OpenGL implementation of the platform-agnostic renderer.
 */
typedef struct EseGLRenderer {
    GLFWwindow *window;            /** GLFW window handle for OpenGL context */
    GLuint shaderProgram;          /** OpenGL shader program ID */
    GLuint vao;                    /** Vertex array object ID */
    GLuint vbo;                    /** Vertex buffer object ID */
    size_t vbo_capacity;           /** Allocated capacity of vertex buffer */
    GLuint ubo;                    /** Uniform buffer object ID */
} EseGLRenderer;

/**
 * @brief OpenGL texture metadata structure.
 * 
 * @details This structure stores OpenGL texture information including
 *          the texture ID and dimensions for texture management and
 *          rendering operations.
 */
typedef struct {
    GLuint id;                     /** OpenGL texture ID */
    int width;                     /** Texture width in pixels */
    int height;                    /** Texture height in pixels */
} GLTexture;

bool _renderer_shader_compile_source(EseRenderer *renderer,
                                     const char *library_name,
                                     const char *sourceString);

#endif // ESE_GL_RENDERER_PRIVATE_H
