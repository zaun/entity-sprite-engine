/*
 * Project: Entity Sprite Engine
 *
 * Lua bindings for entity collider components.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#ifndef ESE_COLLIDER_BINDING_H
#define ESE_COLLIDER_BINDING_H

#include "scripting/lua_engine.h"

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * @brief Initialize Lua bindings for the collider entity component.
 *
 * @details Registers the `EntityComponentCollider` proxy metatable,
 * `EntityComponentCollider` global table, and the internal rects proxy used to
 * expose collider rectangles to Lua.
 *
 * @param engine Lua engine used to register the bindings. Must not be NULL.
 */
void entity_component_collider_init(EseLuaEngine *engine);

#endif // ESE_COLLIDER_BINDING_H
