// OpenGL Implementation of EseRenderer
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "platform/glfw/renderer_private.h"
#include "platform/default_shader.h"
#include "platform/renderer.h"
#include "platform/renderer_private.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include "graphics/render_list.h"
#include "graphics/shader.h"
#include "utility/hashmap.h"
#include "utility/grouped_hashmap.h"
#include "platform/filesystem.h"

// Forward declarations
static void _gl_free_texture(void *value);
static void _gl_free_shader(void *value);

// Internal helper to free GLTexture objects
static void _gl_free_texture(void *value) {
    if (value) {
        GLTexture *tex_data = (GLTexture *)value;
        glDeleteTextures(1, &tex_data->id);
        memory_manager.free(tex_data);
    }
}

// Internal helper to free shader objects
static void _gl_free_shader(void *value) {
    if (value) {
        GLuint *shader_id = (GLuint *)value;
        glDeleteShader(*shader_id);
        memory_manager.free(shader_id);
    }
}

#define MAX_BATCH_VERTICES 100000

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

void _split_library_func(const char *input, char **group, char **name) {
    log_assert("GL_RENDERER", input, "_split_library_func called with NULL input");
    log_assert("GL_RENDERER", group, "_split_library_func called with NULL group");
    log_assert("GL_RENDERER", name, "_split_library_func called with NULL name");

    // Initialize output pointers to NULL
    *group = NULL;
    *name = NULL;

    // Handle NULL input immediately
    if (input == NULL)
    {
        return;
    }

    const char *colon = strchr(input, ':');

    if (colon == NULL)
    {
        // No colon: full string is the name, group is "default"
        *group = memory_manager.strdup("default", MMTAG_RENDERER);
        *name = memory_manager.strdup(input, MMTAG_RENDERER);
    }
    else
    {
        // Calculate lengths of potential group and name parts
        size_t groupLength = colon - input;
        size_t nameLength = strlen(colon + 1);

        // Case: "test:" or ":" (group exists, name is empty)
        if (nameLength == 0)
        {
            return;
        }

        // Case: ":test" (no group, name exists)
        if (groupLength == 0)
        {
            *group = memory_manager.strdup("default", MMTAG_RENDERER);
            *name = memory_manager.strdup(colon + 1, MMTAG_RENDERER);
        }
        // Case: "group:test" (both group and name exist)
        else
        {
            *group = (char *)memory_manager.malloc(groupLength + 1, MMTAG_RENDERER);
            if (*group)
            {
                strncpy(*group, input, groupLength);
                (*group)[groupLength] = '\0';
            }
            *name = memory_manager.strdup(colon + 1, MMTAG_RENDERER);
        }
    }
}

EseRenderer* renderer_create(bool hiDPI) {
    log_debug("RENDERER", "Initializing OpenGL Renderer...");

    // Allocate the renderer struct
    EseRenderer *renderer = (EseRenderer *)memory_manager.malloc(sizeof(EseRenderer), MMTAG_RENDERER);
    EseGLRenderer *internal = (EseGLRenderer *)memory_manager.malloc(sizeof(EseGLRenderer), MMTAG_RENDERER);

    renderer->internal = (void *) internal;
    renderer->textures = hashmap_create((EseHashMapFreeFn)_gl_free_texture);
    renderer->shaders = grouped_hashmap_create((EseGroupedHashMapFreeFn)_gl_free_shader);
    renderer->shadersSources = grouped_hashmap_create((EseGroupedHashMapFreeFn)memory_manager.free);
    renderer->hiDPI = hiDPI;
    
    internal->window = NULL;
    internal->shaderProgram = 0;
    internal->vao = 0;
    internal->vbo = 0;
    internal->vbo_capacity = 0;

    _renderer_shader_compile_source(renderer, "default", DEFAULT_SHADER);
    renderer_create_pipeline_state(renderer, "default:vertexShader", "default:fragmentShader");

    return renderer;
}

void renderer_destroy(EseRenderer *renderer) {
    log_assert("GL_RENDERER", renderer, "renderer_destroy called with NULL renderer");

    EseGLRenderer *internal = (EseGLRenderer *)renderer->internal;
    if (internal->shaderProgram != 0) {
        glDeleteProgram(internal->shaderProgram);
    }
    if (internal->vao != 0) {
        glDeleteVertexArrays(1, &internal->vao);
    }
    if (internal->vbo != 0) {
        glDeleteBuffers(1, &internal->vbo);
    }
    memory_manager.free(renderer->internal);

    // Hashmaps will automatically free their values using the free functions
    hashmap_free(renderer->textures);
    grouped_hashmap_free(renderer->shaders);
    grouped_hashmap_free(renderer->shadersSources);

    memory_manager.free(renderer);
}

