// Metal Implementation of EseRenderer
#import "platform/renderer.h"
#import "core/memory_manager.h"
#import "graphics/render_list.h"
#import "graphics/shader.h"
#import "platform/default_shader.h"
#import "platform/filesystem.h"
#import "platform/mac/renderer_delegate.h"
#import "platform/mac/renderer_private.h"
#import "platform/renderer_private.h"
#import "utility/grouped_hashmap.h"
#import "utility/helpers.h"
#import "utility/log.h"
#import "vendor/stb/stb_image.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <unistd.h>

void _free_hash_item(void *value) {
#if !__has_feature(objc_arc)
    [(id)value release];
#endif
}

bool _renderer_shader_compile_source(EseRenderer *renderer, const char *library_name,
                                     NSString *sourceString) {
    log_assert("METAL_RENDERER", renderer,
               "_renderer_shader_compile_source called with NULL renderer");
    log_assert("METAL_RENDERER", library_name,
               "_renderer_shader_compile_source called with NULL library_name");
    log_assert("METAL_RENDERER", sourceString,
               "_renderer_shader_compile_source called with NULL sourceString");

    ShaderBlob vs = glsl_to_metal([sourceString UTF8String], 0); // Vert
    ShaderBlob fs = glsl_to_metal([sourceString UTF8String], 4); // Frag
    ShaderBlob cs = glsl_to_metal([sourceString UTF8String], 5); // Comp

    // Vert and Frag shaders are required.
    if (!vs.data || !fs.data) {
        log_debug("RENDERER", "Shader compilation failed:\n  vs: %p\n  fs: %p", vs.data, fs.data);
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        return false;
    }

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;
    NSError *error = nil;
    NSString *shaderSourceString = [NSString stringWithUTF8String:vs.data];
    id<MTLLibrary> vertexLibrary = [internal->device newLibraryWithSource:shaderSourceString
                                                                  options:nil
                                                                    error:&error];
    if (!vertexLibrary) {
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name,
                  error.localizedDescription.UTF8String);
        return false;
    }
    grouped_hashmap_set(renderer->shaders, library_name, "vertexShader", [vertexLibrary retain]);
    grouped_hashmap_set(renderer->shadersSources, library_name, "vertexShader",
                        [shaderSourceString retain]);

    log_debug("RENDERER", "Vertex shader source: %s.", shaderSourceString.UTF8String);

    shaderSourceString = [NSString stringWithUTF8String:fs.data];
    id<MTLLibrary> fragmentLibrary = [internal->device newLibraryWithSource:shaderSourceString
                                                                    options:nil
                                                                      error:&error];
    if (!fragmentLibrary) {
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name,
                  error.localizedDescription.UTF8String);
        return false;
    }
    grouped_hashmap_set(renderer->shaders, library_name, "fragmentShader",
                        [fragmentLibrary retain]);
    grouped_hashmap_set(renderer->shadersSources, library_name, "fragmentShader",
                        [shaderSourceString retain]);

    log_debug("RENDERER", "Fragment shader source: %s.", shaderSourceString.UTF8String);

    if (cs.data) {
        shaderSourceString = [NSString stringWithUTF8String:cs.data];
        id<MTLLibrary> computeLibrary = [internal->device newLibraryWithSource:shaderSourceString
                                                                       options:nil
                                                                         error:&error];
        if (!computeLibrary) {
            free_shader_blob(vs);
            free_shader_blob(fs);
            free_shader_blob(cs);
            log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name,
                      error.localizedDescription.UTF8String);
            return false;
        }
        grouped_hashmap_set(renderer->shaders, library_name, "computeShader",
                            [computeLibrary retain]);
        grouped_hashmap_set(renderer->shadersSources, library_name, "computeShader",
                            [shaderSourceString retain]);
    }

    log_debug("RENDERER", "Compiled library %s.", library_name);

    free_shader_blob(vs);
    free_shader_blob(fs);
    free_shader_blob(cs);

    return true;
}

