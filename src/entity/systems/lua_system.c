#include "entity/systems/lua_system.h"
#include "core/engine_private.h"
#include "core/memory_manager.h"
#include "core/system_manager.h"
#include "core/system_manager_private.h"
#include "entity/components/entity_component_lua.h"
#include "entity/components/entity_component_private.h"
#include "utility/log.h"
#include "utility/profile.h"

// ========================================
// Defines and Structs
// ========================================

typedef struct {
	EseEntityComponentLua **components;
	size_t count;
	size_t capacity;
} LuaSystemData;

// ========================================
// PRIVATE FUNCTIONS
// ========================================

static bool lua_sys_accepts(EseSystemManager *self, const EseEntityComponent *comp) {
	(void)self;
	if (!comp) {
		return false;
	}
	return comp->type == ENTITY_COMPONENT_LUA;
}

static void lua_sys_on_add(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
	(void)eng;
	LuaSystemData *d = (LuaSystemData *)self->data;

	if (d->count == d->capacity) {
		d->capacity = d->capacity ? d->capacity * 2 : 64;
		d->components = memory_manager.realloc(
		    d->components, sizeof(EseEntityComponentLua *) * d->capacity, MMTAG_ENGINE);
	}

	d->components[d->count++] = (EseEntityComponentLua *)comp->data;
}

static void lua_sys_on_remove(EseSystemManager *self, EseEngine *eng, EseEntityComponent *comp) {
	(void)eng;
	LuaSystemData *d = (LuaSystemData *)self->data;
	EseEntityComponentLua *ptr = (EseEntityComponentLua *)comp->data;

	for (size_t i = 0; i < d->count; i++) {
		if (d->components[i] == ptr) {
			d->components[i] = d->components[--d->count];
			return;
		}
	}
}

static void lua_sys_init(EseSystemManager *self, EseEngine *eng) {
	(void)eng;
	LuaSystemData *d = memory_manager.calloc(1, sizeof(LuaSystemData), MMTAG_ENGINE);
	self->data = d;
}

static void lua_sys_update(EseSystemManager *self, EseEngine *eng, float dt) {
	(void)eng;
	LuaSystemData *d = (LuaSystemData *)self->data;

	for (size_t i = 0; i < d->count; i++) {
		EseEntityComponentLua *component = d->components[i];

		profile_start(PROFILE_ENTITY_COMP_LUA_UPDATE);

		if (component->script == NULL) {
			profile_cancel(PROFILE_ENTITY_COMP_LUA_UPDATE);
			continue;
		}

		if (component->instance_ref == LUA_NOREF) {
			profile_start(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE);
			component->instance_ref = lua_engine_instance_script(component->engine, component->script);
			profile_stop(PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE, "entity_comp_lua_instance_create");

			if (component->instance_ref == LUA_NOREF) {
				profile_cancel(PROFILE_ENTITY_COMP_LUA_UPDATE);
				profile_count_add("entity_comp_lua_update_instance_creation_failed");
				continue;
			}

			profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE);
			_entity_component_lua_cache_functions(component);
			profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE, "entity_comp_lua_function_cache");

			profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
			entity_component_lua_run(component, component->base.entity, "entity_init", 0, NULL);
			profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN, "entity_comp_lua_init_function");

			profile_count_add("entity_comp_lua_update_first_time_setup");
		}

		profile_start(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN);
		lua_value_set_number(component->arg, dt);
		EseLuaValue *args[] = {component->arg};
		entity_component_lua_run(component, component->base.entity, "entity_update", 1, args);
		profile_stop(PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN, "entity_comp_lua_update_function");

		profile_stop(PROFILE_ENTITY_COMP_LUA_UPDATE, "lua_system_component_update");
	}
}

static void lua_sys_shutdown(EseSystemManager *self, EseEngine *eng) {
	(void)eng;
	LuaSystemData *d = (LuaSystemData *)self->data;
	if (d) {
		if (d->components) {
			memory_manager.free(d->components);
		}
		memory_manager.free(d);
	}
}

static const EseSystemManagerVTable LuaSystemVTable = {
	.init = lua_sys_init,
	.update = lua_sys_update,
	.accepts = lua_sys_accepts,
	.on_component_added = lua_sys_on_add,
	.on_component_removed = lua_sys_on_remove,
	.shutdown = lua_sys_shutdown};

// ========================================
// PUBLIC FUNCTIONS
// ========================================

EseSystemManager *lua_system_create(void) {
	return system_manager_create(&LuaSystemVTable, SYS_PHASE_LUA, NULL);
}

void engine_register_lua_system(EseEngine *eng) {
	log_assert("LUA_SYS", eng, "engine_register_lua_system called with NULL engine");
	EseSystemManager *sys = lua_system_create();
	engine_add_system(eng, sys);
}
