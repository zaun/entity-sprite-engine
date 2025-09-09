#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "platform/window.h"
#import "platform/renderer.h"
#import "platform/mac/app_delegate.h"
#import "platform/mac/renderer_private.h"
#import "platform/renderer_private.h"
#import "core/memory_manager.h"
#import "types/input_state.h"
#import "types/input_state_private.h"

/**
 * @brief Platform-specific window structure for macOS Metal implementation.
 * 
 * @details This structure wraps the macOS NSWindow and NSView handles along
 *          with the engine's input state for the Metal platform. It provides
 *          the bridge between macOS window events and the engine's input system.
 */
typedef struct EseMetalWindow {
    NSWindow *window;              /**< macOS NSWindow handle */
    NSView *view;                  /**< macOS NSView handle for the window */
    EseInputState *inputState;     /**< Reference to the engine's input state */
} EseMetalWindow;

// Map macOS keycodes to your engine's keys
static EseInputKey _mapMacOSKeycodeToInputKey(unsigned short keyCode) {
    switch (keyCode) {
        case 0x00: return InputKey_A;
        case 0x0B: return InputKey_B;
        case 0x08: return InputKey_C;
        case 0x02: return InputKey_D;
        case 0x0E: return InputKey_E;
        case 0x03: return InputKey_F;
        case 0x05: return InputKey_G;
        case 0x04: return InputKey_H;
        case 0x22: return InputKey_I;
        case 0x26: return InputKey_J;
        case 0x28: return InputKey_K;
        case 0x25: return InputKey_L;
        case 0x2E: return InputKey_M;
        case 0x2D: return InputKey_N;
        case 0x1F: return InputKey_O;
        case 0x23: return InputKey_P;
        case 0x0C: return InputKey_Q;
        case 0x0F: return InputKey_R;
        case 0x01: return InputKey_S;
        case 0x11: return InputKey_T;
        case 0x20: return InputKey_U;
        case 0x09: return InputKey_V;
        case 0x0D: return InputKey_W;
        case 0x07: return InputKey_X;
        case 0x10: return InputKey_Y;
        case 0x06: return InputKey_Z;
        
        // Numbers
        case 0x1D: return InputKey_0;
        case 0x12: return InputKey_1;
        case 0x13: return InputKey_2;
        case 0x14: return InputKey_3;
        case 0x15: return InputKey_4;
        case 0x17: return InputKey_5;
        case 0x16: return InputKey_6;
        case 0x1A: return InputKey_7;
        case 0x1C: return InputKey_8;
        case 0x19: return InputKey_9;
        
        // Function keys
        case 0x7A: return InputKey_F1;
        case 0x78: return InputKey_F2;
        case 0x63: return InputKey_F3;
        case 0x76: return InputKey_F4;
        case 0x60: return InputKey_F5;
        case 0x61: return InputKey_F6;
        case 0x62: return InputKey_F7;
        case 0x64: return InputKey_F8;
        case 0x65: return InputKey_F9;
        case 0x6D: return InputKey_F10;
        case 0x67: return InputKey_F11;
        case 0x6F: return InputKey_F12;
        
        // Arrow keys
        case 0x7E: return InputKey_UP;
        case 0x7D: return InputKey_DOWN;
        case 0x7B: return InputKey_LEFT;
        case 0x7C: return InputKey_RIGHT;
        
        // Special keys
        case 0x31: return InputKey_SPACE;
        case 0x24: return InputKey_ENTER;
        case 0x35: return InputKey_ESCAPE;
        case 0x30: return InputKey_TAB;
        case 0x33: return InputKey_BACKSPACE;
        case 0x39: return InputKey_CAPSLOCK;
        
        default: return InputKey_UNKNOWN;

    }
}

