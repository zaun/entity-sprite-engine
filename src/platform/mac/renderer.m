// Metal Implementation of EseRenderer
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <unistd.h>
#import "platform/renderer.h"
#import "platform/mac/renderer_delegate.h"
#import "platform/mac/renderer_private.h"
#import "platform/renderer_private.h"
#import "core/memory_manager.h"
#import "utility/log.h"
#import "graphics/render_list.h"
#import "graphics/shader.h"
#import "utility/grouped_hashmap.h"
#import "platform/filesystem.h"
#import "vendor/stb/stb_image.h"

void _split_library_func(const char* input, char** group, char** name) {
    // Initialize output pointers to NULL
    *group = NULL;
    *name = NULL;

    // Handle NULL input immediately
    if (input == NULL) {
        return;
    }

    const char* colon = strchr(input, ':');

    if (colon == NULL) {
        // No colon: full string is the name, group is "default"
        *group = memory_manager.strdup("default", MMTAG_RENDERER);
        *name = memory_manager.strdup(input, MMTAG_RENDERER);
    } else {
        // Calculate lengths of potential group and name parts
        size_t groupLength = colon - input;
        size_t nameLength = strlen(colon + 1);

        // Case: "test:" or ":" (group exists, name is empty)
        if (nameLength == 0) {
            return;
        }

        // Case: ":test" (no group, name exists)
        if (groupLength == 0) {
            *group = memory_manager.strdup("default", MMTAG_RENDERER);
            *name = memory_manager.strdup(colon + 1, MMTAG_RENDERER);
        }
        // Case: "group:test" (both group and name exist)
        else {
            *group = (char*)memory_manager.malloc(groupLength + 1, MMTAG_RENDERER);
            if (*group) {
                strncpy(*group, input, groupLength);
                (*group)[groupLength] = '\0';
            }
            *name = memory_manager.strdup(colon + 1, MMTAG_RENDERER);
        }
    }
}

void _free_hash_item(void *value) {
    #if !__has_feature(objc_arc)
        [(id)value release];
    #endif
}

EseRenderer* renderer_create(bool hiDPI) {
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
    internal->textureLoader =
        [[MTKTextureLoader alloc] initWithDevice:internal->device];

    // Init renderer state
    internal->render_listBuffer = nil;
    renderer->textures = hashmap_create();
    renderer->shaders = grouped_hashmap_create((EseGroupedHashMapFreeFn)_free_hash_item);
    renderer->shadersSources = grouped_hashmap_create((EseGroupedHashMapFreeFn)_free_hash_item);
    renderer->render_list = NULL;
    renderer->view_w = 0; // Unknown until added to window
    renderer->view_h = 0;

    // Default shader source
    const char *shaderSource =
    "#version 450\n"
    "\n"
    "#ifdef VERTEX_SHADER\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "\n"
    "layout(location = 0) out vec2 TexCoord;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n"
    "#endif\n"
    "\n"
    "#ifdef FRAGMENT_SHADER\n"
    "precision mediump float;\n"
    "\n"
    "layout(location = 0) in vec2 TexCoord;\n"
    "layout(location = 0) out vec4 FragColor;\n"
    "\n"
    "layout(binding = 0) uniform sampler2D ourTexture;\n"
    "\n"
    "layout(binding = 1) uniform UniformBufferObject {\n"
    "    bool useTexture;\n"
    "    vec4 rectColor;\n"
    "} ubo;\n"
    "\n"
    "void main() {\n"
    "    if (ubo.useTexture) {\n"
    "        FragColor = texture(ourTexture, TexCoord);\n"
    "    } else {\n"
    "        FragColor = ubo.rectColor;\n"
    "    }\n"
    "}\n"
    "#endif\n"
    "\n"
    "#ifdef COMPUTE_SHADER\n"
    "#endif\n"
    "\n";

    // Compile default shader
    _renderer_shader_compile_source(
        renderer, "default", [NSString stringWithUTF8String:shaderSource]
    );

    // Create default pipeline
    renderer_create_pipeline_state(
        renderer, "default:vertexShader", "default:fragmentShader"
    );

    return renderer;
}

