// asset_manager.c
#include "core/asset_manager.h"
#include "core/memory_manager.h"
#include "graphics/sprite.h"
#include "platform/filesystem.h"
#include "platform/renderer.h"
#include "scripting/lua_engine.h"
#include "types/types.h"
#include "utility/grouped_hashmap.h"
#include "utility/log.h"
#include "vendor/json/cJSON.h"
#include <string.h>

#define DEFAULT_GROUP "default"

typedef enum
{
    ASSET_SPRITE,
    ASSET_TEXTURE,
    ASSET_MAP,
    // ASSET_SOUND,              // Future use
    // ASSET_MUSIC,              // Future use
    // ASSET_PARTICLE_SYSTEM,    // Future use
    // ASSET_FONT,               // Future use
    // ASSET_MATERIAL            // Future use
} EseAssetType;

typedef struct EseAsset
{
    char *instance_id;
    EseAssetType type;
    void *data;
} EseAsset;

typedef struct EseAssetTexture
{
    int width;
    int heigh;
} EseAssetTexture;

struct EseAssetManager
{
    EseRenderer *renderer;
    EseGroupedHashMap *sprites;
    EseGroupedHashMap *textures;
    EseGroupedHashMap *atlases;
    EseGroupedHashMap *maps;

    // Group tracking
    char **groups;         // Array of group names
    size_t group_count;    // Number of groups in use
    size_t group_capacity; // Allocated capacity
};

/**
 * @brief Splits a string by a colon into a group and a name.
 * * @param input The constant string to split.
 * @param group A pointer to a char* where the group string will be stored.
 * The caller is responsible for freeing this memory.
 * Will be set to NULL if no group is found or if the input is invalid.
 * @param name A pointer to a char* where the name string will be stored.
 * The caller is responsible for freeing this memory.
 * Will be set to NULL if no name is found or if the input is invalid.
 */
void _split_string(const char *input, char **group, char **name)
{
    // Initialize output pointers to NULL
    *group = NULL;
    *name = NULL;

    // Handle NULL input immediately
    if (input == NULL)
    {
        return;
    }

    const char *colon = strchr(input, ':');

    if (colon == NULL)
    {
        // No colon: full string is the name, group is "default"
        *group = memory_manager.strdup("default", MMTAG_GENERAL);
        *name = memory_manager.strdup(input, MMTAG_GENERAL);
    }
    else
    {
        // Calculate lengths of potential group and name parts
        size_t groupLength = colon - input;
        size_t nameLength = strlen(colon + 1);

        // Case: "test:" or ":" (group exists, name is empty)
        if (nameLength == 0)
        {
            return;
        }

        // Case: ":test" (no group, name exists)
        if (groupLength == 0)
        {
            *group = memory_manager.strdup("default", MMTAG_GENERAL);
            *name = memory_manager.strdup(colon + 1, MMTAG_GENERAL);
        }
        // Case: "group:test" (both group and name exist)
        else
        {
            *group = (char *)memory_manager.malloc(groupLength + 1, MMTAG_GENERAL);
            if (*group)
            {
                strncpy(*group, input, groupLength);
                (*group)[groupLength] = '\0';
            }
            *name = memory_manager.strdup(colon + 1, MMTAG_GENERAL);
        }
    }
}

EseAsset *_asset_create()
{
    EseAsset *asset = memory_manager.malloc(sizeof(EseAsset), MMTAG_ASSET);

    return asset;
}

void _asset_free(void *data)
{
    EseAsset *asset = (EseAsset *)data;
    if (asset->type == ASSET_SPRITE)
    {
        EseSprite *sprite = (EseSprite *)asset->data;
        sprite_free(sprite);
    }
    else if (asset->type == ASSET_TEXTURE)
    {
        EseAssetTexture *texture = (EseAssetTexture *)asset->data;
        memory_manager.free(texture);
    }
    else if (asset->type == ASSET_MAP)
    {
        EseMap *map = (EseMap *)asset->data;
        map_destroy(map);
    }
    else
    {
        log_error("ASSET_MANAGER", "Unable to memory_manager.free unknown asset type");
    }
    memory_manager.free(asset);
}