EseWindow* window_create(int width, int height, const char* title) {
    if (NSApp == nil) {
        [NSApplication sharedApplication];
    }

    // Setup menu bar with Quit
    NSMenu *menubar = [[NSMenu alloc] init];
    [NSApp setMainMenu:menubar];

    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];

    NSMenu *appMenu = [[NSMenu alloc] init];
    [appMenuItem setSubmenu:appMenu];

    NSString *appName = [[NSProcessInfo processInfo] processName];
    NSString *quitTitle = [NSString stringWithFormat:@"Quit %@", appName];
    NSMenuItem *quitItem = [[NSMenuItem alloc]
        initWithTitle:quitTitle
               action:@selector(terminate:)
        keyEquivalent:@"q"];
    [appMenu addItem:quitItem];

    // Create window + engine structs
    EseWindow* window = (EseWindow*)memory_manager.malloc(sizeof(EseWindow), MMTAG_WINDOW);
    EseMetalWindow* metalWindow = (EseMetalWindow*)memory_manager.malloc(sizeof(EseMetalWindow), MMTAG_WINDOW);
    metalWindow->inputState = ese_input_state_create(nil);

    // Create window
    NSRect frame = NSMakeRect(100, 100, width, height);
    metalWindow->window = [[NSWindow alloc] initWithContentRect:frame
                                                      styleMask:(NSWindowStyleMaskTitled |
                                                                 NSWindowStyleMaskClosable |
                                                                 NSWindowStyleMaskResizable)
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
    metalWindow->window.title = [NSString stringWithUTF8String:title];

    // Activate app and show window
    [NSApp activateIgnoringOtherApps:YES];
    [metalWindow->window makeKeyAndOrderFront:nil];
    [metalWindow->window setAcceptsMouseMovedEvents:YES];

    // Hook delegate
    EseAppDelegate *delegate = [[EseAppDelegate alloc] init];
    delegate.eseWindow = window;
    [NSApp setDelegate:delegate];

    window->platform_window = metalWindow;
    window->width = width;
    window->height = height;
    window->should_close = false;

    return window;
}

void window_destroy(EseWindow* window) {
    if (!window) return;
    if (window->platform_window) {
        EseMetalWindow* metalWindow = (EseMetalWindow*)window->platform_window;
        [metalWindow->window close];

        ese_input_state_destroy(metalWindow->inputState);
        memory_manager.free(metalWindow);
    }
    memory_manager.free(window);
}

void window_set_renderer(EseWindow* window, EseRenderer* renderer) {
    if (!window || !renderer) return;
    EseMetalWindow* metalWindow = (EseMetalWindow*)window->platform_window;

    EseMetalRenderer *internal = (EseMetalRenderer *)renderer->internal;
    window->renderer = renderer;

    // Set the renderer view
    metalWindow->view = internal->view;
    [metalWindow->window.contentView addSubview:metalWindow->view];
    metalWindow->view.frame = metalWindow->window.contentView.bounds;
    metalWindow->view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    // Enforce hiDPI or lowDPI after attaching to window
    if (!renderer->hiDPI) {
        // Force 1:1 pixel mapping
        internal->view.layer.contentsScale = 1.0;
        internal->view.drawableSize = internal->view.bounds.size;
    } else {
        // Match screen scale (Retina)
        CGFloat scale = metalWindow->window.screen.backingScaleFactor;
        internal->view.layer.contentsScale = scale;
        CGSize pointSize = internal->view.bounds.size;
        internal->view.drawableSize = CGSizeMake(pointSize.width * scale,
                                                           pointSize.height * scale);
    }
}