static inline size_t _align_up(size_t v, size_t a) { return (v + (a - 1)) & ~(a - 1); }

static void _ensure_vertex_ring_buffer(EseMetalRenderer *internal, size_t perFrameNeeded) {
    if (!internal)
        return;

    NSUInteger inflight = internal->inflightCount ? internal->inflightCount : 3;
    size_t currentPerFrame = internal->perFrameVertexCapacity;
    if (currentPerFrame >= perFrameNeeded && internal->vertexBuffer)
        return;

    size_t newPerFrame = currentPerFrame ? currentPerFrame * 2 : (1 << 20);
    while (newPerFrame < perFrameNeeded)
        newPerFrame *= 2;
    if (newPerFrame < perFrameNeeded)
        newPerFrame = perFrameNeeded;

    size_t total = newPerFrame * inflight;
    id<MTLBuffer> newBuf = [internal->device newBufferWithLength:total
                                                         options:MTLResourceStorageModeShared];
    if (!newBuf)
        return;

    if (internal->vertexBuffer)
        [internal->vertexBuffer release];
    internal->vertexBuffer = newBuf;
    internal->perFrameVertexCapacity = newPerFrame;
}

static void _ensure_vertex_buffer_capacity(EseMetalRenderer *internal, size_t needed) {
    if (!internal)
        return;

    if (internal->vertexBuffer && internal->vertexBufferCapacity >= needed) {
        return;
    }

    // grow capacity: choose max(needed, old * 2, 1MB)
    size_t newCap = internal->vertexBufferCapacity ? internal->vertexBufferCapacity * 2 : (1 << 20);
    while (newCap < needed)
        newCap *= 2;
    if (newCap < needed)
        newCap = needed;

    id<MTLBuffer> newBuffer = [internal->device newBufferWithLength:newCap
                                                            options:MTLResourceStorageModeShared];
    if (!newBuffer) {
        // allocation failed, leave existing buffer intact
        return;
    }

    // release old buffer
    if (internal->vertexBuffer) {
        [internal->vertexBuffer release];
    }

    internal->vertexBuffer = newBuffer;
    internal->vertexBufferCapacity = newCap;
}