static void _asset_manager_add_group(EseAssetManager *manager, const char *group)
{
    // Check if group already exists
    for (size_t i = 0; i < manager->group_count; i++)
    {
        if (strcmp(manager->groups[i], group) == 0)
        {
            return;
        }
    }

    // Resize if needed
    if (manager->group_count == manager->group_capacity)
    {
        size_t new_capacity = manager->group_capacity == 0 ? 4 : manager->group_capacity * 2;
        char **new_groups =
            memory_manager.realloc(manager->groups, new_capacity * sizeof(char *), MMTAG_ASSET);
        if (!new_groups)
        {
            log_error("ASSET_MANAGER", "Failed to allocate memory for groups array");
            return;
        }
        manager->groups = new_groups;
        manager->group_capacity = new_capacity;
    }

    manager->groups[manager->group_count++] = memory_manager.strdup(group, MMTAG_ASSET);
}

static void _asset_manager_remove_group(EseAssetManager *manager, const char *group)
{
    for (size_t i = 0; i < manager->group_count; i++)
    {
        if (strcmp(manager->groups[i], group) == 0)
        {
            memory_manager.free(manager->groups[i]);
            // Move last group to this slot
            manager->groups[i] = manager->groups[manager->group_count - 1];
            manager->group_count--;
            return;
        }
    }
}

