// RendererViewDelegate.h
#import <MetalKit/MetalKit.h>
#import "platform/mac/renderer_private.h"

@interface RendererViewDelegate : NSObject <MTKViewDelegate>
@property (nonatomic, assign) EseRenderer *renderer;

- (instancetype)initWithRenderer:(EseRenderer *)renderer;

@end