EseRenderer *renderer_create(bool hiDPI) {
    log_debug("METAL_RENDERER", "Initializing Metal Renderer...");

    // Allocate main renderer
    EseRenderer *renderer = memory_manager.malloc(sizeof(EseRenderer), MMTAG_RENDERER);
    memset(renderer, 0, sizeof(EseRenderer));

    // Allocate internal Metal renderer
    EseMetalRenderer *internal = memory_manager.malloc(sizeof(EseMetalRenderer), MMTAG_RENDERER);
    memset(internal, 0, sizeof(EseMetalRenderer));
    renderer->internal = (void *)internal;

    renderer->hiDPI = hiDPI;

    // Create Metal device
    internal->device = MTLCreateSystemDefaultDevice();
    if (!internal->device) {
        memory_manager.free(internal);
        memory_manager.free(renderer);
        log_error("RENDERER", "Metal is not supported on this device.");
        return NULL;
    }

    // Create MTKView with no size yet
    internal->view = [[MTKView alloc] initWithFrame:CGRectZero device:internal->device];
    internal->view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    internal->view.clearColor = MTLClearColorMake(0, 0, 0, 1);
    internal->view.framebufferOnly = YES;

    internal->viewDelegate = [[RendererViewDelegate alloc] initWithRenderer:renderer];
    internal->view.delegate = internal->viewDelegate;

    // HiDPI scaling will be applied automatically when the view is in a window
    if (!renderer->hiDPI) {
        // Force 1:1 pixel mapping (low DPI mode)
        internal->view.layer.contentsScale = 1.0;

        // Optional: set drawableSize to match point size
        CGSize pointSize = internal->view.bounds.size;
        internal->view.drawableSize = pointSize;
    } else {
        // Let macOS use the display's native scale (Retina)
        CGFloat scale = [NSScreen mainScreen].backingScaleFactor;
        internal->view.layer.contentsScale = scale;

        // Optional: set drawableSize to match scaled size
        CGSize pointSize = internal->view.bounds.size;
        internal->view.drawableSize = CGSizeMake(pointSize.width * scale, pointSize.height * scale);
    }

    // Store view in internal struct
    internal->view = internal->view;
    internal->colorPixelFormat = internal->view.colorPixelFormat;
    internal->clearColor = internal->view.clearColor;

    // Create command queue
    internal->commandQueue = [internal->device newCommandQueue];

    // Create texture loader
    internal->textureLoader = [[MTKTextureLoader alloc] initWithDevice:internal->device];

    // Init renderer state
    internal->render_listBuffer = nil;
    internal->vertexBuffer = nil;
    internal->inflightCount = 3;
    internal->inflightIndex = 0;
    internal->perFrameVertexCapacity = 0;
    internal->frameVertexCursor = 0;

    internal->uboBuffer = nil;
    internal->vertexBufferCapacity = 0;
    renderer->textures = hashmap_create((EseHashMapFreeFn)_free_hash_item);
    renderer->shaders = grouped_hashmap_create((EseGroupedHashMapFreeFn)_free_hash_item);
    renderer->shadersSources = grouped_hashmap_create((EseGroupedHashMapFreeFn)_free_hash_item);
    renderer->render_list = NULL;
    renderer->view_w = 0; // Unknown until added to window
    renderer->view_h = 0;

    // allocate small UBO buffer to reuse
    internal->uboBuffer = [internal->device newBufferWithLength:sizeof(UniformBufferObject)
                                                        options:MTLResourceStorageModeShared];

    // Create initial vertex ring buffer with default per-frame size.
    _ensure_vertex_ring_buffer(internal, 1 << 20);

    // Compile default shader
    _renderer_shader_compile_source(renderer, "default",
                                    [NSString stringWithUTF8String:DEFAULT_SHADER]);

    // Create default pipeline
    renderer_create_pipeline_state(renderer, "default:vertexShader", "default:fragmentShader");

    return renderer;
}

void renderer_destroy(EseRenderer *renderer) {
    log_assert("METAL_RENDERER", renderer, "renderer_destroy called with NULL renderer");

    hashmap_destroy(renderer->textures);
    grouped_hashmap_destroy(renderer->shaders);
    grouped_hashmap_destroy(renderer->shadersSources);

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;
    [internal->commandQueue release];
    [internal->pipelineState release];
    [internal->textureLoader release];
    [internal->render_listBuffer release];

    // release reusable buffers
    if (internal->vertexBuffer)
        [internal->vertexBuffer release];
    if (internal->uboBuffer)
        [internal->uboBuffer release];

    memory_manager.free(internal);
    memory_manager.free(renderer);
}

bool renderer_shader_compile(EseRenderer *renderer, const char *library_name,
                             const char *filename) {
    log_assert("METAL_RENDERER", renderer, "renderer_shader_compile called with NULL renderer");
    log_assert("METAL_RENDERER", library_name,
               "renderer_shader_compile called with NULL library_name");
    log_assert("METAL_RENDERER", filename, "renderer_shader_compile called with NULL filename");

    char *path = filesystem_get_resource(filename);

    NSError *fileError = nil;
    NSString *sourceString = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path]
                                                       encoding:NSUTF8StringEncoding
                                                          error:&fileError];

    // Free the path immediately after use.
    memory_manager.free(path);

    if (sourceString == nil) {
        log_debug("RENDERER", "Failed to load shader file: %s", filename);
        return false;
    }

    bool status = _renderer_shader_compile_source(renderer, library_name, sourceString);
    return status;
}