cJSON *_asset_manager_load_json(const char *filename)
{
    char *full_path = filesystem_get_resource(filename);
    if (!full_path)
    {
        log_error("ENGINE", "Error: filesystem_get_resource failed for %s", filename);
        return NULL;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f)
    {
        log_error("ENGINE", "Error: Failed to open file %s", full_path);
        memory_manager.free(full_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0)
    {
        log_error("ENGINE", "Error: ftell failed for %s", full_path);
        fclose(f);
        memory_manager.free(full_path);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char *data = memory_manager.malloc(len + 1, MMTAG_ASSET);

    size_t read = fread(data, 1, len, f);
    if (read != (size_t)len)
    {
        log_error("ENGINE", "Error: fread failed for %s", full_path);
        memory_manager.free(data);
        fclose(f);
        memory_manager.free(full_path);
        return NULL;
    }
    data[len] = '\0';

    fclose(f);
    memory_manager.free(full_path);

    // Strip // style comments
    char *src = data;
    char *dst = data;
    bool in_str = false;
    bool esc = false;
    while (*src)
    {
        if (!in_str && src[0] == '/' && src[1] == '/')
        {
            src += 2;
            while (*src && *src != '\n')
                src++;
        }
        else
        {
            char c = *src++;
            if (in_str)
            {
                if (esc)
                {
                    esc = false;
                }
                else if (c == '\\')
                {
                    esc = true;
                }
                else if (c == '"')
                {
                    in_str = false;
                }
            }
            else if (c == '"')
            {
                in_str = true;
            }
            *dst++ = c;
        }
    }
    *dst = '\0';

    cJSON *json = cJSON_Parse(data);
    if (!json)
    {
        log_error("ENGINE", "Error: Failed to parse JSON from %s", filename);
    }
    memory_manager.free(data);

    return json;
}

EseAssetManager *asset_manager_create(EseRenderer *renderer)
{
    if (!renderer)
    {
        log_error("ASSET_MANAGER", "Error: asset_manager_create called with NULL renderer");
        return NULL;
    }

    EseAssetManager *manager = memory_manager.malloc(sizeof(EseAssetManager), MMTAG_ASSET);
    manager->renderer = renderer;
    manager->sprites = grouped_hashmap_create((EseGroupedHashMapFreeFn)_asset_free);
    manager->textures = grouped_hashmap_create((EseGroupedHashMapFreeFn)_asset_free);
    manager->atlases = grouped_hashmap_create(NULL);
    manager->maps = grouped_hashmap_create((EseGroupedHashMapFreeFn)_asset_free);

    manager->groups = NULL;      // init groups array
    manager->group_count = 0;    // init count
    manager->group_capacity = 0; // init capacity

    return manager;
}

void asset_manager_destroy(EseAssetManager *manager)
{
    if (!manager)
    {
        log_error("ASSET_MANAGER", "Error: asset_manager_destroy called with NULL manager");
        return;
    }

    grouped_hashmap_free(manager->sprites);
    grouped_hashmap_free(manager->textures);
    grouped_hashmap_free(manager->atlases);
    grouped_hashmap_free(manager->maps);

    for (size_t i = 0; i < manager->group_count; i++)
    {
        memory_manager.free(manager->groups[i]);
    }
    if (manager->groups)
    {
        memory_manager.free(manager->groups);
    }

    memory_manager.free(manager);
}

bool asset_manager_load_sprite_atlas(EseAssetManager *manager,
    const char *filename,
    const char *group)
{
    log_assert("ASSET_MANAGER", manager,
        "asset_manager_load_sprite_atlas_grouped called with NULL manager");
    log_assert("ASSET_MANAGER", filename,
        "asset_manager_load_sprite_atlas_grouped called with NULL filename");
    log_assert(
        "ASSET_MANAGER", group, "asset_manager_load_sprite_atlas_grouped called with NULL group");

    if (grouped_hashmap_get(manager->atlases, group, filename) == (void *)1)
    {
        return true;
    }

    cJSON *json = _asset_manager_load_json(filename);
    if (!json)
    {
        log_error("ASSET_MANAGER", "Error: Failed to load atlas JSON: %s", filename);
        return false;
    }

    // Get image property
    cJSON *image_item = cJSON_GetObjectItem(json, "image");
    if (!cJSON_IsString(image_item))
    {
        log_error("ASSET_MANAGER", "Error: 'image' property missing or not a string in atlas");
        cJSON_Delete(json);
        return false;
    }
    const char *image = image_item->valuestring;

    // Make the texture ID
    char *texture_id;
    size_t texture_id_len = strlen(group) + 1 + strlen(image) + 1;
    texture_id = memory_manager.malloc(texture_id_len, MMTAG_ASSET);
    snprintf(texture_id, texture_id_len, "%s:%s", group, image);

    // Load texture
    int img_width, img_height;
    renderer_load_texture_indexed(manager->renderer, texture_id, image, &img_width, &img_height);
    EseAsset *asset_texture = _asset_create();
    asset_texture->type = ASSET_TEXTURE;
    asset_texture->data = memory_manager.malloc(sizeof(EseAssetTexture), MMTAG_ASSET);
    grouped_hashmap_set(manager->textures, group, image, asset_texture);

    // Get regions array
    cJSON *frameData = cJSON_GetObjectItem(json, "frames");
    if (!cJSON_IsArray(frameData))
    {
        log_error("ASSET_MANAGER", "Error: 'frames' property missing or not an array in atlas");
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }

    // Get sprites array
    cJSON *sprites = cJSON_GetObjectItem(json, "sprites");
    if (!cJSON_IsArray(sprites))
    {
        log_error("ASSET_MANAGER", "Error: 'sprites' property missing or not an array in atlas");
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }

    // Makse sure the group is added to the manager
    _asset_manager_add_group(manager, group);

    // For each sprite (animation)
    int sprite_count = cJSON_GetArraySize(sprites);
    log_debug("ASSET_MANAGER", "Loading %d sprites from atlas", sprite_count);
    for (int i = 0; i < sprite_count; i++)
    {
        cJSON *sprite_obj = cJSON_GetArrayItem(sprites, i);
        if (!cJSON_IsObject(sprite_obj))
            continue;

        cJSON *name_item = cJSON_GetObjectItem(sprite_obj, "name");
        cJSON *speed_item = cJSON_GetObjectItem(sprite_obj, "speed");
        cJSON *frames = cJSON_GetObjectItem(sprite_obj, "frames");
        if (!cJSON_IsString(name_item) || !cJSON_IsNumber(speed_item) || !cJSON_IsArray(frames))
            continue;

        const char *spriteName = name_item->valuestring;
        EseSprite *sprite = sprite_create();
        if (!sprite)
        {
            log_error("ASSET_MANAGER", "Error: Failed to create sprite for %s", spriteName);
            continue;
        }

        int speed = speed_item->valueint;
        if (speed < 0)
        {
            speed = 0;
        }
        sprite_set_speed(sprite, speed / 1000.0f);

        int frame_count = cJSON_GetArraySize(frames);
        for (int j = 0; j < frame_count; j++)
        {
            cJSON *frame_name_item = cJSON_GetArrayItem(frames, j);
            if (!cJSON_IsString(frame_name_item))
                continue;
            const char *frame_name = frame_name_item->valuestring;

            // Find frame by name
            cJSON *frame = NULL;
            int region_count = cJSON_GetArraySize(frameData);
            for (int k = 0; k < region_count; k++)
            {
                cJSON *reg = cJSON_GetArrayItem(frameData, k);
                cJSON *reg_name = cJSON_GetObjectItem(reg, "name");
                if (cJSON_IsString(reg_name) && strcmp(reg_name->valuestring, frame_name) == 0)
                {
                    frame = reg;
                    break;
                }
            }
            if (!frame)
            {
                log_error("ASSET_MANAGER", "Error: Frame region '%s' not found for sprite '%s'",
                    frame_name, spriteName);
                continue;
            }

            cJSON *x_item = cJSON_GetObjectItem(frame, "x");
            cJSON *y_item = cJSON_GetObjectItem(frame, "y");
            cJSON *w_item = cJSON_GetObjectItem(frame, "width");
            cJSON *h_item = cJSON_GetObjectItem(frame, "height");
            if (!cJSON_IsNumber(x_item) || !cJSON_IsNumber(y_item) || !cJSON_IsNumber(w_item) ||
                !cJSON_IsNumber(h_item))
            {
                log_error("ASSET_MANAGER", "Error: Malformed region for frame '%s'", frame_name);
                continue;
            }

            // Add the sprite frame
            sprite_add_frame(sprite, texture_id, (float)x_item->valueint / (float)img_width,
                (float)y_item->valueint / (float)img_height,
                (float)(x_item->valueint + w_item->valueint) / (float)img_width,
                (float)(y_item->valueint + h_item->valueint) / (float)img_height, w_item->valueint,
                h_item->valueint);
        }

        if (sprite_get_frame_count(sprite) == 0)
        {
            log_error("ASSET_MANAGER", "Sprite '%s' has no valid frames; skipping", spriteName);
            sprite_free(sprite);
            continue;
        }

        EseAsset *asset = _asset_create();
        asset->type = ASSET_SPRITE;
        asset->data = (void *)sprite;

        grouped_hashmap_set(manager->sprites, group, spriteName, asset);
        log_debug("ASSET_MANAGER", "Adding sprite '%s' with %d frames from atlas", spriteName,
            sprite_get_frame_count(sprite));
    }

    cJSON_Delete(json);
    memory_manager.free(texture_id);

    grouped_hashmap_set(manager->atlases, group, filename, (void *)1);
    return true;
}

EseSprite *asset_manager_get_sprite(EseAssetManager *manager, const char *asset_id)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_get_sprite called with NULL manager");
    log_assert("ASSET_MANAGER", asset_id, "asset_manager_get_sprite called with NULL asset_id");

    char *out_group = NULL;
    char *out_name = NULL;
    _split_string(asset_id, &out_group, &out_name);

    EseAsset *asset = (EseAsset *)grouped_hashmap_get(manager->sprites, out_group, out_name);
    memory_manager.free(out_group);
    memory_manager.free(out_name);

    if (!asset)
    {
        return NULL;
    }

    return (EseSprite *)asset->data;
}

bool asset_manager_load_map(EseAssetManager *manager,
    EseLuaEngine *lua,
    const char *filename,
    const char *group)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_load_map called with NULL manager");
    log_assert("ASSET_MANAGER", lua, "asset_manager_load_map called with NULL lua");
    log_assert("ASSET_MANAGER", filename, "asset_manager_load_map called with NULL filename");
    log_assert("ASSET_MANAGER", group, "asset_manager_load_map called with NULL group");

    // Already loaded?
    if (grouped_hashmap_get(manager->maps, group, filename) != NULL)
    {
        return true;
    }

    cJSON *json = _asset_manager_load_json(filename);
    if (!json)
    {
        return false;
    }

    // Validate dimensions
    cJSON *width_item = cJSON_GetObjectItem(json, "width");
    cJSON *height_item = cJSON_GetObjectItem(json, "height");
    if (!cJSON_IsNumber(width_item) || !cJSON_IsNumber(height_item))
    {
        log_error("ASSET_MANAGER", "Map JSON missing width/height or not numbers: %s", filename);
        cJSON_Delete(json);
        return false;
    }
    int width = width_item->valueint;
    int height = height_item->valueint;
    if (width <= 0 || height <= 0)
    {
        log_error("ASSET_MANAGER", "Invalid map dimensions in %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Map type
    EseMapType map_type = MAP_TYPE_GRID;
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (cJSON_IsString(type_item))
    {
        map_type = map_type_from_string(type_item->valuestring);
    }

    // Tileset object must exist and be an object
    cJSON *tileset_obj = cJSON_GetObjectItem(json, "tileset");
    if (!tileset_obj || tileset_obj->type != cJSON_Object)
    {
        log_error("ASSET_MANAGER", "Map JSON missing or invalid 'tileset' object: %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Create a tileset (no Lua registration)
    EseTileSet *tileset = tileset_create(lua, true);
    if (!tileset)
    {
        log_error("ASSET_MANAGER", "Failed to create tileset for %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Populate tileset from JSON; validate keys and entries
    for (cJSON *tile_entry = tileset_obj->child; tile_entry; tile_entry = tile_entry->next)
    {
        if (!tile_entry->string)
        {
            log_error("ASSET_MANAGER", "Invalid tile key in tileset for %s", filename);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        char *endptr = NULL;
        long tid = strtol(tile_entry->string, &endptr, 10);
        if (*endptr != '\0' || tid < 0 || tid > 255)
        {
            log_error("ASSET_MANAGER", "Invalid tile id '%s' in tileset for %s", tile_entry->string,
                filename);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }
        uint8_t tile_id = (uint8_t)tid;

        if (!cJSON_IsArray(tile_entry))
        {
            log_error("ASSET_MANAGER", "Tileset entry for id %d is not an array in %s", tile_id,
                filename);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        int mapping_count = cJSON_GetArraySize(tile_entry);
        for (int mi = 0; mi < mapping_count; mi++)
        {
            cJSON *map_item = cJSON_GetArrayItem(tile_entry, mi);
            if (!map_item || !cJSON_IsObject(map_item))
            {
                log_error(
                    "ASSET_MANAGER", "Malformed mapping for tile %d in %s", tile_id, filename);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }

            cJSON *sprite_item = cJSON_GetObjectItem(map_item, "sprite");
            if (!sprite_item || !cJSON_IsString(sprite_item) ||
                strlen(sprite_item->valuestring) == 0)
            {
                log_error("ASSET_MANAGER", "Missing or invalid 'sprite' for tile %d in %s", tile_id,
                    filename);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            const char *sprite_str = sprite_item->valuestring;

            cJSON *weight_item = cJSON_GetObjectItem(map_item, "weight");
            int weight = 1;
            if (weight_item && cJSON_IsNumber(weight_item))
            {
                weight = weight_item->valueint;
            }
            if (weight <= 0)
                weight = 1;

            if (!tileset_add_sprite(tileset, tile_id, sprite_str, (uint16_t)weight))
            {
                log_error("ASSET_MANAGER", "Failed to add sprite '%s' for tile %d in %s",
                    sprite_str, tile_id, filename);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
        }
    }

    // Cells array must exist and match width*height
    cJSON *cells = cJSON_GetObjectItem(json, "cells");
    if (!cells || !cJSON_IsArray(cells))
    {
        log_error("ASSET_MANAGER", "Map JSON missing or invalid 'cells' array: %s", filename);
        tileset_destroy(tileset);
        cJSON_Delete(json);
        return false;
    }
    int cell_count = cJSON_GetArraySize(cells);
    if (cell_count != width * height)
    {
        log_error("ASSET_MANAGER", "Cells length (%d) != width*height (%d) in %s", cell_count,
            width * height, filename);
        tileset_destroy(tileset);
        cJSON_Delete(json);
        return false;
    }

    // Create map (no Lua registration)
    EseMap *map = map_create(lua, (uint32_t)width, (uint32_t)height, map_type, true);
    if (!map)
    {
        log_error("ASSET_MANAGER", "Failed to create map struct for %s", filename);
        tileset_destroy(tileset);
        cJSON_Delete(json);
        return false;
    }

    // Set metadata if present
    cJSON *title_item = cJSON_GetObjectItem(json, "title");
    if (title_item && cJSON_IsString(title_item))
    {
        map_set_title(map, title_item->valuestring);
    }
    cJSON *author_item = cJSON_GetObjectItem(json, "author");
    if (author_item && cJSON_IsString(author_item))
    {
        map_set_author(map, author_item->valuestring);
    }
    cJSON *version_item = cJSON_GetObjectItem(json, "version");
    if (version_item && cJSON_IsNumber(version_item))
    {
        map_set_version(map, version_item->valueint);
    }

    // Parse and set each cell
    for (int ci = 0; ci < cell_count; ci++)
    {
        cJSON *cell_obj = cJSON_GetArrayItem(cells, ci);
        if (!cell_obj || !cJSON_IsObject(cell_obj))
        {
            log_error("ASSET_MANAGER", "Invalid cell object at index %d in %s", ci, filename);
            map_destroy(map);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        cJSON *layers = cJSON_GetObjectItem(cell_obj, "layers");
        if (!layers || !cJSON_IsArray(layers))
        {
            log_error("ASSET_MANAGER", "Cell %d missing 'layers' array in %s", ci, filename);
            map_destroy(map);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        EseMapCell *tmp = mapcell_create(lua, true);
        if (!tmp)
        {
            log_error("ASSET_MANAGER", "Failed to allocate map cell for %s", filename);
            map_destroy(map);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        int layer_count = cJSON_GetArraySize(layers);
        for (int li = 0; li < layer_count; li++)
        {
            cJSON *lid_item = cJSON_GetArrayItem(layers, li);
            if (!lid_item || !cJSON_IsNumber(lid_item))
            {
                log_error("ASSET_MANAGER", "Invalid layer id at cell %d layer %d in %s", ci, li,
                    filename);
                mapcell_destroy(tmp);
                map_destroy(map);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            long lid = lid_item->valueint;
            if (lid < 0 || lid > 255)
            {
                log_error("ASSET_MANAGER", "Out of range tile id %ld at cell %d in %s", lid, ci,
                    filename);
                mapcell_destroy(tmp);
                map_destroy(map);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            uint8_t tile_id = (uint8_t)lid;

            // Ensure tile_id exists in tileset
            if (tileset_get_sprite_count(tileset, tile_id) == 0)
            {
                log_error("ASSET_MANAGER",
                    "Tile id %d used in cells but not defined in tileset for %s", tile_id,
                    filename);
                mapcell_destroy(tmp);
                map_destroy(map);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }

            if (!mapcell_add_layer(tmp, tile_id))
            {
                log_error("ASSET_MANAGER", "Failed to add layer for tile %d at cell %d in %s",
                    tile_id, ci, filename);
                mapcell_destroy(tmp);
                map_destroy(map);
                tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
        }

        // Optional flags
        cJSON *flags_item = cJSON_GetObjectItem(cell_obj, "flags");
        if (flags_item && cJSON_IsNumber(flags_item))
        {
            tmp->flags = (uint32_t)flags_item->valueint;
        }

        // Optional isDynamic (boolean or numeric)
        cJSON *dyn_item = cJSON_GetObjectItem(cell_obj, "isDynamic");
        if (dyn_item)
        {
            if (dyn_item->type == cJSON_True)
            {
                tmp->isDynamic = true;
            }
            else if (dyn_item->type == cJSON_False)
            {
                tmp->isDynamic = false;
            }
            else if (cJSON_IsNumber(dyn_item))
            {
                tmp->isDynamic = (dyn_item->valueint != 0);
            }
        }

        uint32_t x = (uint32_t)(ci % width);
        uint32_t y = (uint32_t)(ci / width);

        if (!map_set_cell(map, x, y, tmp))
        {
            log_error("ASSET_MANAGER", "Failed to set cell (%u,%u) for %s", x, y, filename);
            mapcell_destroy(tmp);
            map_destroy(map);
            tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        // tmp copied into map; free temporary
        mapcell_destroy(tmp);
    }

    // Attach tileset to map
    map_set_tileset(map, tileset);

    // Register group and store asset
    _asset_manager_add_group(manager, group);
    EseAsset *asset = _asset_create();
    asset->type = ASSET_MAP;
    asset->data = (void *)map;
    grouped_hashmap_set(manager->maps, group, filename, asset);

    cJSON_Delete(json);
    return true;
}

EseMap *asset_manager_get_map(EseAssetManager *manager, const char *asset_id)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_get_map called with NULL manager");
    log_assert("ASSET_MANAGER", asset_id, "asset_manager_get_map called with NULL asset_id");

    char *out_group = NULL;
    char *out_name = NULL;
    _split_string(asset_id, &out_group, &out_name);

    EseAsset *asset = (EseAsset *)grouped_hashmap_get(manager->maps, out_group, out_name);
    memory_manager.free(out_group);
    memory_manager.free(out_name);

    if (!asset)
    {
        return NULL;
    }

    return (EseMap *)asset->data;
}

void asset_manager_remove_group(EseAssetManager *manager, const char *group)
{
    if (!manager)
    {
        log_error("ASSET_MANAGER", "Error: asset_manager_remove_group called with NULL manager");
        return;
    }

    if (!group)
    {
        log_error("ASSET_MANAGER", "Error: asset_manager_remove_group called with NULL group");
        return;
    }

    grouped_hashmap_remove_group(manager->sprites, group);
    grouped_hashmap_remove_group(manager->textures, group);
    grouped_hashmap_remove_group(manager->atlases, group);
    grouped_hashmap_remove_group(manager->maps, group);

    _asset_manager_remove_group(manager, group);
}