void renderer_destroy(EseRenderer* renderer) {
    log_assert("METAL_RENDERER", renderer, "renderer_destroy called with NULL renderer");

#if !__has_feature(objc_arc)
    EseHashMapIter* iter = hashmap_iter_create(renderer->textures);
    void* value = NULL;
    while (hashmap_iter_next(iter, NULL, &value)) {
        [(id)value release];
    }
    hashmap_iter_free(iter);
#endif

    hashmap_free(renderer->textures);
    grouped_hashmap_free(renderer->shaders);
    grouped_hashmap_free(renderer->shadersSources);

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;
    [internal->commandQueue release];
    [internal->pipelineState release];
    [internal->textureLoader release];
    [internal->render_listBuffer release];

     memory_manager.free(internal);
     memory_manager.free(renderer);
}

bool _renderer_shader_compile_source(EseRenderer* renderer, const char *library_name, NSString *sourceString) {
    log_assert("METAL_RENDERER", renderer, "_renderer_shader_compile_source called with NULL renderer");
    log_assert("METAL_RENDERER", library_name, "_renderer_shader_compile_source called with NULL library_name");
    log_assert("METAL_RENDERER", sourceString, "_renderer_shader_compile_source called with NULL sourceString");

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
    id<MTLLibrary> vertexLibrary = [internal->device newLibraryWithSource:shaderSourceString options:nil error:&error];
    if (!vertexLibrary) {
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name, error.localizedDescription.UTF8String);
        return false;
    }
    grouped_hashmap_set(renderer->shaders, library_name, "vertexShader", [vertexLibrary retain]);
    grouped_hashmap_set(renderer->shadersSources, library_name, "vertexShader", [shaderSourceString retain]);

    shaderSourceString = [NSString stringWithUTF8String:fs.data];
    id<MTLLibrary> fragmentLibrary = [internal->device newLibraryWithSource:shaderSourceString options:nil error:&error];
    if (!fragmentLibrary) {
        free_shader_blob(vs);
        free_shader_blob(fs);
        free_shader_blob(cs);
        log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name, error.localizedDescription.UTF8String);
        return false;
    }
    grouped_hashmap_set(renderer->shaders, library_name, "fragmentShader", [fragmentLibrary retain]);
    grouped_hashmap_set(renderer->shadersSources, library_name, "fragmentShader", [shaderSourceString retain]);

    if (cs.data) {
        shaderSourceString = [NSString stringWithUTF8String:fs.data];
        id<MTLLibrary> computeLibrary = [internal->device newLibraryWithSource:shaderSourceString options:nil error:&error];
        if (!computeLibrary) {
            free_shader_blob(vs);
            free_shader_blob(fs);
            free_shader_blob(cs);
            log_debug("RENDERER", "Failed to compile shader library %s:\n%s", library_name, error.localizedDescription.UTF8String);
            return false;
        }
        grouped_hashmap_set(renderer->shaders, library_name, "computeShader", [computeLibrary retain]);
        grouped_hashmap_set(renderer->shadersSources, library_name, "computeShader", [shaderSourceString retain]);
    }

    log_debug("RENDERER", "Compiled library %s.", library_name);
    
    free_shader_blob(vs);
    free_shader_blob(fs);
    free_shader_blob(cs);

    return true;
}

