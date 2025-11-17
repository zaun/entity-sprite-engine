# Comprehensive Immediate Mode GUI System Design

This document details the design of a flexible, immediate mode GUI system built with a C-style API. It prioritizes adaptability, performance, and ease of integration by utilizing a flexbox-inspired layout, a clear frame-based processing model, and robust context management for handling multiple UI instances.

## 1. Core Philosophy

*   **Immediate Mode Rendering:** The UI is defined and rendered entirely within each frame. The UI structure is rebuilt every frame, eliminating the need for a persistent retained scene graph, which leads to predictable state management and simplified debugging.
*   **Flexbox-Inspired Layout:** A powerful layout engine replaces rigid windowing metaphors. It allows UI elements to arrange themselves dynamically based on defined rules for alignment, justification, and spacing, offering unparalleled design flexibility.
*   **Context-Driven Operations:** All UI system functions operate via a `ui_context_t`. This context manages the system's state, including rendering data, input processing, layout stacks, and clipping regions.
*   **Frame Session Management:** The `ui_begin`/`ui_end` functions define discrete "frame sessions" within a context, each representing an independent UI canvas, layer, or window.

## 2. Key API Components & Concepts

### 2.1. Context Management

*   **`ui_context_t`:**
    *   The central orchestrator for the GUI system. It holds all necessary state for rendering, input, layout, and managing multiple UI sessions or layers.
    *   A single application may use one `ui_context_t` for all UI within a single OS window (managing multiple nested sessions) or multiple `ui_context_t` instances (one per OS window, each managing its own sessions).
*   **Initialization:**
    *   `void ui_init_context(ui_context_t* ctx, ...);`
    *   Initializes a `ui_context_t` instance. This might involve setting up rendering targets, input callbacks, or default system parameters.

### 2.2. Frame Session Management

These functions define individual, self-contained UI sessions within a `ui_context_t`. They are analogous to separate canvases or layers.

*   **`ui_begin(ui_context_t* ctx, int x, int y, int width, int height)`:**
    *   **Purpose:** Starts a new UI frame session within the given context. Defines the absolute clipping rectangle `(x, y, width, height)` for this entire session.
    *   **Role:** Acts as the **absolute root** for this UI session. It is *not* a layout node itself.
    *   **Behavior:** Resets the session's specific state (layout stack, interaction tracking) and establishes the clipping boundaries. It implicitly expects a single primary layout element (like `ui_open_flex` or `ui_open_stack`) to follow before `ui_end` is called for this session.
    *   **Analogy:** Similar to `ui_open_stack`, but defines the entire root area for a session, not just a localized sized region.

*   **`ui_end(ui_context_t* ctx)`:**
    *   **Purpose:** Finalizes the current UI frame session within the context.
    *   **Behavior:** Processes all layout calculations, handles input, executes callbacks for interactive elements, and queues drawing commands for the current session. If the context manages multiple nested sessions, `ui_end` processes the most recently opened one, then restores the state (clipping, layout stack) of the parent session.
    *   **Requirement:** Must be called after a corresponding `ui_begin` for the session, and all layout containers opened within that session must be properly closed.

### 2.3. Layout System (Flexbox Inspired)

These components define how UI elements are arranged *within* an active `ui_begin`/`ui_end` session.

*   **`ui_open_flex(ui_context_t* ctx, enum FlexDirection direction, enum FlexJustify justify, enum FlexAlignItems align_items, int spacing, int padding_left, int padding_top, int padding_right, int padding_bottom)`:**
    *   **Purpose:** Opens a new flexbox-style layout container. This container defines how its direct children are arranged.
    *   **Properties:**
        *   `direction`: Main axis (`FLEX_DIRECTION_ROW`, `FLEX_DIRECTION_COLUMN`, etc.).
        *   `justify`: Alignment along the main axis (`FLEX_JUSTIFY_START`, `FLEX_JUSTIFY_CENTER`, `FLEX_JUSTIFY_SPACE_BETWEEN`, etc.).
        *   `align_items`: Alignment along the cross axis (`FLEX_ALIGN_ITEMS_START`, `FLEX_ALIGN_ITEMS_CENTER`, `FLEX_ALIGN_ITEMS_STRETCH`, etc.).
        *   `spacing`: Gap between children.
        *   `padding`: Inner space within the container.
    *   **Behavior:** Pushes a new layout scope. Children added will be positioned according to these flex properties.

*   **`ui_close_flex(ui_context_t* ctx)`:**
    *   **Purpose:** Closes the current active flex container, returning to the parent layout scope.

