#ifndef ESE_RENDERER_H
#define ESE_RENDERER_H

#include <stdbool.h>

// Opaque pointer to the real EseRenderer struct
typedef struct EseRenderer EseRenderer;
typedef struct EseRenderList EseRenderList;

// C API functions
#ifdef __cplusplus
extern "C" {
#endif

EseRenderer *renderer_create(bool hiDPI);
void renderer_destroy(EseRenderer *dev);

bool renderer_shader_compile(EseRenderer *renderer, const char *library,
                             const char *filename);
bool renderer_create_pipeline_state(EseRenderer *dev, const char *vertexFunc,
                                    const char *fragmentFunc);

bool renderer_load_texture(EseRenderer *renderer, const char *texture_id,
                           const unsigned char *rgba_data, int width,
                           int height);

bool renderer_set_render_list(EseRenderer *dev, EseRenderList *render_list);
EseRenderList *renderer_get_render_list(EseRenderer *dev);
bool renderer_clear_render_list(EseRenderer *dev);

void renderer_draw(EseRenderer *renderer);

bool renderer_get_size(EseRenderer *dev, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // ESE_RENDERER_H
