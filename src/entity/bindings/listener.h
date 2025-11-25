/*
 * Project: Entity Sprite Engine
 *
 * Lua bindings for entity listener components.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#ifndef ESE_LISTENER_BINDING_H
#define ESE_LISTENER_BINDING_H

#include "scripting/lua_engine.h"

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Initialize Lua bindings for the listener entity component.
 *
 * @details Registers the `EntityComponentListener` proxy metatable,
 * `EntityComponentListener` global table, and the internal rects proxy used to
 * expose listener rectangles to Lua.
 *
 * @param engine Lua engine used to register the bindings. Must not be NULL.
 */
void entity_component_listener_init(EseLuaEngine *engine);

#endif // ESE_LISTENER_BINDING_H