bool renderer_create_pipeline_state(EseRenderer *renderer, const char *vertexFunc,
                                    const char *fragmentFunc) {
    log_assert("METAL_RENDERER", renderer,
               "renderer_create_pipeline_state called with NULL renderer");
    log_assert("METAL_RENDERER", vertexFunc,
               "renderer_create_pipeline_state called with NULL vertexFunc");
    log_assert("METAL_RENDERER", fragmentFunc,
               "renderer_create_pipeline_state called with NULL fragmentFunc");

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;

    char vLib[64];
    char vFunc[64];
    char fLib[64];
    char fFunc[64];
    ese_helper_split(vertexFunc, vLib, sizeof(vLib), vFunc, sizeof(vFunc));
    ese_helper_split(fragmentFunc, fLib, sizeof(fLib), fFunc, sizeof(fFunc));

    // Parse vertex function string
    if (vLib[0] == '\0') {
        return false;
    }

    if (fLib[0] == '\0') {
        return false;
    }

    // Lookup vertex library
    id<MTLLibrary> vertexLibrary = grouped_hashmap_get(renderer->shaders, vLib, vFunc);
    if (!vertexLibrary) {
        NSLog(@"Vertex library not found: %s", vLib);
        return false;
    }

    // Lookup fragment library
    id<MTLLibrary> fragmentLibrary = grouped_hashmap_get(renderer->shaders, fLib, fFunc);
    if (!fragmentLibrary) {
        NSLog(@"Fragment library not found: %s", fLib);
        return false;
    }

    // Get vertex and fragment functions
    id<MTLFunction> vertex =
        [vertexLibrary newFunctionWithName:[NSString stringWithUTF8String:vFunc]];
    id<MTLFunction> fragment =
        [fragmentLibrary newFunctionWithName:[NSString stringWithUTF8String:fFunc]];
    if (!vertex || !fragment) {
        NSLog(@"Failed to get vertex or fragment function: %s, %s", vertexFunc, fragmentFunc);
        return false;
    }

    // New: Create and configure the MTLVertexDescriptor
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];

    // Position attribute (aPos) at attribute index 0
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;

    // Texture coordinate attribute (aTexCoord) at attribute index 1
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = sizeof(float) * 3; // Offset after the float3 position
    vertexDescriptor.attributes[1].bufferIndex = 0;

    // Layout for buffer 0
    vertexDescriptor.layouts[0].stride =
        sizeof(float) * 5; // Size of a single vertex: 3 floats (pos) + 2 floats (tex coords)
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stepRate = 1;

    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vertex;
    desc.fragmentFunction = fragment;
    desc.vertexDescriptor = vertexDescriptor; // New: Assign the vertex descriptor
    desc.colorAttachments[0].pixelFormat = internal->colorPixelFormat;
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    NSError *error = nil;
    id<MTLRenderPipelineState> pipelineState =
        [internal->device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipelineState) {
        NSLog(@"Failed to create pipeline state: %@", error);
        return false;
    }

    internal->pipelineState = pipelineState;
    return true;
}