bool _renderer_shader_compile_source(EseRenderer *renderer, const char *library_name, const char *sourceString) {
    log_assert("GL_RENDERER", renderer, "_renderer_shader_compile_source called with NULL renderer");
    log_assert("GL_RENDERER", library_name, "_renderer_shader_compile_source called with NULL library_name");
    log_assert("GL_RENDERER", sourceString, "_renderer_shader_compile_source called with NULL sourceString");

    ShaderBlob vs = glsl_to_glsl(sourceString, 0); // Vert
    ShaderBlob fs = glsl_to_glsl(sourceString, 4); // Frag
    ShaderBlob cs = glsl_to_glsl(sourceString, 5); // Comp

    // Vert and Frag shaders are required.
    if (!vs.data || !fs.data)
    {
        log_debug("RENDERER", "Shader compilation failed:\n  vs: %p\n  fs: %p", vs.data, fs.data);
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        return false;
    }

    // debug
    log_debug("SHADER", "Vertex Shader:\n%s", vs.data);
    log_debug("SHADER", "Fragment Shader:\n%s", fs.data);
    // log_debug("SHADER", "Compute Shader:\n%s", cs.data);

    int success;
    char infoLog[512];

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *vertexShaderSource = vs.data;
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        log_debug("SHADER", "Vertex compile error: %s", infoLog);
        glDeleteShader(vertexShader);
        return false;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *fragmentShaderSource = fs.data;
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        log_debug("SHADER", "Fragment compile error: %s", infoLog);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    GLuint *vertexShaderId = memory_manager.malloc(sizeof(GLuint), MMTAG_RENDERER);
    *vertexShaderId = vertexShader;
    grouped_hashmap_set(
        renderer->shaders,
        library_name, "vertexShader",
        vertexShaderId);
    grouped_hashmap_set(
        renderer->shadersSources,
        library_name, "vertexShader",
        memory_manager.strdup(vs.data, MMTAG_RENDERER));

    GLuint *fragmentShaderId = memory_manager.malloc(sizeof(GLuint), MMTAG_RENDERER);
    *fragmentShaderId = fragmentShader;
    grouped_hashmap_set(
        renderer->shaders,
        library_name, "fragmentShader",
        fragmentShaderId);
    grouped_hashmap_set(
        renderer->shadersSources,
        library_name, "fragmentShader",
        memory_manager.strdup(fs.data, MMTAG_RENDERER));

    if (cs.data)
    {
        grouped_hashmap_set(
            renderer->shadersSources,
            library_name, "computeShader",
            memory_manager.strdup(cs.data, MMTAG_RENDERER));
    }

    free_shader_blob(vs);
    free_shader_blob(fs);
    free_shader_blob(cs);

    log_debug("RENDERER", "Compiled library %s.", library_name);

    return true;
}

bool renderer_shader_compile(EseRenderer *renderer, const char *library, const char *filename){
    log_assert("GL_RENDERER", renderer, "renderer_shader_compile called with NULL renderer");
    log_assert("GL_RENDERER", library, "renderer_shader_compile called with NULL library");
    log_assert("GL_RENDERER", filename, "renderer_shader_compile called with NULL filename");

    char *path = filesystem_get_resource(filename);

    FILE *file = fopen(path, "r");
    if (!file)
    {
        log_debug("RENDERER", "Failed to open shader file: %s", filename);
        memory_manager.free(path);
        return false;
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length == -1)
    {
        log_debug("RENDERER", "Failed to get file size for file: %s", filename);
        fclose(file);
        memory_manager.free(path);
        return false;
    }

    // Read the file into the buffer
    char *source = (char *)memory_manager.malloc(length + 1, MMTAG_RENDERER);
    size_t read_bytes = fread(source, 1, length, file);
    if (read_bytes != (size_t)length)
    {
        log_debug("RENDERER", "Failed to read full shader file: %s", filename);
        memory_manager.free(source);
        fclose(file);
        memory_manager.free(path);
        return false;
    }

    // Null-terminate the string
    source[length] = '\0';

    fclose(file);
    memory_manager.free(path);

    bool status = _renderer_shader_compile_source(renderer, library, source);
    memory_manager.free(source);

    return status;
}

