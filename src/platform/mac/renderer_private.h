#ifndef ESE_METAL_RENDERER_PRIVATE_H
#define ESE_METAL_RENDERER_PRIVATE_H

#import <Foundation/NSObjCRuntime.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "utility/hashmap.h"
#import "core/memory_manager.h"
#import "graphics/render_list.h"
#import "platform/renderer.h"

// Forward declare Objective-C classes for C compatibility
@class MTKTextureLoader;
@class RendererViewDelegate;

/**
 * @brief Platform-specific renderer structure for macOS Metal implementation.
 * 
 * @details This structure manages Metal-specific rendering resources including
 *          Metal device, command queue, pipeline state, and various buffers.
 *          It provides the Metal implementation of the platform-agnostic renderer
 *          with support for efficient GPU rendering and texture management.
 */
typedef struct EseMetalRenderer {
    MTKView *view;                 /**< MetalKit view for rendering */
    RendererViewDelegate *viewDelegate; /**< Delegate for view events and rendering */

    MTKTextureLoader *textureLoader; /**< Metal texture loader for asset loading */

    id<MTLDevice> device;          /**< Metal device for GPU operations */
    MTLClearColor clearColor;      /**< Background clear color */
    MTLPixelFormat colorPixelFormat; /**< Color pixel format for the render target */

    id<MTLCommandQueue> commandQueue; /**< Command queue for GPU command submission */
    id<MTLRenderPipelineState> pipelineState; /**< Render pipeline state for shaders */

    id<MTLBuffer> render_listBuffer; /**< Buffer for render list data */

    // Reusable buffers to avoid per-batch allocations
    id<MTLBuffer> vertexBuffer;    /**< Reusable vertex buffer for rendering */
    size_t perFrameVertexCapacity; /**< Vertex capacity per frame */
    size_t frameVertexCursor;      /**< Current vertex position in buffer */
    NSUInteger inflightCount;      /**< Number of inflight frames */
    NSUInteger inflightIndex;      /**< Current inflight frame index */
    id<MTLBuffer> uboBuffer;       /**< Uniform buffer for shader parameters */
    size_t vertexBufferCapacity;   /**< Total vertex buffer capacity */
} EseMetalRenderer;

bool _renderer_shader_compile_source(EseRenderer* renderer, const char *library_name, NSString *sourceString);

#endif // ESE_METAL_RENDERER_H