void renderer_draw(EseRenderer *renderer) {
    log_assert("METAL_RENDERER", renderer, "renderer_draw called with NULL renderer");

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;
    if (!internal) {
        return;
    }

    // update public size
    renderer->view_w = (int)internal->view.drawableSize.width;
    renderer->view_h = (int)internal->view.drawableSize.height;

    id<MTLCommandBuffer> commandBuffer = [internal->commandQueue commandBuffer];
    MTLRenderPassDescriptor *descriptor = internal->view.currentRenderPassDescriptor;
    if (!descriptor)
        return;

    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:descriptor];
    if (!encoder) {
        [commandBuffer commit];
        return;
    }

    if (internal->pipelineState) {
        [encoder setRenderPipelineState:internal->pipelineState];
    } else {
        [encoder endEncoding];
        [commandBuffer commit];
        return;
    }

    // reset per-frame cursor
    internal->frameVertexCursor = 0;

    // ensure sensible inflight count
    NSUInteger inflight = internal->inflightCount ? internal->inflightCount : 3;
    size_t perFrameCap = internal->perFrameVertexCapacity;
    size_t frameBase = (internal->inflightIndex % inflight) * perFrameCap;

    if (renderer->render_list && render_list_get_batch_count(renderer->render_list) > 0) {
        size_t numBatches = render_list_get_batch_count(renderer->render_list);

        for (size_t i = 0; i < numBatches; ++i) {
            const EseRenderBatch *batch = render_list_get_batch(renderer->render_list, i);
            if (!batch)
                continue;
            if (batch->vertex_count == 0)
                continue;

            // Handle scissor test for this batch
            if (batch->scissor_active) {
                // Convert from screen coordinates to Metal viewport coordinates
                // Metal uses top-left origin, same as our screen coordinates
                MTLScissorRect scissorRect = {.x = (NSUInteger)batch->scissor_x,
                                              .y = (NSUInteger)batch->scissor_y,
                                              .width = (NSUInteger)batch->scissor_w,
                                              .height = (NSUInteger)batch->scissor_h};
                [encoder setScissorRect:scissorRect];
            } else {
                // Disable scissor test by setting it to the full viewport
                MTLScissorRect fullRect = {.x = 0,
                                           .y = 0,
                                           .width = (NSUInteger)renderer->view_w,
                                           .height = (NSUInteger)renderer->view_h};
                [encoder setScissorRect:fullRect];
            }

            id<MTLTexture> tex = nil;
            if (batch->type == RL_TEXTURE) {
                tex = hashmap_get(renderer->textures, batch->shared_state.texture_id);
                if (!tex)
                    continue;
            }

            // compute data size depending on batch type
            size_t data_size = 0;
            if (batch->type == RL_TEXTURE) {
                data_size = batch->vertex_count * sizeof(EseVertex);
            } else if (batch->type == RL_COLOR) {
                data_size = batch->vertex_count * 5 * sizeof(float);
            }
            if (data_size == 0)
                continue;

            // ensure per-frame capacity can hold this batch
            if (!internal->vertexBuffer || perFrameCap < data_size) {
                _ensure_vertex_ring_buffer(internal, data_size);
                perFrameCap = internal->perFrameVertexCapacity;
                frameBase = (internal->inflightIndex % inflight) * perFrameCap;
            }
            if (!internal->vertexBuffer || perFrameCap < data_size) {
                // allocation failed or still too small
                continue;
            }

            // align writes to 256 bytes to be safe for GPU
            size_t align = 256;
            size_t writeOffset = (internal->frameVertexCursor + (align - 1)) & ~(align - 1);
            if (writeOffset + data_size > perFrameCap) {
                // not enough room left in this frame region
                continue;
            }

            // copy vertex data into the ring buffer region for this frame
            void *dest = (char *)[internal->vertexBuffer contents] + frameBase + writeOffset;
            memcpy(dest, batch->vertex_buffer, data_size);

            // bind vertex buffer with global offset into ring buffer
            [encoder setVertexBuffer:internal->vertexBuffer
                              offset:(frameBase + writeOffset)
                             atIndex:0];

            // prepare and set small UBO via setFragmentBytes (avoids MTLBuffer
            // reuse races)
            UniformBufferObject ubo;

            // Just hard code defaults for now
            ubo.tint = (EseVector4){1.0f, 1.0f, 1.0f, 1.0f};
            ubo.opacity = 1.0f;

            if (batch->type == RL_TEXTURE) {
                ubo.useTexture.x = 1;
                ubo.color.x = 0.0f;
                ubo.color.y = 0.0f;
                ubo.color.z = 0.0f;
                ubo.color.w = 0.0f;
            } else if (batch->type == RL_COLOR) {
                ubo.useTexture.x = 0;
                ubo.color.x = (float)batch->shared_state.color.r / 255.0f;
                ubo.color.y = (float)batch->shared_state.color.g / 255.0f;
                ubo.color.z = (float)batch->shared_state.color.b / 255.0f;
                ubo.color.w = (float)batch->shared_state.color.a / 255.0f;
            }

            [encoder setFragmentTexture:(batch->type == RL_TEXTURE ? tex : nil) atIndex:0];
            [encoder setFragmentBytes:&ubo length:sizeof(UniformBufferObject) atIndex:1];

            // draw
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:batch->vertex_count];

            // advance cursor for this frame
            internal->frameVertexCursor = writeOffset + data_size;
        }
    }

    [encoder endEncoding];
    [commandBuffer presentDrawable:internal->view.currentDrawable];
    [commandBuffer commit];

    // advance inflight index to avoid overwriting in-use GPU regions
    internal->inflightIndex =
        (internal->inflightIndex + 1) % (internal->inflightCount ? internal->inflightCount : 3);
}