*   **`ui_open_stack(ui_context_t* ctx, int width, int height)`:**
    *   **Purpose:** Creates a fixed-size bounding box for its *single child*.
    *   **Behavior:** Defines a specific `width` and `height`. This box is automatically centered both horizontally and vertically within the available space of its parent layout container.
    *   **Constraint:** Designed to hold only one direct child.

*   **`ui_close_stack(ui_context_t* ctx)`:**
    *   **Purpose:** Closes the current `ui_open_stack` scope.

### 2.4. UI Elements (Widgets)

These are the visual and interactive components of the UI. They are placed within layout containers.

*   **`ui_push_[widget_type](...)`:**
    *   **Examples:** `ui_push_button`, `ui_push_label`, `ui_push_checkbox`, `ui_push_slider`, `ui_push_textarea`, `ui_push_image`.
    *   **Purpose:** Creates and renders a specific UI element.
    *   **Behavior:**
        *   Operates within the current layout scope. Position and initial sizing are determined by the parent layout container.
        *   Widgets may accept styling parameters (e.g., text, font, color).
        *   Interactive widgets require unique identifiers (`id`) for state management and input routing.

#### Widget Details:

*   **`ui_push_button(ui_context_t* ctx, const char* id, const char* text, void (*callback)(ui_context_t* ctx));`**
    *   **Purpose:** Creates a clickable button.
    *   **Behavior:** Renders text and provides visual feedback (hover, pressed states). If clicked within the frame, the provided `callback` function is executed by `ui_end`. No return value is needed by the caller for action triggering.

*   **`ui_push_image(ui_context_t* ctx, const char* texture_id, enum ImageFit fit, ...);`**
    *   **Purpose:** Displays an image.
    *   **`enum ImageFit`:** Controls how the image content scales within its allocated space:
        *   `IMAGE_FIT_COVER`: Scales to fill, maintaining aspect ratio, clips excess.
        *   `IMAGE_FIT_CONTAIN`: Scales to fit within bounds, maintaining aspect ratio, potentially leaving empty space.
        *   `IMAGE_FIT_FILL`: Stretches to fill bounds, ignoring aspect ratio.
        *   `IMAGE_FIT_REPEAT`: Tiles the image within the allocated space.
    *   **Behavior:** The allocated space for the image is defined by its parent layout container. The `fit` parameter determines how the texture is rendered within that space.

## 3. Workflow and Usage Models

### 3.1. Single Context, Multiple Nested Sessions (Model A)

*   **Concept:** One `ui_context_t` manages multiple `ui_begin`/`ui_end` sessions, acting as distinct layers or canvases within a single rendering target (e.g., one OS window).
*   **Workflow:**
    1.  Initialize a single `ui_context_t`.
    2.  For each UI layer (HUD, dialog, inventory), call `ui_begin(&ctx, x, y, w, h)`.
    3.  Build the UI for that layer using `ui_open_flex`, `ui_push_*`, etc.
    4.  Call `ui_end(&ctx)` to finalize the layer's frame session.
    5.  The context internally manages the stack of sessions, clipping, and layout restoration.
    6.  A final rendering pass uses the data collected by the context to draw all completed sessions in the correct order.

    ```c
    // Example: HUD, Dialog, and Inventory in one window
    ui_context_t main_ui_ctx;
    ui_init_context(&main_ui_ctx, ...);

    // HUD Layer
    ui_begin(&main_ui_ctx, hud_x, hud_y, hud_w, hud_h);
        // ... HUD UI elements ...
    ui_end(&main_ui_ctx);

    // Dialog Layer (if active)
    if (is_dialog_active) {
        ui_begin(&main_ui_ctx, dialog_x, dialog_y, dialog_w, dialog_h);
            // ... Dialog UI elements ...
        ui_end(&main_ui_ctx);
    }

    // Inventory Layer (if open)
    if (is_inventory_open) {
        ui_begin(&main_ui_ctx, inventory_x, inventory_y, inventory_w, inventory_h);
            // ... Inventory UI elements ...
        ui_end(&main_ui_ctx);
    }

    // Final render pass for main_ui_ctx.
    ```

### 3.2. Multiple Contexts, One Session Per Context (Model B)

