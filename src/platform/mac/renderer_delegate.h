// RendererViewDelegate.h
#import "platform/mac/renderer_private.h"
#import <MetalKit/MetalKit.h>

@interface RendererViewDelegate : NSObject <MTKViewDelegate>
@property(nonatomic, assign) EseRenderer *renderer;

- (instancetype)initWithRenderer:(EseRenderer *)renderer;

@end
