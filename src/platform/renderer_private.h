#ifndef ESE_RENDERER_PRIVATE_H
#define ESE_RENDERER_PRIVATE_H

#include <stdbool.h>
#include <simd/simd.h>

typedef struct EseRenderList EseRenderList;
typedef struct EseHashMap EseHashMap;
typedef struct EseGroupedHashMap EseGroupedHashMap;

/**
 * @brief Uniform buffer object for shader parameters.
 * 
 * @details This structure defines the layout of uniform data passed to
 *          shaders, including texture usage flags and rectangle color
 *          information for rendering decisions.
 */
typedef struct {
    simd_int1   useTexture;         /**< Flag indicating whether to use texture (1) or color (0) */
    simd_float4 rectColor;          /**< RGBA color for rectangle rendering */
} UniformBufferObject;

/**
 * @brief Platform-agnostic renderer interface.
 * 
 * @details This structure provides a unified interface for rendering
 *          operations across different graphics APIs (Metal, OpenGL).
 *          It manages textures, shaders, render lists, and viewport
 *          dimensions for consistent rendering behavior.
 */
typedef struct EseRenderer {
    void *internal;                 /**< Platform-specific internal data */

    bool hiDPI;                     /**< True if the display should be hiDPI */

    EseHashMap *textures;           /**< Hash map of loaded textures by ID */
    EseGroupedHashMap *shaders;     /**< Hash map of compiled shaders by group and ID */
    EseGroupedHashMap *shadersSources; /**< Hash map of shader source code by group and ID */

    EseRenderList *render_list;     /**< Current render list for drawing operations */

    float view_w;                   /**< Viewport width for coordinate calculations */
    float view_h;                   /**< Viewport height for coordinate calculations */
} EseRenderer;

#endif // ESE_RENDERER_PRIVATE_H
