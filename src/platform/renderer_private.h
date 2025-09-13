#ifndef ESE_RENDERER_PRIVATE_H
#define ESE_RENDERER_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct EseRenderList EseRenderList;
typedef struct EseHashMap EseHashMap;
typedef struct EseGroupedHashMap EseGroupedHashMap;

/**
 * @brief Platform-agnostic vector types for rendering.
 * 
 * @details These types provide consistent vector operations across
 *          different platforms and graphics APIs.
 */
typedef struct {
    float x, y, z, w;
} EseVector4;

typedef struct {
    int32_t x;
} EseVector1i;

/**
 * @brief Uniform buffer object for shader parameters.
 * 
 * @details This structure defines the layout of uniform data passed to
 *          shaders, including texture usage flags and rectangle color
 *          information for rendering decisions.
 */
typedef struct {
    EseVector1i useTexture;         /**< Flag indicating whether to use texture (1) or color (0) */
    uint32_t    _pad0[3];           /**< Padding for std140 alignment */
    EseVector4  color;              /**< RGBA color for rendering */
    EseVector4  tint;               /**< RGBA tint for texture rendering */
    float       opacity;            /**< Opacity for rendering */
    float       _pad[3];            /**< Padding for std140 alignment */
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