bool renderer_shader_compile(EseRenderer* renderer, const char *library_name, const char *filename) {
    log_assert("METAL_RENDERER", renderer, "renderer_shader_compile called with NULL renderer");
    log_assert("METAL_RENDERER", library_name, "renderer_shader_compile called with NULL library_name");
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

bool renderer_create_pipeline_state(EseRenderer* renderer, const char *vertexFunc, const char *fragmentFunc) {
    log_assert("METAL_RENDERER", renderer, "renderer_create_pipeline_state called with NULL renderer");
    log_assert("METAL_RENDERER", vertexFunc, "renderer_create_pipeline_state called with NULL vertexFunc");
    log_assert("METAL_RENDERER", fragmentFunc, "renderer_create_pipeline_state called with NULL fragmentFunc");

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;

    char *vLib = NULL;
    char *vFunc = NULL;
    char *fLib = NULL;
    char *fFunc = NULL;

    // Parse vertex function string
    _split_library_func(vertexFunc, &vLib, &vFunc);
    if (!vLib || !vFunc) {
        if (vLib) memory_manager.free(vLib);
        if (vFunc) memory_manager.free(vFunc);
        return false;
    }

    _split_library_func(fragmentFunc, &fLib, &fFunc);
    if (!fLib || !fFunc) {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        if (fLib) memory_manager.free(fLib);
        if (fFunc) memory_manager.free(fFunc);
        return false;
    }


    // Lookup vertex library
    id<MTLLibrary> vertexLibrary = grouped_hashmap_get(renderer->shaders, vLib,  vFunc);
    if (!vertexLibrary) {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        memory_manager.free(fLib);
        memory_manager.free(fFunc);
        NSLog(@"Vertex library not found: %s", vLib);
        return false;
    }

    // Lookup fragment library
    id<MTLLibrary> fragmentLibrary = grouped_hashmap_get(renderer->shaders, fLib, fFunc);
    if (!fragmentLibrary) {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        memory_manager.free(fLib);
        memory_manager.free(fFunc);
        NSLog(@"Fragment library not found: %s", fLib);
        return false;
    }

    // Get vertex and fragment functions
    id<MTLFunction> vertex = [vertexLibrary newFunctionWithName:[NSString stringWithUTF8String:vFunc]];
    id<MTLFunction> fragment = [fragmentLibrary newFunctionWithName:[NSString stringWithUTF8String:fFunc]];
    if (!vertex || !fragment) {
        memory_manager.free(vLib);
        memory_manager.free(vFunc);
        memory_manager.free(fLib);
        memory_manager.free(fFunc);
        NSLog(@"Failed to get vertex or fragment function: %s, %s", vertexFunc, fragmentFunc);
        return false;
    }

    memory_manager.free(vLib);
    memory_manager.free(vFunc);
    memory_manager.free(fLib);
    memory_manager.free(fFunc);

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
    vertexDescriptor.layouts[0].stride = sizeof(float) * 5; // Size of a single vertex: 3 floats (pos) + 2 floats (tex coords)
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
    id<MTLRenderPipelineState> pipelineState = [internal->device newRenderPipelineStateWithDescriptor:desc error:&error];
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
    if (!descriptor) return;

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

    if (internal->pipelineState) {
        [encoder setRenderPipelineState:internal->pipelineState];
    } else {
        [encoder endEncoding];
        [commandBuffer commit];
        return;
    }

    if (renderer->render_list && render_list_get_batch_count(renderer->render_list) > 0) {
        size_t numBatches = render_list_get_batch_count(renderer->render_list);

        // --- Batch Drawing Loop ---
        for (size_t i = 0; i < numBatches; ++i) {
            const EseRenderBatch *batch = render_list_get_batch(renderer->render_list, i);
            
            if (batch->vertex_count == 0) continue;

            if (batch->type == RL_TEXTURE) {
                // Handle texture batch
                id<MTLTexture> tex = hashmap_get(renderer->textures, batch->shared_state.texture_id);
                if (!tex) continue;

                // Create vertex buffer from batch data
                size_t data_size = batch->vertex_count * sizeof(EseVertex);
                id<MTLBuffer> vertexBuffer = [internal->device newBufferWithBytes:batch->vertex_buffer 
                                                                           length:data_size 
                                                                          options:MTLResourceStorageModeShared];

                // Create UBO for texture rendering
                UniformBufferObject ubo;
                ubo.useTexture = 1;
                ubo.rectColor = simd_make_float4(0.0f, 0.0f, 0.0f, 0.0f);
                
                id<MTLBuffer> uboBuffer = [internal->device newBufferWithLength:sizeof(UniformBufferObject) 
                                                                        options:MTLResourceStorageModeShared];
                memcpy([uboBuffer contents], &ubo, sizeof(UniformBufferObject));

                // Set render state
                [encoder setFragmentTexture:tex atIndex:0];
                [encoder setFragmentBuffer:uboBuffer offset:0 atIndex:1];
                [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];

                // Draw the batch
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:batch->vertex_count];

                [vertexBuffer release];
                [uboBuffer release];

            } else if (batch->type == RL_RECT) {
                // Handle rectangle batch
                // Create vertex buffer from batch data
                size_t data_size = batch->vertex_count * 5 * sizeof(float);
                id<MTLBuffer> vertexBuffer = [internal->device newBufferWithBytes:batch->vertex_buffer 
                                                                           length:data_size 
                                                                          options:MTLResourceStorageModeShared];

                // Create UBO for solid color rendering
                UniformBufferObject ubo;
                ubo.useTexture = 0;
                ubo.rectColor = simd_make_float4(
                    (float)batch->shared_state.rect_color.r / 255.0f,
                    (float)batch->shared_state.rect_color.g / 255.0f,
                    (float)batch->shared_state.rect_color.b / 255.0f,
                    (float)batch->shared_state.rect_color.a / 255.0f
                );
                
                id<MTLBuffer> uboBuffer = [internal->device newBufferWithLength:sizeof(UniformBufferObject) 
                                                                        options:MTLResourceStorageModeShared];
                memcpy([uboBuffer contents], &ubo, sizeof(UniformBufferObject));

                // Set render state
                [encoder setFragmentTexture:nil atIndex:0];
                [encoder setFragmentBuffer:uboBuffer offset:0 atIndex:1];
                [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];

                // Draw the batch
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:batch->vertex_count];

                [vertexBuffer release];
                [uboBuffer release];
            }
        }
    }

    [encoder endEncoding];
    [commandBuffer presentDrawable:internal->view.currentDrawable];
    [commandBuffer commit];
}