// Loads, compiles, and links a shader program.
bool renderer_create_pipeline_state(EseRenderer *renderer, const char *vertexFunc, const char *fragmentFunc) {
    log_assert("GL_RENDERER", renderer, "renderer_create_pipeline_state called with NULL renderer");
    log_assert("GL_RENDERER", vertexFunc, "renderer_create_pipeline_state called with NULL vertexFunc");
    log_assert("GL_RENDERER", fragmentFunc, "renderer_create_pipeline_state called with NULL fragmentFunc");

    EseGLRenderer *internal = (EseGLRenderer *)renderer->internal;

    int success;
    char infoLog[512];

    char *vLib = NULL;
    char *vFunc = NULL;
    char *fLib = NULL;
    char *fFunc = NULL;

    // Parse vertex function string
    _split_library_func(vertexFunc, &vLib, &vFunc);
    if (!vLib)
    {
        log_debug("RENDERER", "_split_library_func failed for %s", vertexFunc);
        return false;
    }

    _split_library_func(fragmentFunc, &fLib, &fFunc);
    if (!fLib)
    {
        log_debug("RENDERER", "_split_library_func failed for %s", fragmentFunc);
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        return false;
    }

    GLuint *vertexShaderId = grouped_hashmap_get(renderer->shaders, vLib, vFunc);
    if (!vertexShaderId)
    {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        memory_manager.free(fLib);
        memory_manager.free(fFunc);
        log_debug("RENDERER", "Vertex shader not found: %s", vertexFunc);
        return false;
    }

    GLuint *fragmentShaderId = grouped_hashmap_get(renderer->shaders, fLib, fFunc);
    if (!fragmentShaderId)
    {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        memory_manager.free(fLib);
        memory_manager.free(fFunc);
        log_debug("RENDERER", "Fragment shader not found: %s", fragmentFunc);
        return false;
    }

    memory_manager.free(vLib);
    memory_manager.free(vFunc);
    memory_manager.free(fLib);
    memory_manager.free(fFunc);

    internal->shaderProgram = glCreateProgram();
    glAttachShader(internal->shaderProgram, *vertexShaderId);
    glAttachShader(internal->shaderProgram, *fragmentShaderId);
    glLinkProgram(internal->shaderProgram);

    glGetProgramiv(internal->shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(internal->shaderProgram, 512, NULL, infoLog);
        glDeleteProgram(internal->shaderProgram);
        log_debug("RENDERER", "Failed to create pipeline: %s", infoLog);
        return false;
    }

    // After creating pipeline state, add:
    glGenVertexArrays(1, &internal->vao);
    glGenBuffers(1, &internal->vbo);

    glBindVertexArray(internal->vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->vbo);
    internal->vbo_capacity = MAX_BATCH_VERTICES * 5 * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, internal->vbo_capacity, NULL, GL_DYNAMIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return true;
}

bool renderer_load_texture(EseRenderer *renderer, const char *id, const unsigned char *rgba_data, int width, int height) {
    log_assert("GL_RENDERER", renderer, "renderer_load_texture called with NULL renderer");
    log_assert("GL_RENDERER", id, "renderer_load_texture called with NULL id");
    log_assert("GL_RENDERER", rgba_data, "renderer_load_texture called with NULL rgba_data");
    log_assert("GL_RENDERER", width > 0, "renderer_load_texture called with invalid width");
    log_assert("GL_RENDERER", height > 0, "renderer_load_texture called with invalid height");

    // Check if texture already loaded
    if (hashmap_get(renderer->textures, id)) {
        log_debug("GL_RENDERER", "Texture already loaded (%s)", id);
        return true;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);

    // Create and populate the GLTexture struct
    GLTexture *tex_data = memory_manager.malloc(sizeof(GLTexture), MMTAG_RENDERER);
    if (tex_data)
    {
        tex_data->id = texture_id;
        tex_data->width = width;
        tex_data->height = height;
        hashmap_set(renderer->textures, id, tex_data);
        log_debug("RENDERER_GL", "Loaded raw texture (%s) %dx%d", id, width, height);
    }
    else
    {
        // Failed to allocate, clean up texture
        glDeleteTextures(1, &texture_id);
        fprintf(stderr, "Failed to allocate GLTexture struct\n");
        return false;
    }

    return true;
}

bool renderer_set_render_list(EseRenderer *renderer, EseRenderList *render_list) {
    log_assert("GL_RENDERER", renderer, "renderer_set_render_list called with NULL renderer");
    log_assert("GL_RENDERER", render_list, "renderer_set_render_list called with NULL render_list");

    renderer->render_list = render_list;

    return renderer->render_list != NULL;
}

EseRenderList *renderer_get_render_list(EseRenderer *renderer) {
    log_assert("GL_RENDERER", renderer, "renderer_get_render_list called with NULL renderer");

    return renderer->render_list;
}

bool renderer_clear_render_list(EseRenderer *renderer) {
    log_assert("GL_RENDERER", renderer, "renderer_clear_render_list called with NULL renderer");

    renderer->render_list = NULL;
    return true;
}

