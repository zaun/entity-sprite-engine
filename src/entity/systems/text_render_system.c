/*
 * Project: Entity Sprite Engine
 *
 * Implementation of the Text Render System. Collects text components and
 * renders them to the draw list in the LATE phase, converting world coordinates
 * to screen coordinates using the camera.
 *
 * Details:
 * The system maintains a dynamic array of text component pointers for efficient
 * rendering. Components are added/removed via callbacks. During update, text is
 * rendered with proper justification, alignment, and camera-relative
 * positioning.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#include "entity/systems/text_render_system.h"
#include "core/engine.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_private.h"
#include "entity/components/entity_component_text.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "graphics/draw_list.h"
#include "graphics/font.h"
#include "types/point.h"
#include "utility/log.h"

// ========================================
// Defines and Structs
// ========================================

// Font constants (matching console font)
#define FONT_CHAR_WIDTH 10
#define FONT_CHAR_HEIGHT 20
#define FONT_SPACING 1

/**
 * @brief Internal data for the text render system.
 *
 * Maintains a dynamically-sized array of text component pointers for efficient
 * rendering during the LATE phase.
 */
typedef struct {
  EseEntityComponentText **texts; /** Array of text component pointers */
  size_t count;                   /** Current number of tracked texts */
  size_t capacity;                /** Allocated capacity of the array */
} TextRenderSystemData;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

static void _text_font_texture_callback(float screen_x, float screen_y,
                                        float screen_w, float screen_h,
                                        uint64_t z_index,
                                        const char *texture_id,
                                        float texture_x1, float texture_y1,
                                        float texture_x2, float texture_y2,
                                        int width, int height, void *user_data);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

/**
 * @brief Checks if the system accepts this component type.
 *
 * @param self System manager instance
 * @param comp Component to check
 * @return true if component type is ENTITY_COMPONENT_TEXT
 */
static bool text_render_sys_accepts(EseSystemManager *self,
                                    const EseEntityComponent *comp) {
  (void)self;
  if (!comp) {
    return false;
  }
  return comp->type == ENTITY_COMPONENT_TEXT;
}

/**
 * @brief Called when a text component is added to an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was added
 */
static void text_render_sys_on_add(EseSystemManager *self, EseEngine *eng,
                                   EseEntityComponent *comp) {
  (void)eng;
  TextRenderSystemData *d = (TextRenderSystemData *)self->data;

  // Expand array if needed
  if (d->count == d->capacity) {
    d->capacity = d->capacity ? d->capacity * 2 : 64;
    d->texts = memory_manager.realloc(
        d->texts, sizeof(EseEntityComponentText *) * d->capacity,
        MMTAG_RS_TEXT);
  }

  // Add text to tracking array
  d->texts[d->count++] = (EseEntityComponentText *)comp->data;
}

/**
 * @brief Called when a text component is removed from an entity.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param comp Component that was removed
 */
static void text_render_sys_on_remove(EseSystemManager *self, EseEngine *eng,
                                      EseEntityComponent *comp) {
  (void)eng;
  TextRenderSystemData *d = (TextRenderSystemData *)self->data;
  EseEntityComponentText *tc = (EseEntityComponentText *)comp->data;

  // Find and remove text from tracking array (swap with last element)
  for (size_t i = 0; i < d->count; i++) {
    if (d->texts[i] == tc) {
      d->texts[i] = d->texts[--d->count];
      return;
    }
  }
}

/**
 * @brief Initialize the text render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void text_render_sys_init(EseSystemManager *self, EseEngine *eng) {
  (void)eng;
  TextRenderSystemData *d =
      memory_manager.calloc(1, sizeof(TextRenderSystemData), MMTAG_RS_TEXT);
  self->data = d;
}

/**
 * @brief Render all text components.
 *
 * Iterates through all tracked text components and submits them to the
 * renderer.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 * @param dt Delta time (unused)
 */