bool renderer_load_texture(EseRenderer *renderer, const char *texture_id,
                           const unsigned char *rgba_data, int width, int height) {
    log_assert("METAL_RENDERER", renderer, "renderer_load_texture called with NULL renderer");
    log_assert("METAL_RENDERER", texture_id, "renderer_load_texture called with NULL texture_id");
    log_assert("METAL_RENDERER", rgba_data, "renderer_load_texture called with NULL rgba_data");
    log_assert("METAL_RENDERER", width > 0, "renderer_load_texture called with invalid width");
    log_assert("METAL_RENDERER", height > 0, "renderer_load_texture called with invalid height");

    // Check if texture already loaded
    if (hashmap_get(renderer->textures, texture_id)) {
        log_debug("METAL_RENDERER", "Texture already loaded (%s)", texture_id);
        return true;
    }

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;

    // Create Metal texture from raw RGBA pixel data
    MTLTextureDescriptor *desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    id<MTLTexture> texture = [internal->device newTextureWithDescriptor:desc];
    if (!texture) {
        NSLog(@"Failed to create Metal texture for %s", texture_id);
        return false;
    }

    // Upload the RGBA data to the texture
    MTLRegion region = {{0, 0, 0}, {width, height, 1}};
    NSUInteger bytesPerRow = width * 4; // 4 bytes per pixel (RGBA)
    [texture replaceRegion:region mipmapLevel:0 withBytes:rgba_data bytesPerRow:bytesPerRow];

    // Store in hashmap (key: texture_id, value: texture)
    hashmap_set(renderer->textures, texture_id, texture);

    return true;
}

bool renderer_set_render_list(EseRenderer *renderer, EseRenderList *render_list) {
    log_assert("METAL_RENDERER", renderer, "renderer_set_render_list called with NULL renderer");
    log_assert("METAL_RENDERER", render_list,
               "renderer_set_render_list called with NULL render_list");

    renderer->render_list = render_list;

    return renderer->render_list != nil;
}

EseRenderList *renderer_get_render_list(EseRenderer *renderer) {
    log_assert("METAL_RENDERER", renderer, "renderer_get_render_list called with NULL renderer");

    return renderer->render_list;
}

bool renderer_clear_render_list(EseRenderer *renderer) {
    log_assert("METAL_RENDERER", renderer, "renderer_clear_render_list called with NULL renderer");

    renderer->render_list = nil;

    return renderer->render_list == nil;
}

bool renderer_get_size(EseRenderer *renderer, int *width, int *height) {
    log_assert("METAL_RENDERER", renderer, "renderer_get_size called with NULL renderer");

    if (width) {
        *width = (int)ceilf((float)renderer->view_w);
    }

    if (height) {
        *height = (int)ceilf((float)renderer->view_h);
    }

    return true;
}
