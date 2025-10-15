#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>
#include <stddef.h>

// Snapshot structure for storing timing data
typedef struct {
    char *key;
    uint64_t total;
    uint64_t count;
    uint64_t max;
    char *snapshot_name;
} ProfileSnapshot;

#ifdef __cplusplus
extern "C" {
#endif

// Timer ID constants for engine_update
#define PROFILE_ENG_UPDATE_OVERALL              0
#define PROFILE_ENG_UPDATE_SECTION              1

// Timer ID constants for lua_value
#define PROFILE_LUA_VALUE_RESET_OVERALL         2
#define PROFILE_LUA_VALUE_RESET_SECTION         3
#define PROFILE_LUA_VALUE_SET                   4

// Timer ID constants for lua_engine
#define PROFILE_LUA_ENGINE_LOAD_SCRIPT          5
#define PROFILE_LUA_ENGINE_LOAD_SCRIPT_STRING   6
#define PROFILE_LUA_ENGINE_INSTANCE_SCRIPT      7
#define PROFILE_LUA_ENGINE_RUN_FUNCTION         8
#define PROFILE_LUA_ENGINE_RUN_FUNCTION_REF     9
#define PROFILE_LUA_ENGINE_FUNCTION_LOOKUP      10
#define PROFILE_LUA_ENGINE_ARG_CONVERSION       11
#define PROFILE_LUA_ENGINE_LUA_EXECUTION        12
#define PROFILE_LUA_ENGINE_HOOK_SETUP           13
#define PROFILE_LUA_ENGINE_HOOK_CLEANUP         14
#define PROFILE_LUA_ENGINE_RESULT_CONVERSION    15
#define PROFILE_LUA_ENGINE_ALLOC                16

// Timer ID constants for entity_component_lua
#define PROFILE_ENTITY_COMP_LUA_INSTANCE_CREATE 17
#define PROFILE_ENTITY_COMP_LUA_FUNCTION_CACHE  18
#define PROFILE_ENTITY_COMP_LUA_FUNCTION_RUN    19

// Timer ID constants for entity_component_map
#define PROFILE_ENTITY_COMP_MAP_INSTANCE_CREATE 20
#define PROFILE_ENTITY_COMP_MAP_FUNCTION_CACHE  21
#define PROFILE_ENTITY_COMP_MAP_FUNCTION_RUN    22

// Timer ID constants for entity system
#define PROFILE_ENTITY_UPDATE_OVERALL           20
#define PROFILE_ENTITY_UPDATE_SECTION           21
#define PROFILE_ENTITY_COMPONENT_UPDATE         22
#define PROFILE_ENTITY_COLLISION_DETECT         23
#define PROFILE_ENTITY_COLLISION_CALLBACK       24
#define PROFILE_ENTITY_DRAW_OVERALL             25
#define PROFILE_ENTITY_DRAW_SECTION             26

// Timer ID constants for entity creation/destruction
#define PROFILE_ENTITY_CREATE                   27
#define PROFILE_ENTITY_DESTROY                  28
#define PROFILE_ENTITY_COPY                     29
#define PROFILE_ENTITY_COMPONENT_ADD            30
#define PROFILE_ENTITY_COMPONENT_REMOVE         31
#define PROFILE_ENTITY_COMPONENT_COPY           32
#define PROFILE_ENTITY_COMPONENT_DESTROY        33

// Timer ID constants for entity collision system
#define PROFILE_ENTITY_COLLISION_TEST           34
#define PROFILE_ENTITY_COLLISION_KEY_GEN        35
#define PROFILE_ENTITY_COLLISION_BOUNDS_UPDATE  36
#define PROFILE_ENTITY_COLLISION_RECT_DETECT    37

// Timer ID constants for entity drawing system
#define PROFILE_ENTITY_DRAW_VISIBILITY          38
#define PROFILE_ENTITY_DRAW_SCREEN_POS          39
#define PROFILE_ENTITY_DRAW_CALLBACK            40

// Timer ID constants for entity component specific operations
#define PROFILE_ENTITY_COMP_COLLIDER_UPDATE     41
#define PROFILE_ENTITY_COMP_LUA_UPDATE          42
#define PROFILE_ENTITY_COMP_MAP_UPDATE          43
#define PROFILE_ENTITY_COMP_SHAPE_UPDATE        44
#define PROFILE_ENTITY_COMP_SPRITE_UPDATE       45
#define PROFILE_ENTITY_COMP_TEXT_UPDATE         46
#define PROFILE_ENTITY_COMP_COLLIDER_DRAW       47
#define PROFILE_ENTITY_COMP_MAP_DRAW            48
#define PROFILE_ENTITY_COMP_SHAPE_DRAW          49
#define PROFILE_ENTITY_COMP_SPRITE_DRAW         50
#define PROFILE_ENTITY_COMP_TEXT_DRAW           51

// Timer ID constants for entity Lua operations
#define PROFILE_ENTITY_LUA_REGISTER             52
#define PROFILE_ENTITY_LUA_FUNCTION_CALL        53
#define PROFILE_ENTITY_LUA_PROPERTY_ACCESS      54

// Timer ID constants for point Lua operations
#define PROFILE_LUA_POINT_INDEX                 55
#define PROFILE_LUA_POINT_NEWINDEX              56
#define PROFILE_LUA_POINT_NEW                   57
#define PROFILE_LUA_POINT_ZERO                  58
#define PROFILE_LUA_POINT_FROM_JSON             59
#define PROFILE_LUA_POINT_TO_JSON               60

// Timer ID constants for rect Lua operations
#define PROFILE_LUA_RECT_INDEX                  61
#define PROFILE_LUA_RECT_NEWINDEX               62
#define PROFILE_LUA_RECT_NEW                    63
#define PROFILE_LUA_RECT_ZERO                   64
#define PROFILE_LUA_RECT_FROM_JSON              65
#define PROFILE_LUA_RECT_TO_JSON                66

// Timer ID constants for uuid Lua operations
#define PROFILE_LUA_UUID_INDEX                  67
#define PROFILE_LUA_UUID_NEWINDEX               68
#define PROFILE_LUA_UUID_NEW                    69

// Timer ID constants for ray Lua operations
#define PROFILE_LUA_RAY_INDEX                   70
#define PROFILE_LUA_RAY_NEWINDEX                71
#define PROFILE_LUA_RAY_NEW                     72
#define PROFILE_LUA_RAY_ZERO                    73
#define PROFILE_LUA_RAY_FROM_JSON               74
#define PROFILE_LUA_RAY_TO_JSON                 75

// Timer ID constants for input_state Lua operations
#define PROFILE_LUA_INPUT_STATE_INDEX           76
#define PROFILE_LUA_INPUT_STATE_NEWINDEX        77

// Timer ID constants for display Lua operations
#define PROFILE_LUA_DISPLAY_INDEX               78
#define PROFILE_LUA_DISPLAY_NEWINDEX            79

// Timer ID constants for camera Lua operations
#define PROFILE_LUA_CAMERA_INDEX                80
#define PROFILE_LUA_CAMERA_NEWINDEX             81

// Timer ID constants for arc Lua operations
#define PROFILE_LUA_ARC_INDEX                   82
#define PROFILE_LUA_ARC_NEWINDEX                83
#define PROFILE_LUA_ARC_NEW                     84
#define PROFILE_LUA_ARC_ZERO                    85

// Timer ID constants for vector Lua operations
#define PROFILE_LUA_VECTOR_INDEX                86
#define PROFILE_LUA_VECTOR_NEWINDEX             87
#define PROFILE_LUA_VECTOR_NEW                  88
#define PROFILE_LUA_VECTOR_ZERO                 89

// Timer ID constants for color Lua operations
#define PROFILE_LUA_COLOR_INDEX                 90
#define PROFILE_LUA_COLOR_NEWINDEX              91
#define PROFILE_LUA_COLOR_NEW                   92
#define PROFILE_LUA_COLOR_WHITE                 93
#define PROFILE_LUA_COLOR_BLACK                 94
#define PROFILE_LUA_COLOR_RED                   95
#define PROFILE_LUA_COLOR_GREEN                 96
#define PROFILE_LUA_COLOR_BLUE                  97
#define PROFILE_LUA_COLOR_SET_HEX               98
#define PROFILE_LUA_COLOR_SET_BYTE              99

// Timer ID constants for poly_line Lua operations
#define PROFILE_LUA_POLY_LINE_INDEX             100
#define PROFILE_LUA_POLY_LINE_NEWINDEX          101
#define PROFILE_LUA_POLY_LINE_NEW               102
#define PROFILE_LUA_POLY_LINE_ADD_POINT         103
#define PROFILE_LUA_POLY_LINE_REMOVE_POINT      104
#define PROFILE_LUA_POLY_LINE_GET_POINT         105
#define PROFILE_LUA_POLY_LINE_GET_POINT_COUNT   106
#define PROFILE_LUA_POLY_LINE_CLEAR_POINTS      107

// MapCell
#define PROFILE_LUA_map_cell_INDEX               108
#define PROFILE_LUA_map_cell_NEWINDEX            109
#define PROFILE_LUA_map_cell_NEW                 110

// Tileset
#define PROFILE_LUA_TILESET_INDEX               111
#define PROFILE_LUA_TILESET_NEWINDEX            112
#define PROFILE_LUA_TILESET_NEW                 113

#define PROFILE_LUA_MAP_NEW                     114

// CollisionHit Lua operations
#define PROFILE_LUA_COLLISION_HIT_INDEX         115


// Entity component collision pair checks
#define PROFILE_ENTITY_COMP_MAP_COLLIDES        116
#define PROFILE_ENTITY_COMP_COLLIDER_COLLIDES   117

// Spatial index and resolver profiling
#define PROFILE_SPATIAL_INDEX_SECTION           118
#define PROFILE_COLLISION_RESOLVER_SECTION      119

// Entity component dispatch profiling
#define PROFILE_ENTITY_COMPONENT_DISPATCH       120


#ifdef ESE_PROFILE_ENABLED

// API
void profile_time(const char *key, uint64_t nanoseconds);
uint64_t profile_get_max(const char *key);
uint64_t profile_get_average(const char *key);
uint64_t profile_get_count(const char *key);
void profile_clear(const char *key);
void profile_reset_all(void);
void profile_destroy(void);
void profile_display(void);

// Snapshot API
void profile_snapshot(const char *name);
ProfileSnapshot* profile_snapshot_get(const char *name);

// Timer API (supports up to 100 concurrent timers)
void profile_start(int id);
void profile_cancel(int id);
void profile_stop(int id, const char *key);

// Count API (simple counting without timing)
void profile_count_add(const char *key);
void profile_count_remove(const char *key);
uint64_t profile_count_get(const char *key);
void profile_count_clear(const char *key);
void profile_count_reset_all(void);

#else // ESE_PROFILE_ENABLED not defined

// Compile-out stubs
static inline void profile_time(const char *key, uint64_t ns) { (void)key; (void)ns; }
static inline uint64_t profile_get_max(const char *key) { (void)key; return 0; }
static inline uint64_t profile_get_average(const char *key) { (void)key; return 0; }
static inline uint64_t profile_get_count(const char *key) { (void)key; return 0; }
static inline void profile_clear(const char *key) { (void)key; }
static inline void profile_reset_all(void) {}
static inline void profile_destroy(void) {}
static inline void profile_display(void) {}
static inline void profile_start(int id) { (void)id; }
static inline void profile_cancel(int id) { (void)id; }
static inline void profile_stop(int id, const char *key) { (void)id; (void)key; }

// Snapshot API compile-out stubs
static inline void profile_snapshot(const char *name) { (void)name; }
static inline ProfileSnapshot* profile_snapshot_get(const char *name) { (void)name; return NULL; }

// Count API compile-out stubs
static inline void profile_count_add(const char *key) { (void)key; }
static inline void profile_count_remove(const char *key) { (void)key; }
static inline uint64_t profile_count_get(const char *key) { (void)key; return 0; }
static inline void profile_count_clear(const char *key) { (void)key; }
static inline void profile_count_reset_all(void) {}

#endif // ESE_PROFILE_ENABLED

#ifdef __cplusplus
}
#endif

#endif // PROFILE_H