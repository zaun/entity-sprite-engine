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

typedef struct EseMetalRenderer {
    MTKView *view;
    RendererViewDelegate *viewDelegate;

    MTKTextureLoader *textureLoader;

    id<MTLDevice> device;
    MTLClearColor clearColor;
    MTLPixelFormat colorPixelFormat;

    id<MTLCommandQueue> commandQueue;
    id<MTLRenderPipelineState> pipelineState;

    id<MTLBuffer> render_listBuffer;
} EseMetalRenderer;

bool _renderer_shader_compile_source(EseRenderer* renderer, const char *library_name, NSString *sourceString);

#endif // ESE_METAL_RENDERER_H
