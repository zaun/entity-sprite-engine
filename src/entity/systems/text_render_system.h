/*
 * Project: Entity Sprite Engine
 *
 * Text rendering system for the Entity Component System. This system handles
 * text rendering by collecting all text components and submitting them to the
 * renderer in the LATE phase after all updates and Lua scripts have completed.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_TEXT_RENDER_SYSTEM_H
#define ESE_TEXT_RENDER_SYSTEM_H

typedef struct EseEngine EseEngine;
typedef struct EseSystemManager EseSystemManager;

// ========================================
// Defines and Structs
// ========================================

/**
 * @brief Creates and returns a new Text Render System.
 *
 * @details The text render system handles text rendering for all entities
 *          with text components. It runs in the LATE phase after all updates
 *          and Lua scripts have completed.
 *
 * @return Pointer to the newly created text render system.
 */
EseSystemManager *text_render_system_create(void);

/**
 * @brief Registers the text render system with the engine.
 *
 * @details Convenience function that creates and registers the text render
 *          system with the engine in one call.
 *
 * @param eng Pointer to the engine.
 */
void engine_register_text_render_system(EseEngine *eng);

#endif /* ESE_TEXT_RENDER_SYSTEM_H */