void renderer_draw(EseRenderer *renderer) {
    log_assert("GL_RENDERER", renderer, "renderer_draw called with NULL renderer");

    EseGLRenderer *internal = (EseGLRenderer *)renderer->internal;
    if (!internal) {
        return;
    }

    // Clear the screen to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (renderer->render_list && render_list_get_batch_count(renderer->render_list) > 0) {
        size_t numBatches = render_list_get_batch_count(renderer->render_list);

        // --- GL State Setup (ONCE per frame) ---
        glUseProgram(internal->shaderProgram);
        glBindVertexArray(internal->vao);
        glBindBuffer(GL_ARRAY_BUFFER, internal->vbo);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Get uniform locations ONCE per frame
        GLint useTextureLocation = glGetUniformLocation(internal->shaderProgram, "ubo.useTexture");
        GLint rectColorLocation = glGetUniformLocation(internal->shaderProgram, "ubo.rectColor");
        GLint tintLocation       = glGetUniformLocation(internal->shaderProgram, "ubo.tint");
        GLint opacityLocation    = glGetUniformLocation(internal->shaderProgram, "ubo.opacity");
        GLint textureLocation = glGetUniformLocation(internal->shaderProgram, "ourTexture");
        
        // Set the texture sampler to use texture unit 0
        if (textureLocation != -1) {
            glUniform1i(textureLocation, 0);
        }

        // --- Batch Drawing Loop ---
        for (size_t i = 0; i < numBatches; ++i) {
            const EseRenderBatch *batch = render_list_get_batch(renderer->render_list, i);
            
            if (batch->vertex_count == 0) continue;

            // defaults for optional fields
            float tint_r = 1.0f, tint_g = 1.0f, tint_b = 1.0f, tint_a = 1.0f;
            float opacity = 1.0f;

            if (batch->type == RL_TEXTURE) {
                // Handle texture batch
                GLTexture* tex_data = (GLTexture*)hashmap_get(renderer->textures, batch->shared_state.texture_id);
                if (!tex_data) {
                    log_debug("GL_RENDERER", "Unable to find texture %s", batch->shared_state.texture_id);
                    continue;
                }

                // Bind the texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex_data->id);

                // Set uniforms for texture rendering
                if (useTextureLocation != -1) {
                    glUniform1ui(useTextureLocation, 1);
                } else {
                    log_debug("GL_RENDERER", "Invalid texture uniform location");
                }
                if (tintLocation != -1) {
                    glUniform4f(tintLocation, tint_r, tint_g, tint_b, tint_a);
                }
                if (opacityLocation != -1) {
                    glUniform1f(opacityLocation, opacity);
                }

                // Upload vertex data to VBO
                size_t data_size = batch->vertex_count * sizeof(EseVertex);
                if (data_size > internal->vbo_capacity) {
                    internal->vbo_capacity = max(internal->vbo_capacity * 2, data_size * 2);
                    glBufferData(GL_ARRAY_BUFFER, internal->vbo_capacity, batch->vertex_buffer, GL_DYNAMIC_DRAW);
                } else {
                    glBufferSubData(GL_ARRAY_BUFFER, 0, data_size, batch->vertex_buffer);
                }

                // Draw the batch
                glDrawArrays(GL_TRIANGLES, 0, batch->vertex_count);

            } else if (batch->type == RL_COLOR) {
                // Handle rectangle batch
                // Unbind texture for solid color rendering
                glBindTexture(GL_TEXTURE_2D, 0);

                // Set uniforms for solid color rendering
                if (useTextureLocation != -1) {
                    glUniform1ui(useTextureLocation, 0);
                }
                if (tintLocation != -1) {
                    glUniform4f(tintLocation, tint_r, tint_g, tint_b, tint_a);
                }
                if (opacityLocation != -1) {
                    glUniform1f(opacityLocation, opacity);
                }
                if (rectColorLocation != -1) {
                    glUniform4f(rectColorLocation, 
                               (float)batch->shared_state.color.r / 255.0f,
                               (float)batch->shared_state.color.g / 255.0f,
                               (float)batch->shared_state.color.b / 255.0f,
                               (float)batch->shared_state.color.a / 255.0f);
                }

                // Upload vertex data to VBO
                size_t data_size = batch->vertex_count * sizeof(EseVertex);
                if (data_size > internal->vbo_capacity) {
                    internal->vbo_capacity = max(internal->vbo_capacity * 2, data_size * 2);
                    glBufferData(GL_ARRAY_BUFFER, internal->vbo_capacity, batch->vertex_buffer, GL_DYNAMIC_DRAW);
                } else {
                    glBufferSubData(GL_ARRAY_BUFFER, 0, data_size, batch->vertex_buffer);
                }

                // Draw the batch
                glDrawArrays(GL_TRIANGLES, 0, batch->vertex_count);
            }
        }
        
        // --- Cleanup ---
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDisable(GL_BLEND);
    }
}

bool renderer_get_size(EseRenderer *renderer, int *width, int *height) {
    log_assert("GL_RENDERER", renderer, "renderer_get_size called with NULL renderer");
    
    if (width) *width = renderer->view_w;
    if (height) *height = renderer->view_h;
    
    return true;
}