bool renderer_load_texture(EseRenderer* renderer, const char* texture_id, const char* filename, int *out_width, int *out_height) {
    log_assert("METAL_RENDERER", renderer, "renderer_load_texture called with NULL renderer");
    log_assert("METAL_RENDERER", texture_id, "renderer_load_texture called with NULL texture_id");
    log_assert("METAL_RENDERER", filename, "renderer_load_texture called with NULL filename");

    // The hashmap now stores a pointer to a GLTexture struct
    if (hashmap_get(renderer->textures, texture_id)) {
        log_debug("METAL_RENDERER", "Texture already loaded (%s) %s", texture_id, filename);
        return true;
    }

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;

    // Check cache first
    if (hashmap_get(renderer->textures, filename)) return true;

    // Create NSString from filename (assume no extension)
    NSString *name = [NSString stringWithUTF8String:filename];

    // List of supported extensions
    NSArray *extensions = @[ @"png", @"jpg", @"jpeg", @"bmp" ];
    NSString *path = nil;

    // Try each extension
    for (NSString *ext in extensions) {
        path = [[NSBundle mainBundle] pathForResource:name ofType:ext];
        if (path) break;
    }

    if (!path) {
        NSLog(@"Texture file not found: %s (tried png, jpg, jpeg, bmp)", filename);
        return false;
    }

    NSURL *url = [NSURL fileURLWithPath:path];
    NSError *error = nil;
    id<MTLTexture> texture = [internal->textureLoader newTextureWithContentsOfURL:url options:nil error:&error];
    if (!texture) {
        NSLog(@"Failed to load texture %s: %@", filename, error.localizedDescription);
        return false;
    }

    // Store in hashmap (key: filename, value: texture)
    hashmap_set(renderer->textures, texture_id, texture);

    // Get the dimensions from the texture object
    NSUInteger width = [texture width];
    NSUInteger height = [texture height];

    if (out_width) {
        *out_width = (int)width;
    }
    if (out_height) {
        *out_height = (int)height;
    }

    return true;
}

