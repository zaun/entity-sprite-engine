// RendererViewDelegate.m
#import "platform/mac/renderer_delegate.h"
#import "platform/renderer.h"
#import "platform/renderer_private.h"

@implementation RendererViewDelegate

- (instancetype)initWithRenderer:(EseRenderer *)renderer {
  if (self = [super init]) {
    _renderer = renderer;
  }
  return self;
}

// Called whenever drawable size changes (including first layout)
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
  _renderer->view_w = (int)size.width;
  _renderer->view_h = (int)size.height;
}

// Called every frame — you can trigger your render loop here if needed
- (void)drawInMTKView:(MTKView *)view {
  renderer_draw(_renderer);
}

@end
