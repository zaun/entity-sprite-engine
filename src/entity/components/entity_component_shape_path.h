#ifndef ESE_ENTITY_COMPONENT_SHARE_PATH_H
#define ESE_ENTITY_COMPONENT_SHARE_PATH_H

#include <stddef.h> // size_t

// Forward declarations
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EsePolyLine EsePolyLine;

/**
 * @brief Convert an SVG path string to an array of PolyLine objects.
 *
 * @param engine Lua engine used for allocations and object creation
 * @param path   NUL-terminated SVG path data string
 * @param scale  Scale to apply to all coordinates (1.0f = no scale)
 * @param out_count Optional out parameter to receive number of returned polylines
 * @return EsePolyLine** Pointer to an array of polylines; caller owns and must free entries and array
 */
EsePolyLine **shape_path_to_polylines(EseLuaEngine *engine, float scale, const char *path, size_t *out_count);

#endif // ESE_ENTITY_COMPONENT_SHARE_PATH_H
