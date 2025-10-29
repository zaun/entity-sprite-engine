#include "graphics/gui/gui_private.h"
#include "core/memory_manager.h"
#include "graphics/gui/gui_widget.h"
#include "utility/log.h"
#include <stdlib.h>
#include <string.h>

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// No longer need font callback and draw generation here; widgets handle their
// own drawing via vtables.

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// All layout and draw responsibilities have moved to widgets and high-level
// coordination in gui.c

void _ese_gui_layout_destroy(EseGuiLayout *layout) {
    if (layout == NULL) {
        return;
    }
    if (layout->root != NULL) {
        layout->root->type.destroy(layout->root);
        layout->root = NULL;
    }
    layout->current_widget = NULL;
}
