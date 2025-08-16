#ifndef ESE_RENDERER_PRIVATE_H
#define ESE_RENDERER_PRIVATE_H

#include <stdbool.h>
#include <simd/simd.h>

typedef struct EseRenderList EseRenderList;
typedef struct EseHashMap EseHashMap;
typedef struct EseGroupedHashMap EseGroupedHashMap;

typedef struct {
    simd_int1   useTexture;
    simd_float4 rectColor;
} UniformBufferObject;

typedef struct EseRenderer {
    void *internal;                     // Interanl data for each renderer

    bool hiDPI;                         // True if the display should be hiDPI

    EseHashMap *textures;        // key: const char*
    EseGroupedHashMap *shaders;         // key: const char*
    EseGroupedHashMap *shadersSources;  // key: const char*

    EseRenderList *render_list;

    float view_w;
    float view_h;
} EseRenderer;

#endif // ESE_RENDERER_PRIVATE_H
