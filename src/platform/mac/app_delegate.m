#import "app_delegate.h"

@implementation EseAppDelegate

// Called when user hits Cmd+Q or selects Quit from menu
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if (self.eseWindow) {
        self.eseWindow->should_close = true;
        return NSTerminateCancel; // let engine handle shutdown
    }
    return NSTerminateNow;
}

// Called when user clicks the red close button
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    if (self.eseWindow) {
        self.eseWindow->should_close = true;
    }
    return NO;
}

@end
