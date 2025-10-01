#ifndef ESE_ENTITY_COMPONENT_TEXT_H
#define ESE_ENTITY_COMPONENT_TEXT_H

#include <string.h>
#include "vendor/lua/src/lua.h"
#include "entity/components/entity_component_private.h" // EseEntityComponent

#define ENTITY_COMPONENT_TEXT_PROXY_META "EntityComponentTextProxyMeta"

// Forward declarations
typedef struct EseEntity EseEntity;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePoint EsePoint;

/**
 * @brief Text justification options for horizontal alignment
 */
typedef enum {
    TEXT_JUSTIFY_LEFT,
    TEXT_JUSTIFY_CENTER,
    TEXT_JUSTIFY_RIGHT,
} EseTextJustify;

/**
 * @brief Text alignment options for vertical alignment
 */
typedef enum {
    TEXT_ALIGN_TOP,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_BOTTOM,
} EseTextAlign;

/**
 * @brief Component that provides text rendering capabilities to an entity.
 * 
 * @details This component manages text display with configurable justification,
 *          alignment, and offset positioning. It stores the text content,
 *          justification and alignment settings, and offset from the entity position.
 *          The text is rendered using the console font system.
 */
typedef struct EseEntityComponentText {
    EseEntityComponent base;        /** Base component structure */

    char *text;                     /** The text string to display */
    EseTextJustify justify;         /** Horizontal text justification */
    EseTextAlign align;             /** Vertical text alignment */
    EsePoint *offset;               /** Offset from entity position */
} EseEntityComponentText;

EseEntityComponent *_entity_component_text_copy(const EseEntityComponentText *src);

void _entity_component_text_destroy(EseEntityComponentText *component);

void _entity_component_text_update(EseEntityComponentText *component, EseEntity *entity, float delta_time);

EseEntityComponentText *_entity_component_text_get(lua_State *L, int idx);

void _entity_component_text_init(EseLuaEngine *engine);

void _entity_component_text_draw(EseEntityComponentText *component, float screen_x, float screen_y, EntityDrawTextureCallback texCallback, void *callback_user_data);

EseEntityComponent *entity_component_text_create(EseLuaEngine *engine, const char *text);

#endif // ESE_ENTITY_COMPONENT_TEXT_H
