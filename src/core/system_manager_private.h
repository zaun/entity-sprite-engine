/*
 * Project: Entity Sprite Engine
 *
 * Private/internal definitions for the System architecture. This header exposes
 * the internal structure of EseSystem for use by system implementations that
 * need to access their user data. This should only be included by system
 * implementation files.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */
#ifndef ESE_SYSTEM_MANAGER_PRIVATE_H
#define ESE_SYSTEM_MANAGER_PRIVATE_H

#include "core/system_manager.h"

/**
 * @brief Internal structure for a System instance.
 */
struct EseSystemManager {
  const EseSystemManagerVTable
      *vt;              /** Virtual table defining system behavior */
  EseSystemPhase phase; /** Execution phase for this system */
  void *data;           /** User-defined data for system-specific state */
  bool active;          /** Whether this system is currently active */
};

#endif /* ESE_SYSTEM_MANAGER_PRIVATE_H */
