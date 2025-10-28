#ifndef ESE_COLLISION_HIT_H
#define ESE_COLLISION_HIT_H

/* --- Forward Declarations
 * --------------------------------------------------------------------- */
typedef struct lua_State lua_State;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseEntity EseEntity;
typedef struct EseRect EseRect;
typedef struct EseMap EseMap;
typedef struct EseLuaValue EseLuaValue;

/* --- Macros
 * -----------------------------------------------------------------------------------
 */
#define COLLISION_HIT_META "CollisionHitMeta"

/* --- Public Types
 * -----------------------------------------------------------------------------
 */
/**
 * @brief Identifies the source/type of a collision hit.
 */
typedef enum EseCollisionKind {
  COLLISION_KIND_COLLIDER = 1, /**< Collider-to-collider hit */
  COLLISION_KIND_MAP = 2       /**< Collider-to-map hit */
} EseCollisionKind;

/**
 * @brief State of the collision within the frame timeline.
 */
typedef enum EseCollisionState {
  COLLISION_STATE_NONE = 0,  /**< Not colliding */
  COLLISION_STATE_ENTER = 1, /**< Began colliding this frame */
  COLLISION_STATE_STAY = 2,  /**< Continued colliding this frame */
  COLLISION_STATE_LEAVE = 3  /**< Stopped colliding this frame */
} EseCollisionState;

/**
 * @brief Opaque handle to a collision hit description.
 */
typedef struct EseCollisionHit EseCollisionHit; // Opaque

/* --- Lua API
 * ----------------------------------------------------------------------------------
 */
/**
 * @brief Registers the `EseCollisionHit` metatable and its constants in Lua.
 *
 * Creates the `CollisionHitMeta` metatable and sets the global
 * `EseCollisionHit` table containing `TYPE` and `STATE` constant tables.
 */
void ese_collision_hit_lua_init(EseLuaEngine *engine);

/**
 * @brief Pushes an `EseCollisionHit` to Lua, creating userdata if needed.
 */
void ese_collision_hit_lua_push(EseCollisionHit *hit);

/**
 * @brief Extracts an `EseCollisionHit*` from a Lua userdata.
 */
EseCollisionHit *ese_collision_hit_lua_get(lua_State *L, int idx);

/**
 * @brief Adds a Lua registry reference; increments internal ref-count.
 */
void ese_collision_hit_ref(EseCollisionHit *hit);

/**
 * @brief Removes a Lua registry reference when ref-count reaches zero.
 */
void ese_collision_hit_unref(EseCollisionHit *hit);

/* --- C API
 * ------------------------------------------------------------------------------------
 */
/**
 * @brief Creates a new `EseCollisionHit` bound to the engine's Lua state.
 */
EseCollisionHit *ese_collision_hit_create(EseLuaEngine *engine);

/**
 * @brief Deep copy of an existing `EseCollisionHit` (Lua refs are not copied).
 */
EseCollisionHit *ese_collision_hit_copy(const EseCollisionHit *src);

/**
 * @brief Destroys an `EseCollisionHit` or defers via unref if Lua owns it.
 */
void ese_collision_hit_destroy(EseCollisionHit *hit);

/* --- Property Access
 * -------------------------------------------------------------------------- */
/** @brief Gets the collision kind. */
EseCollisionKind ese_collision_hit_get_kind(const EseCollisionHit *hit);
/**
 * @brief Sets the collision kind and clears non-matching data.
 *
 * Switching to COLLIDER clears map pointer and destroys owned cell_x/cell_y.
 * Switching to MAP clears (destroys) the owned rect.
 */
void ese_collision_hit_set_kind(EseCollisionHit *hit, EseCollisionKind kind);
EseCollisionState ese_collision_hit_get_state(const EseCollisionHit *hit);
void ese_collision_hit_set_state(EseCollisionHit *hit, EseCollisionState state);
EseEntity *ese_collision_hit_get_entity(const EseCollisionHit *hit);
void ese_collision_hit_set_entity(EseCollisionHit *hit, EseEntity *entity);
EseEntity *ese_collision_hit_get_target(const EseCollisionHit *hit);
void ese_collision_hit_set_target(EseCollisionHit *hit, EseEntity *target);
/**
 * @brief Sets the collider rect by copying the source; the hit owns the copy.
 */
void ese_collision_hit_set_rect(EseCollisionHit *hit, const EseRect *rect);
/** @brief Gets the owned collider rect pointer (may be NULL). */
EseRect *ese_collision_hit_get_rect(const EseCollisionHit *hit);
void ese_collision_hit_set_map(EseCollisionHit *hit, EseMap *map);
EseMap *ese_collision_hit_get_map(const EseCollisionHit *hit);

/** @brief Sets the map cell X coordinate; hit owns and replaces the value. */
void ese_collision_hit_set_cell_x(EseCollisionHit *hit, int cell_x);

/** @brief Gets the cell X coordinate; returns 0 if not set. */
int ese_collision_hit_get_cell_x(const EseCollisionHit *hit);

/** @brief Sets the map cell Y coordinate; hit owns and replaces the value. */
void ese_collision_hit_set_cell_y(EseCollisionHit *hit, int cell_y);

/** @brief Gets the cell Y coordinate; returns 0 if not set. */
int ese_collision_hit_get_cell_y(const EseCollisionHit *hit);

/** @brief Gets the Lua state associated with the collision hit. */
lua_State *ese_collision_hit_get_state_ptr(const EseCollisionHit *hit);

/** @brief Gets the Lua registry reference for the collision hit. */
int ese_collision_hit_get_lua_ref(const EseCollisionHit *hit);

/** @brief Gets the Lua reference count for the collision hit. */
int ese_collision_hit_get_lua_ref_count(const EseCollisionHit *hit);

#endif // ESE_COLLISION_HIT_H