void window_process(EseWindow* window, EseInputState* out_input_state) {
    if (!window) return;

    EseMetalWindow* metalWindow = (EseMetalWindow*)window->platform_window;
    if (!metalWindow) return;

    if (window->should_close) {
        [metalWindow->window close];
    }

    NSEvent *event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {

        // Menu handled it; skip engine handling and don't cause beep.
        if ([[NSApp mainMenu] performKeyEquivalent:event]) {
            continue;
        }

        // --- Update modifier keys for every event ---
        NSEventModifierFlags mods = [event modifierFlags];
        metalWindow->inputState->keys_down[InputKey_LSHIFT]  = (mods & NSEventModifierFlagShift) != 0;
        metalWindow->inputState->keys_down[InputKey_RSHIFT]  = (mods & NSEventModifierFlagShift) != 0;

        metalWindow->inputState->keys_down[InputKey_LCTRL]   = (mods & NSEventModifierFlagControl) != 0;
        metalWindow->inputState->keys_down[InputKey_RCTRL]   = (mods & NSEventModifierFlagControl) != 0;

        metalWindow->inputState->keys_down[InputKey_LALT]    = (mods & NSEventModifierFlagOption) != 0;
        metalWindow->inputState->keys_down[InputKey_RALT]    = (mods & NSEventModifierFlagOption) != 0;

        metalWindow->inputState->keys_down[InputKey_LCMD]    = (mods & NSEventModifierFlagCommand) != 0;
        metalWindow->inputState->keys_down[InputKey_RCMD]    = (mods & NSEventModifierFlagCommand) != 0;

        metalWindow->inputState->keys_down[InputKey_CAPSLOCK] = (mods & NSEventModifierFlagCapsLock) != 0;

        switch ([event type]) {
            case NSEventTypeKeyDown: {
                EseInputKey key = _mapMacOSKeycodeToInputKey([event keyCode]);
                if (key != InputKey_UNKNOWN && key < InputKey_MAX) {
                    metalWindow->inputState->keys_down[key] = true;
                    metalWindow->inputState->keys_pressed[key] = true;
                }
                // Dont forward the event to prevent beeps
                continue;
            } break;
            case NSEventTypeKeyUp: {
                EseInputKey key = _mapMacOSKeycodeToInputKey([event keyCode]);
                if (key != InputKey_UNKNOWN && key < InputKey_MAX) {
                    metalWindow->inputState->keys_down[key] = false;
                    metalWindow->inputState->keys_released[key] = true;
                }
            } break;
            case NSEventTypeLeftMouseDown:
                metalWindow->inputState->mouse_buttons[0] = true;
                break;
            case NSEventTypeLeftMouseUp:
                metalWindow->inputState->mouse_buttons[0] = false;
                break;
            case NSEventTypeRightMouseDown:
                metalWindow->inputState->mouse_buttons[1] = true;
                break;
            case NSEventTypeRightMouseUp:
                metalWindow->inputState->mouse_buttons[1] = false;
                break;
            case NSEventTypeMouseMoved:
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged: {
                NSPoint loc = [metalWindow->view convertPoint:[event locationInWindow] fromView:nil];
                metalWindow->inputState->mouse_x = loc.x;
                metalWindow->inputState->mouse_y = loc.y;
            } break;
            case NSEventTypeScrollWheel:
                metalWindow->inputState->mouse_scroll_dx += [event scrollingDeltaX];
                metalWindow->inputState->mouse_scroll_dy += [event scrollingDeltaY];
                break;
            default:
                break;
        }
        [NSApp sendEvent:event];
    }

    // Copy input state to output
    size_t prefix = offsetof(EseInputState, state);
    memmove(out_input_state, metalWindow->inputState, prefix);

    // Reset per-frame input
    memset(metalWindow->inputState->keys_pressed, 0, sizeof(metalWindow->inputState->keys_pressed));
    memset(metalWindow->inputState->keys_released, 0, sizeof(metalWindow->inputState->keys_released));
    metalWindow->inputState->mouse_scroll_dx = 0;
    metalWindow->inputState->mouse_scroll_dy = 0;

    if (!metalWindow->window || ![metalWindow->window isVisible]) {
        window->should_close = true;
        return;
    }

    [metalWindow->view setNeedsDisplay:YES];
}

void window_close(EseWindow* window) {
    if (!window) return;
    EseMetalWindow* metalWindow = (EseMetalWindow*)window->platform_window;
    if (metalWindow && metalWindow->window) {
        [metalWindow->window close]; // actually close the macOS window
    }
}

bool window_should_close(EseWindow* window) {
    if (!window) return true;
    return window->should_close;
}