*   **Concept:** Each `ui_context_t` represents the entire UI system for a distinct OS window or independent rendering surface.
*   **Workflow:**
    1.  Create separate `ui_context_t` instances for each OS window (e.g., `window1_ctx`, `window2_ctx`).
    2.  For each window, manage its events and then call `ui_begin(&windowX_ctx, 0, 0, windowX_w, windowX_h)` and `ui_end(&windowX_ctx)` for its UI frame.
    3.  Within each window's context, you can use nested `ui_begin`/`ui_end` calls (Model A principles) for layering if needed.

    ```c
    // Example: Two separate OS windows
    ui_context_t window1_ctx;
    ui_context_t window2_ctx;
    ui_init_context(&window1_ctx, ...);
    ui_init_context(&window2_ctx, ...);

    // Main application loop polls events for both windows

    // Process Window 1
    ui_begin(&window1_ctx, 0, 0, window1_width, window1_height);
        // ... UI for Window 1 ...
    ui_end(&window1_ctx);

    // Process Window 2
    ui_begin(&window2_ctx, 0, 0, window2_width, window2_height);
        // ... UI for Window 2 ...
    ui_end(&window2_ctx);

    // Render results for Window 1 and Window 2 separately.
    ```

## 4. New Input Processing Component

*   **`ui_process_input(ui_context_t* ctx, const ui_input_event_t* event);`**
    *   **Purpose:** Injects raw input events into the GUI system for processing within a specific context.
    *   **`ui_input_event_t`:** A structure that encapsulates input data:
        *   `type`: (e.g., `INPUT_EVENT_MOUSE_MOVE`, `INPUT_EVENT_MOUSE_BUTTON_PRESS`, `INPUT_EVENT_KEY_PRESS`, `INPUT_EVENT_TEXT_INPUT`).
        *   `mouse_pos`: Current mouse coordinates (if applicable).
        *   `mouse_buttons`: Bitmask of pressed mouse buttons (if applicable).
        *   `key`: The specific key code (if applicable).
        *   `text_input`: UTF-8 string for text input (if applicable).
        *   `target_id`: Optional ID of the widget the input is directed to (for special cases).
    *   **Behavior:** This function should be called *before* `ui_begin` for the relevant context. It populates internal state within the `ui_context_t` that will be consumed during the `ui_end` processing phase. This allows interactive widgets to track hover states, button presses, and focus.
    *   **Integration:** Input events are typically global to a context's rendering area. The system will determine which widgets are under the cursor or have focus based on the input received.

## 5. Backend Rendering Interface (Revised)

The `ui_context_t` will now maintain an internal iterator or index for its draw commands.

*   **`void ui_prepare_for_rendering(ui_context_t* ctx);`**
    *   **Purpose:** Finalizes all layout and input processing, and prepares the internal draw command buffer for iteration. This is called *after* `ui_end` has been called for all desired sessions within a context.
    *   **Behavior:** This function consolidates all generated draw commands from the frame's sessions into a single, ordered stream within the `ui_context_t`. It resets any internal rendering cursors.

*   **`const ui_draw_command_t* ui_draw_next(ui_context_t* ctx);`**
    *   **Purpose:** Retrieves the next available draw command for rendering.
    *   **Behavior:** This function should be called repeatedly in a `while` loop *after* `ui_prepare_for_rendering`. It returns a pointer to the next `ui_draw_command_t` in the stream. When there are no more commands, it returns `NULL`.
    *   **Return Value:**
        *   A pointer to a `ui_draw_command_t` if a command is available.
        *   `NULL` when all commands for the current frame have been yielded.
    *   **Ownership:** The returned pointer is to data owned by the `ui_context_t`. It is valid only until the *next* call to `ui_draw_next` or until `ui_prepare_for_rendering` is called again.

## 6. Error Handling and Constraints

*   **`ui_begin` as Root:** `ui_begin` establishes the root scope. It must be followed by a primary layout node (e.g., `ui_open_flex` or `ui_open_stack`) before other layout nodes or widgets are added as direct children of the session.
*   **Single Child for `ui_open_stack`:** `ui_open_stack` strictly enforces a single direct child.
*   **Layout Nesting:** `ui_close_flex` and `ui_close_stack` must be called to match every `ui_open_flex` and `ui_open_stack`, respectively, ensuring a properly structured layout stack.
*   **`ui_end` Completion:** `ui_end` requires that all opened layout scopes within its session have been closed.
*   **Context Integrity:** Functions should validate context pointers and ensure operations occur within a valid `ui_begin`/`ui_end` session scope.

## 7. Advantages

*   **Flexibility:** Powerful layout options through flexbox and explicit sizing.
*   **Performance:** Immediate mode avoids retained graphics overhead; layout can be efficient.
*   **Predictability:** Frame-by-frame rendering simplifies state management and debugging.
*   **Modularity:** Support for multiple contexts and nested sessions allows for distinct UI layers and independent windows.
*   **Integrability:** A C-style API and clear responsibilities make integration into various rendering engines straightforward.
*   **No Windowing Burden:** Avoids the complexities of OS-level windowing APIs for UI layout.

---