bool renderer_load_texture_indexed(EseRenderer* renderer, const char* texture_id, const char* filename, int *out_width, int *out_height) {
    log_assert("METAL_RENDERER", renderer, "renderer_load_texture_indexed called with NULL renderer");
    log_assert("METAL_RENDERER", texture_id, "renderer_load_texture_indexed called with NULL texture_id");
    log_assert("METAL_RENDERER", filename, "renderer_load_texture_indexed called with NULL filename");

    // The hashmap now stores a pointer to a GLTexture struct
    if (hashmap_get(renderer->textures, texture_id)) {
        log_debug("METAL_RENDERER", "Texture already loaded (%s) %s", texture_id, filename);
        return true;
    }

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;

    NSString *name = [NSString stringWithUTF8String:filename];
    NSArray *extensions = @[ @"png", @"jpg", @"jpeg", @"bmp" ];
    NSString *path = nil;

    for (NSString *ext in extensions) {
        path = [[NSBundle mainBundle] pathForResource:name ofType:ext];
        if (path) break;
    }

    if (!path) {
        NSLog(@"Texture file not found: %s (tried png, jpg, jpeg, bmp)", filename);
        return false;
    }

    NSImage *image = [[NSImage alloc] initWithContentsOfFile:path];
    if (!image) {
        NSLog(@"Failed to load image: %s", filename);
        return false;
    }

    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
    if (!bitmap) {
        NSLog(@"Failed to create bitmap for: %s", filename);
        return false;
    }

    NSInteger width = bitmap.pixelsWide;
    NSInteger height = bitmap.pixelsHigh;
    NSInteger bytesPerPixel = 4;
    NSInteger srcRowBytes = [bitmap bytesPerRow];

    // Prepare output buffer (RGBA)
    NSUInteger dstRowBytes = width * bytesPerPixel;
    NSUInteger bufferSize = dstRowBytes * height;
    unsigned char *dst = memory_manager.malloc(bufferSize, MMTAG_RENDERER);
    unsigned char *src = [bitmap bitmapData];

    // Get transparent color from top-left pixel (y = height-1, x = 0)
    unsigned char *topLeft = src + (height - 1) * srcRowBytes;
    unsigned char tr = topLeft[0];
    unsigned char tg = topLeft[1];
    unsigned char tb = topLeft[2];

    // Fast C loop for transparency
    for (NSInteger y = 0; y < height; y++) {
        unsigned char *srcRow = src + y * srcRowBytes;
        unsigned char *dstRow = dst + y * dstRowBytes;
        for (NSInteger x = 0; x < width; x++) {
            unsigned char *sp = srcRow + x * bytesPerPixel;
            unsigned char *dp = dstRow + x * bytesPerPixel;
            dp[0] = sp[0]; // R
            dp[1] = sp[1]; // G
            dp[2] = sp[2]; // B
            dp[3] = (sp[0] == tr && sp[1] == tg && sp[2] == tb) ? 0 : 255; // A
        }
    }

    // Create Metal texture from raw pixel data
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                   width:width
                                                                                  height:height
                                                                               mipmapped:NO];
    id<MTLTexture> texture = [internal->device newTextureWithDescriptor:desc];

    MTLRegion region = { {0, 0, 0}, {width, height, 1} };
    [texture replaceRegion:region mipmapLevel:0 withBytes:dst bytesPerRow:dstRowBytes];

    hashmap_set(renderer->textures, texture_id, texture);

    memory_manager.free(dst);

    if (out_width) {
        *out_width = width;
    }
    if (out_height) {
        *out_height = height;
    }

    return true;
}

bool renderer_set_render_list(EseRenderer *renderer, EseRenderList *render_list) {
    log_assert("METAL_RENDERER", renderer, "renderer_set_render_list called with NULL renderer");
    log_assert("METAL_RENDERER", render_list, "renderer_set_render_list called with NULL render_list");

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