static void text_render_sys_update(EseSystemManager *self, EseEngine *eng,
                                   float dt) {
  (void)dt;
  TextRenderSystemData *d = (TextRenderSystemData *)self->data;

  for (size_t i = 0; i < d->count; i++) {
    EseEntityComponentText *tc = d->texts[i];

    // Skip texts without content or inactive entities
    if (!tc->text || strlen(tc->text) == 0 || !tc->base.entity ||
        !tc->base.entity->active || !tc->base.entity->visible) {
      continue;
    }

    // Calculate text dimensions
    int text_width =
        strlen(tc->text) * (FONT_CHAR_WIDTH + FONT_SPACING) - FONT_SPACING;
    int text_height = FONT_CHAR_HEIGHT;

    // Get entity world position
    float entity_x = ese_point_get_x(tc->base.entity->position);
    float entity_y = ese_point_get_y(tc->base.entity->position);

    // Apply offset
    float final_x = entity_x + ese_point_get_x(tc->offset);
    float final_y = entity_y + ese_point_get_y(tc->offset);

    // Apply justification (horizontal alignment)
    switch (tc->justify) {
    case TEXT_JUSTIFY_CENTER:
      final_x -= text_width / 2.0f;
      break;
    case TEXT_JUSTIFY_RIGHT:
      final_x -= text_width;
      break;
    case TEXT_JUSTIFY_LEFT:
    default:
      break;
    }

    // Apply alignment (vertical alignment)
    switch (tc->align) {
    case TEXT_ALIGN_CENTER:
      final_y -= text_height / 2.0f;
      break;
    case TEXT_ALIGN_BOTTOM:
      final_y -= text_height;
      break;
    case TEXT_ALIGN_TOP:
    default:
      break;
    }

    // Convert world coordinates to screen coordinates using camera
    EseCamera *camera = engine_get_camera(eng);
    EseDisplay *display = engine_get_display(eng);
    float camera_x = ese_point_get_x(camera->position);
    float camera_y = ese_point_get_y(camera->position);
    float view_width = ese_display_get_viewport_width(display);
    float view_height = ese_display_get_viewport_height(display);

    float view_left = camera_x - view_width / 2.0f;
    float view_top = camera_y - view_height / 2.0f;

    float screen_x = final_x - view_left;
    float screen_y = final_y - view_top;

    // Use the font drawing function
    EseDrawList *draw_list = engine_get_draw_list(eng);
    font_draw_text(eng, "console_font_10x20", tc->text, screen_x, screen_y,
                   tc->base.entity->draw_order, _text_font_texture_callback,
                   draw_list);
  }
}

/**
 * @brief Clean up the text render system.
 *
 * @param self System manager instance
 * @param eng Engine pointer
 */
static void text_render_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
  (void)eng;
  TextRenderSystemData *d = (TextRenderSystemData *)self->data;
  if (d) {
    if (d->texts) {
      memory_manager.free(d->texts);
    }
    memory_manager.free(d);
  }
}

/**
 * @brief Virtual table for the text render system.
 */
static const EseSystemManagerVTable TextRenderSystemVTable = {
    .init = text_render_sys_init,
    .update = text_render_sys_update,
    .accepts = text_render_sys_accepts,
    .on_component_added = text_render_sys_on_add,
    .on_component_removed = text_render_sys_on_remove,
    .shutdown = text_render_sys_shutdown};

/**
 * @brief Callback function for font texture rendering.
 *
 * @param screen_x Screen X position
 * @param screen_y Screen Y position
 * @param screen_w Screen width
 * @param screen_h Screen height
 * @param z_index Z index for draw order
 * @param texture_id Texture identifier
 * @param texture_x1 Texture X1 coordinate
 * @param texture_y1 Texture Y1 coordinate
 * @param texture_x2 Texture X2 coordinate
 * @param texture_y2 Texture Y2 coordinate
 * @param width Texture width
 * @param height Texture height
 * @param user_data Draw list pointer
 */
static void _text_font_texture_callback(
    float screen_x, float screen_y, float screen_w, float screen_h,
    uint64_t z_index, const char *texture_id, float texture_x1,
    float texture_y1, float texture_x2, float texture_y2, int width, int height,
    void *user_data) {
  EseDrawList *draw_list = (EseDrawList *)user_data;
  EseDrawListObject *text_obj = draw_list_request_object(draw_list);
  draw_list_object_set_texture(text_obj, texture_id, texture_x1, texture_y1,
                               texture_x2, texture_y2);
  draw_list_object_set_bounds(text_obj, screen_x, screen_y, (int)screen_w,
                              (int)screen_h);
  draw_list_object_set_z_index(text_obj, z_index);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Create a text render system.
 *
 * @return EseSystemManager* Created system
 */
EseSystemManager *text_render_system_create(void) {
  return system_manager_create(&TextRenderSystemVTable, SYS_PHASE_LATE, NULL);
}

/**
 * @brief Register the text render system with the engine.
 *
 * @param eng Engine pointer
 */
void engine_register_text_render_system(EseEngine *eng) {
  log_assert("TEXT_RENDER_SYS", eng,
             "engine_register_text_render_system called with NULL engine");
  EseSystemManager *sys = text_render_system_create();
  engine_add_system(eng, sys);
}
