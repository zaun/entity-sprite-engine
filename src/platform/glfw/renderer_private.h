#ifndef ESE_GL_RENDERER_PRIVATE_H
#define ESE_GL_RENDERER_PRIVATE_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "utility/hashmap.h"
#include "core/memory_manager.h"
#include "graphics/render_list.h"
#include "platform/renderer.h"

typedef struct EseGLRenderer {
    GLFWwindow *window;
    GLuint shaderProgram;
    GLuint vao;
    GLuint vbo;
    size_t vbo_capacity;
    GLuint ubo;
} EseGLRenderer;

typedef struct {
    GLuint id;
    int width;
    int height;
} GLTexture;

bool _renderer_shader_compile_source(EseRenderer *renderer,
                                     const char *library_name,
                                     const char *sourceString);

#endif // ESE_GL_RENDERER_PRIVATE_H
