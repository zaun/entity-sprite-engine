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
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

#define DEFAULT_GROUP "default"

typedef enum {
    ASSET_SPRITE,
    ASSET_TEXTURE,
    ASSET_MAP,
    // ASSET_SOUND,              // Future use
    // ASSET_MUSIC,              // Future use
    // ASSET_PARTICLE_SYSTEM,    // Future use
    // ASSET_FONT,               // Future use
    // ASSET_MATERIAL            // Future use
} EseAssetType;

/**
 * @brief Represents a single asset in the asset management system.
 * 
 * @details This structure stores metadata about a loaded asset including
 *          its unique instance ID, type classification, and pointer to
 *          the actual asset data. The instance_id is heap-allocated
 *          and must be freed when the asset is destroyed.
 */
typedef struct EseAsset {
    char *instance_id;              /** Unique identifier for this asset instance */
    EseAssetType type;              /** Type classification of the asset */
    void *data;                     /** Pointer to the actual asset data */
} EseAsset;

/**
 * @brief Structure for texture asset metadata.
 * 
 * @details This structure stores metadata about loaded textures including
 *          dimensions and any additional properties needed for texture
 *          management and rendering.
 */
typedef struct EseAssetTexture {
    int width;                      /** Width of the texture in pixels */
    int height;                     /** Height of the texture in pixels */
} EseAssetTexture;

/**
 * @brief Main asset management system.
 * 
 * @details This structure manages all game assets including sprites, textures,
 *          atlases, and maps. It organizes assets by groups for efficient
 *          loading, caching, and retrieval. The renderer reference is used
 *          for texture loading operations.
 */
struct EseAssetManager {
    EseRenderer *renderer;          /** Reference to renderer for texture operations */
    EseGroupedHashMap *sprites;     /** Hash map of sprite assets by group and ID */
    EseGroupedHashMap *textures;    /** Hash map of texture assets by group and ID */
    EseGroupedHashMap *atlases;     /** Hash map of atlas assets by group and ID */
    EseGroupedHashMap *maps;        /** Hash map of map assets by group and ID */

    // Group tracking
    char **groups;                  /** Array of group names for asset organization */
    size_t group_count;             /** Number of groups currently in use */
    size_t group_capacity;          /** Allocated capacity for groups array */
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
void _split_string(
    const char *input,
    char **group,
    char **name)
{
    // Initialize output pointers to NULL
    *group = NULL;
    *name = NULL;

    // Handle NULL input immediately
    if (input == NULL) {
        return;
    }

    const char *colon = strchr(input, ':');

    if (colon == NULL) {
        // No colon: full string is the name, group is "default"
        *group = memory_manager.strdup("default", MMTAG_GENERAL);
        *name = memory_manager.strdup(input, MMTAG_GENERAL);
    } else {
        // Calculate lengths of potential group and name parts
        size_t groupLength = colon - input;
        size_t nameLength = strlen(colon + 1);

        // Case: "test:" or ":" (group exists, name is empty)
        if (nameLength == 0) {
            return;
        }

        // Case: ":test" (no group, name exists)
        if (groupLength == 0) {
            *group = memory_manager.strdup("default", MMTAG_GENERAL);
            *name = memory_manager.strdup(colon + 1, MMTAG_GENERAL);
        }
        // Case: "group:test" (both group and name exist)
        else {
            *group = (char *)memory_manager.malloc(groupLength + 1, MMTAG_GENERAL);
            if (*group) {
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

void _asset_free(
    void *data)
{
    EseAsset *asset = (EseAsset *)data;
    if (asset->type == ASSET_SPRITE) {
        EseSprite *sprite = (EseSprite *)asset->data;
        sprite_free(sprite);
    } else if (asset->type == ASSET_TEXTURE) {
        EseAssetTexture *texture = (EseAssetTexture *)asset->data;
        memory_manager.free(texture);
    } else if (asset->type == ASSET_MAP) {
        EseMap *map = (EseMap *)asset->data;
        ese_map_destroy(map);
    } else {
        log_error("ASSET_MANAGER", "Unable to memory_manager.free unknown asset type");
    }
    memory_manager.free(asset);
}

static void _asset_manager_add_group(
    EseAssetManager *manager,
    const char *group)
{
    // Check if group already exists
    for (size_t i = 0; i < manager->group_count; i++) {
        if (strcmp(manager->groups[i], group) == 0) {
            return;
        }
    }

    // Resize if needed
    if (manager->group_count == manager->group_capacity) {
        size_t new_capacity = manager->group_capacity == 0 ? 4 : manager->group_capacity * 2;
        char **new_groups =
            memory_manager.realloc(manager->groups, new_capacity * sizeof(char *), MMTAG_ASSET);
        if (!new_groups) {
            log_error("ASSET_MANAGER", "Failed to allocate memory for groups array");
            return;
        }
        manager->groups = new_groups;
        manager->group_capacity = new_capacity;
    }

    manager->groups[manager->group_count++] = memory_manager.strdup(group, MMTAG_ASSET);
}

static void _asset_manager_remove_group(
    EseAssetManager *manager,
    const char *group)
{
    for (size_t i = 0; i < manager->group_count; i++) {
        if (strcmp(manager->groups[i], group) == 0) {
            memory_manager.free(manager->groups[i]);
            // Move last group to this slot
            manager->groups[i] = manager->groups[manager->group_count - 1];
            manager->group_count--;
            return;
        }
    }
}

cJSON *_asset_manager_load_json(
    const char *filename)
{
    char *full_path = filesystem_get_resource(filename);
    if (!full_path) {
        log_error("ENGINE", "Error: filesystem_get_resource failed for %s", filename);
        return NULL;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        log_error("ENGINE", "Error: Failed to open file %s", full_path);
        memory_manager.free(full_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        log_error("ENGINE", "Error: ftell failed for %s", full_path);
        fclose(f);
        memory_manager.free(full_path);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char *data = memory_manager.malloc(len + 1, MMTAG_ASSET);

    size_t read = fread(data, 1, len, f);
    if (read != (size_t)len) {
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
    while (*src) {
        if (!in_str && src[0] == '/' && src[1] == '/') {
            src += 2;
            while (*src && *src != '\n')
                src++;
        } else {
            char c = *src++;
            if (in_str) {
                if (esc) {
                    esc = false;
                } else if (c == '\\') {
                    esc = true;
                } else if (c == '"') {
                    in_str = false;
                }
            } else if (c == '"') {
                in_str = true;
            }
            *dst++ = c;
        }
    }
    *dst = '\0';

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        log_error("ENGINE", "Error: Failed to parse JSON from %s", filename);
    }
    memory_manager.free(data);

    return json;
}

EseAssetManager *asset_manager_create(
    EseRenderer *renderer)
{
    if (!renderer) {
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

void asset_manager_destroy(
    EseAssetManager *manager)
{
    if (!manager) {
        log_error("ASSET_MANAGER", "Error: asset_manager_destroy called with NULL manager");
        return;
    }

    grouped_hashmap_free(manager->sprites);
    grouped_hashmap_free(manager->textures);
    grouped_hashmap_free(manager->atlases);
    grouped_hashmap_free(manager->maps);

    for (size_t i = 0; i < manager->group_count; i++) {
        memory_manager.free(manager->groups[i]);
    }
    if (manager->groups) {
        memory_manager.free(manager->groups);
    }

    memory_manager.free(manager);
}

bool asset_manager_load_sprite_atlas(
    EseAssetManager *manager,
    const char *filename,
    const char *group,
    bool indexed)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_load_sprite_atlas_grouped called with NULL manager");
    log_assert("ASSET_MANAGER", filename, "asset_manager_load_sprite_atlas_grouped called with NULL filename");
    log_assert("ASSET_MANAGER", group, "asset_manager_load_sprite_atlas_grouped called with NULL group");

    if (grouped_hashmap_get(manager->atlases, group, filename) == (void *)1) {
        return true;
    }

    cJSON *json = _asset_manager_load_json(filename);
    if (!json) {
        log_error("ASSET_MANAGER", "Error: Failed to load atlas JSON: %s", filename);
        return false;
    }

    // Get image property
    cJSON *image_item = cJSON_GetObjectItem(json, "image");
    if (!cJSON_IsString(image_item)) {
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

    // Load image with stb_image (probe extensions only if none provided)
    int img_width, img_height, img_channels;
    unsigned char *image_data = NULL;
    char *image_path = NULL;
    
    const char *extensions[] = {"png", "jpg", "jpeg", "bmp"};
    char full_filename[256];
    
    const char *dot = strchr(image, '.');
    if (dot) {
        // Image already has an extension; use it directly
        char *temp_path = filesystem_get_resource(image);
        if (temp_path) {
            if (access(temp_path, F_OK) == 0) {
                image_path = temp_path;
            } else {
                memory_manager.free(temp_path);
            }
        }
    } else {
        // Probe common extensions when none provided
        for (int i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
            if (snprintf(full_filename, sizeof(full_filename), "%s.%s", image, extensions[i]) >= sizeof(full_filename)) {
                continue;
            }
            
            char *temp_path = filesystem_get_resource(full_filename);
            if (temp_path) {
                if (access(temp_path, F_OK) == 0) {
                    image_path = temp_path;
                    break;
                }
                memory_manager.free(temp_path);
            }
        }
    }
    
    if (!image_path) {
        if (dot) {
            log_error("ASSET_MANAGER", "Error: Image file not found: %s", image);
        } else {
            log_error("ASSET_MANAGER", "Error: Image file not found: %s (tried png, jpg, jpeg, bmp)", image);
        }
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }
    
    image_data = stbi_load(image_path, &img_width, &img_height, &img_channels, 4);
    memory_manager.free(image_path);
    
    if (!image_data) {
        log_error("ASSET_MANAGER", "Error: Failed to load image: %s", image);
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }
    
    unsigned char *processed_data = image_data;
    
    // Process indexed texture if requested
    if (indexed) {
        // Get transparent color from top-left pixel
        unsigned char tr = image_data[0];
        unsigned char tg = image_data[1];
        unsigned char tb = image_data[2];
        
        // Allocate buffer for processed data
        size_t buffer_size = img_width * img_height * 4;
        processed_data = memory_manager.malloc(buffer_size, MMTAG_ASSET);
        if (!processed_data) {
            log_error("ASSET_MANAGER", "Error: Failed to allocate memory for indexed texture processing");
            stbi_image_free(image_data);
            cJSON_Delete(json);
            memory_manager.free(texture_id);
            return false;
        }
        
        // Process pixels: convert transparency key to alpha
        for (int y = 0; y < img_height; y++) {
            unsigned char *src_row = image_data + y * img_width * 4;
            unsigned char *dst_row = processed_data + y * img_width * 4;
            for (int x = 0; x < img_width; x++) {
                unsigned char *src_pixel = src_row + x * 4;
                unsigned char *dst_pixel = dst_row + x * 4;
                
                dst_pixel[0] = src_pixel[0]; // R
                dst_pixel[1] = src_pixel[1]; // G
                dst_pixel[2] = src_pixel[2]; // B
                dst_pixel[3] = (src_pixel[0] == tr && src_pixel[1] == tg && src_pixel[2] == tb) ? 0 : 255; // A
            }
        }
    }
    
    // Load texture using renderer_load_texture
    if (!renderer_load_texture(manager->renderer, texture_id, processed_data, img_width, img_height)) {
        log_error("ASSET_MANAGER", "Error: Failed to load texture for image: %s", image);
        if (indexed && processed_data != image_data) {
            memory_manager.free(processed_data);
        }
        stbi_image_free(image_data);
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }
    
    // Create texture asset
    EseAsset *asset_texture = _asset_create();
    asset_texture->type = ASSET_TEXTURE;
    asset_texture->data = memory_manager.malloc(sizeof(EseAssetTexture), MMTAG_ASSET);
    if (asset_texture->data) {
        EseAssetTexture *tex_data = (EseAssetTexture *)asset_texture->data;
        tex_data->width = img_width;
        tex_data->height = img_height;
    }
    grouped_hashmap_set(manager->textures, group, image, asset_texture);
    
    // Free the processed data if it was allocated separately
    if (indexed && processed_data != image_data) {
        memory_manager.free(processed_data);
    }
    
    // Free the original image data
    stbi_image_free(image_data);

    // Get regions array
    cJSON *frameData = cJSON_GetObjectItem(json, "frames");
    if (!cJSON_IsArray(frameData)) {
        log_error("ASSET_MANAGER", "Error: 'frames' property missing or not an array in atlas");
        cJSON_Delete(json);
        memory_manager.free(texture_id);
        return false;
    }

    // Get sprites array
    cJSON *sprites = cJSON_GetObjectItem(json, "sprites");
    if (!cJSON_IsArray(sprites)) {
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
    for (int i = 0; i < sprite_count; i++) {
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
        if (!sprite) {
            log_error("ASSET_MANAGER", "Error: Failed to create sprite for %s", spriteName);
            continue;
        }

        int speed = speed_item->valueint;
        if (speed < 0) {
            speed = 0;
        }
        sprite_set_speed(sprite, speed / 1000.0f);

        int frame_count = cJSON_GetArraySize(frames);
        for (int j = 0; j < frame_count; j++) {
            cJSON *frame_name_item = cJSON_GetArrayItem(frames, j);
            if (!cJSON_IsString(frame_name_item))
                continue;
            const char *frame_name = frame_name_item->valuestring;

            // Find frame by name
            cJSON *frame = NULL;
            int region_count = cJSON_GetArraySize(frameData);
            for (int k = 0; k < region_count; k++) {
                cJSON *reg = cJSON_GetArrayItem(frameData, k);
                cJSON *reg_name = cJSON_GetObjectItem(reg, "name");
                if (cJSON_IsString(reg_name) && strcmp(reg_name->valuestring, frame_name) == 0) {
                    frame = reg;
                    break;
                }
            }
            if (!frame) {
                log_error("ASSET_MANAGER", "Error: Frame region '%s' not found for sprite '%s'",
                    frame_name, spriteName);
                continue;
            }

            cJSON *x_item = cJSON_GetObjectItem(frame, "x");
            cJSON *y_item = cJSON_GetObjectItem(frame, "y");
            cJSON *w_item = cJSON_GetObjectItem(frame, "width");
            cJSON *h_item = cJSON_GetObjectItem(frame, "height");
            if (!cJSON_IsNumber(x_item) || !cJSON_IsNumber(y_item) || !cJSON_IsNumber(w_item) ||
                !cJSON_IsNumber(h_item)) {
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

        if (sprite_get_frame_count(sprite) == 0) {
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

EseSprite *asset_manager_get_sprite(
    EseAssetManager *manager,
    const char *asset_id)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_get_sprite called with NULL manager");
    log_assert("ASSET_MANAGER", asset_id, "asset_manager_get_sprite called with NULL asset_id");

    char *out_group = NULL;
    char *out_name = NULL;
    _split_string(asset_id, &out_group, &out_name);

    EseAsset *asset = (EseAsset *)grouped_hashmap_get(manager->sprites, out_group, out_name);
    memory_manager.free(out_group);
    memory_manager.free(out_name);

    if (!asset) {
        return NULL;
    }

    return (EseSprite *)asset->data;
}

bool asset_manager_load_map(
    EseAssetManager *manager,
    EseLuaEngine *lua,
    const char *filename,
    const char *group)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_load_map called with NULL manager");
    log_assert("ASSET_MANAGER", lua, "asset_manager_load_map called with NULL lua");
    log_assert("ASSET_MANAGER", filename, "asset_manager_load_map called with NULL filename");
    log_assert("ASSET_MANAGER", group, "asset_manager_load_map called with NULL group");

    // Already loaded?
    if (grouped_hashmap_get(manager->maps, group, filename) != NULL) {
        return true;
    }

    cJSON *json = _asset_manager_load_json(filename);
    if (!json) {
        return false;
    }

    // Validate dimensions
    cJSON *width_item = cJSON_GetObjectItem(json, "width");
    cJSON *height_item = cJSON_GetObjectItem(json, "height");
    if (!cJSON_IsNumber(width_item) || !cJSON_IsNumber(height_item)) {
        log_error("ASSET_MANAGER", "Map JSON missing width/height or not numbers: %s", filename);
        cJSON_Delete(json);
        return false;
    }
    int width = width_item->valueint;
    int height = height_item->valueint;
    if (width <= 0 || height <= 0) {
        log_error("ASSET_MANAGER", "Invalid map dimensions in %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Map type
    EseMapType ese_map_type = MAP_TYPE_GRID;
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (cJSON_IsString(type_item)) {
        ese_map_type = ese_map_type_from_string(type_item->valuestring);
    }

    // Tileset object must exist and be an object
    cJSON *tileset_obj = cJSON_GetObjectItem(json, "tileset");
    if (!tileset_obj || tileset_obj->type != cJSON_Object) {
        log_error("ASSET_MANAGER", "Map JSON missing or invalid 'tileset' object: %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Create a tileset (C-only, not Lua-registered)
    EseTileSet *tileset = ese_tileset_create(lua);
    if (!tileset) {
        log_error("ASSET_MANAGER", "Failed to create tileset for %s", filename);
        cJSON_Delete(json);
        return false;
    }

    // Populate tileset from JSON
    for (cJSON *tile_entry = tileset_obj->child; tile_entry; tile_entry = tile_entry->next) {
        if (!tile_entry->string) {
            log_error("ASSET_MANAGER", "Invalid tile key in tileset for %s", filename);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        char *endptr = NULL;
        long tid = strtol(tile_entry->string, &endptr, 10);
        if (*endptr != '\0' || tid < 0 || tid > 255) {
            log_error("ASSET_MANAGER", "Invalid tile id '%s' in tileset for %s",
                      tile_entry->string, filename);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }
        uint8_t tile_id = (uint8_t)tid;

        if (!cJSON_IsArray(tile_entry)) {
            log_error("ASSET_MANAGER", "Tileset entry for id %d is not an array in %s",
                      tile_id, filename);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        int mapping_count = cJSON_GetArraySize(tile_entry);
        for (int mi = 0; mi < mapping_count; mi++) {
            cJSON *ese_map_item = cJSON_GetArrayItem(tile_entry, mi);
            if (!ese_map_item || !cJSON_IsObject(ese_map_item)) {
                log_error("ASSET_MANAGER", "Malformed mapping for tile %d in %s",
                          tile_id, filename);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }

            cJSON *sprite_item = cJSON_GetObjectItem(ese_map_item, "sprite");
            if (!sprite_item || !cJSON_IsString(sprite_item) ||
                strlen(sprite_item->valuestring) == 0) {
                log_error("ASSET_MANAGER", "Missing or invalid 'sprite' for tile %d in %s",
                          tile_id, filename);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            size_t sprite_len = strlen(group) + 1 + strlen(sprite_item->valuestring) + 1;
            char *sprite_str = (char *)memory_manager.malloc(sprite_len, MMTAG_ASSET);
            snprintf(sprite_str, sprite_len, "%s:%s", group, sprite_item->valuestring);

            cJSON *weight_item = cJSON_GetObjectItem(ese_map_item, "weight");
            int weight = 1;
            if (weight_item && cJSON_IsNumber(weight_item)) {
                weight = weight_item->valueint;
            }
            if (weight <= 0) weight = 1;

            if (!ese_tileset_add_sprite(tileset, tile_id, sprite_str, (uint16_t)weight)) {
                log_error("ASSET_MANAGER", "Failed to add sprite '%s' for tile %d in %s",
                          sprite_str, tile_id, filename);
                memory_manager.free(sprite_str);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            memory_manager.free(sprite_str);
        }
    }

    // Cells array must exist and match width*height
    cJSON *cells = cJSON_GetObjectItem(json, "cells");
    if (!cells || !cJSON_IsArray(cells)) {
        log_error("ASSET_MANAGER", "Map JSON missing or invalid 'cells' array: %s", filename);
        ese_tileset_destroy(tileset);
        cJSON_Delete(json);
        return false;
    }
    int cell_count = cJSON_GetArraySize(cells);
    if (cell_count != width * height) {
        log_error("ASSET_MANAGER", "Cells length (%d) != width*height (%d) in %s",
                  cell_count, width * height, filename);
        ese_tileset_destroy(tileset);
        cJSON_Delete(json);
        return false;
    }

    // Create map (C-only, not Lua-registered)
    EseMap *map = ese_map_create(lua, (uint32_t)width, (uint32_t)height, ese_map_type, false);

    // Set metadata if present
    cJSON *title_item = cJSON_GetObjectItem(json, "title");
    if (title_item && cJSON_IsString(title_item)) {
        ese_map_set_title(map, title_item->valuestring);
    }
    cJSON *author_item = cJSON_GetObjectItem(json, "author");
    if (author_item && cJSON_IsString(author_item)) {
        ese_map_set_author(map, author_item->valuestring);
    }
    cJSON *version_item = cJSON_GetObjectItem(json, "version");
    if (version_item && cJSON_IsNumber(version_item)) {
        ese_map_set_version(map, version_item->valueint);
    }

    // Parse and set each cell directly
    for (int ci = 0; ci < cell_count; ci++) {
        cJSON *cell_obj = cJSON_GetArrayItem(cells, ci);
        if (!cell_obj || !cJSON_IsObject(cell_obj)) {
            log_error("ASSET_MANAGER", "Invalid cell object at index %d in %s", ci, filename);
            ese_map_destroy(map);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        cJSON *layers = cJSON_GetObjectItem(cell_obj, "layers");
        if (!layers || !cJSON_IsArray(layers)) {
            log_error("ASSET_MANAGER", "Cell %d missing 'layers' array in %s", ci, filename);
            ese_map_destroy(map);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        uint32_t x = (uint32_t)(ci % width);
        uint32_t y = (uint32_t)(ci / width);
        EseMapCell *dst = ese_map_get_cell(map, x, y);
        if (!dst) {
            log_error("ASSET_MANAGER", "Invalid cell coords (%u,%u) in %s", x, y, filename);
            ese_map_destroy(map);
            ese_tileset_destroy(tileset);
            cJSON_Delete(json);
            return false;
        }

        int layer_count = cJSON_GetArraySize(layers);
        for (int li = 0; li < layer_count; li++) {
            cJSON *lid_item = cJSON_GetArrayItem(layers, li);
            if (!lid_item || !cJSON_IsNumber(lid_item)) {
                log_error("ASSET_MANAGER", "Invalid layer id at cell %d layer %d in %s",
                          ci, li, filename);
                ese_map_destroy(map);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            long lid = lid_item->valueint;
            if (lid == -1) {
                if (!ese_mapcell_add_layer(dst, -1)) {
                    log_error(
                        "ASSET_MANAGER",
                        "Failed to add layer for blank tile at cell %d in %s",
                        ci, filename
                    );
                    ese_map_destroy(map);
                    ese_tileset_destroy(tileset);
                    cJSON_Delete(json);
                    return false;
                }
                continue;
            }

            if (lid < 0 || lid > 255) {
                log_error("ASSET_MANAGER", "Out of range tile id %ld at cell %d in %s",
                          lid, ci, filename);
                ese_map_destroy(map);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
            int tile_id = (int)lid;

            if (ese_tileset_get_sprite_count(tileset, tile_id) == 0) {
                log_error("ASSET_MANAGER",
                          "Tile id %d used in cells but not defined in tileset for %s",
                          tile_id, filename);
                ese_map_destroy(map);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }

            if (!ese_mapcell_add_layer(dst, tile_id)) {
                log_error("ASSET_MANAGER", "Failed to add layer for tile %d at cell %d in %s",
                          tile_id, ci, filename);
                ese_map_destroy(map);
                ese_tileset_destroy(tileset);
                cJSON_Delete(json);
                return false;
            }
        }

        // Optional flags
        cJSON *flags_item = cJSON_GetObjectItem(cell_obj, "flags");
        if (flags_item && cJSON_IsNumber(flags_item)) {
            ese_mapcell_set_flags(dst, (uint32_t)flags_item->valueint);
        }

        // Optional isDynamic
        cJSON *dyn_item = cJSON_GetObjectItem(cell_obj, "isDynamic");
        if (dyn_item) {
            if (dyn_item->type == cJSON_True) ese_mapcell_set_is_dynamic(dst, true);
            else if (dyn_item->type == cJSON_False) ese_mapcell_set_is_dynamic(dst, false);
            else if (cJSON_IsNumber(dyn_item)) ese_mapcell_set_is_dynamic(dst, (dyn_item->valueint != 0));
        }
    }

    // Attach tileset to map
    ese_map_set_tileset(map, tileset);

    // Register group and store asset
    _asset_manager_add_group(manager, group);
    EseAsset *asset = _asset_create();
    asset->type = ASSET_MAP;
    asset->data = (void *)map;
    grouped_hashmap_set(manager->maps, group, filename, asset);

    log_debug("ASSET_MANAGER", "Added map '%s'.", filename);
    cJSON_Delete(json);
    return true;
}

EseMap *asset_manager_get_map(
    EseAssetManager *manager,
    const char *asset_id)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_get_map called with NULL manager");
    log_assert("ASSET_MANAGER", asset_id, "asset_manager_get_map called with NULL asset_id");

    char *out_group = NULL;
    char *out_name = NULL;
    _split_string(asset_id, &out_group, &out_name);

    EseAsset *asset = (EseAsset *)grouped_hashmap_get(manager->maps, out_group, out_name);
    memory_manager.free(out_group);
    memory_manager.free(out_name);

    if (!asset) {
        return NULL;
    }

    return (EseMap *)asset->data;
}

bool asset_manager_create_font_atlas(
    EseAssetManager *manager,
    const char *name,
    const unsigned char *font_data,
    int total_chars,
    int char_width,
    int char_height)
{
    log_assert("ASSET_MANAGER", manager, "asset_manager_create_font_atlas called with NULL manager");
    log_assert("ASSET_MANAGER", name, "asset_manager_create_font_atlas called with NULL name");
    log_assert("ASSET_MANAGER", font_data, "asset_manager_create_font_atlas called with NULL font_data");

    // Use the passed parameters for character dimensions
    int chars_per_row = 16;  // 16 characters per row in the font bitmap
    
    // Calculate the total dimensions needed for the atlas
    int atlas_width = chars_per_row * char_width;
    int atlas_height = (total_chars / chars_per_row) * char_height;
    
    // Allocate RGBA buffer (4 bytes per pixel)
    unsigned char *rgba_data = memory_manager.malloc(atlas_width * atlas_height * 4, MMTAG_ASSET);
    if (!rgba_data) {
        log_error("ASSET_MANAGER", "Failed to allocate RGBA buffer for font atlas");
        return false;
    }
    
    // Convert font bitmap to RGBA
    for (int char_y = 0; char_y < (total_chars / chars_per_row); char_y++) {
        for (int char_x = 0; char_x < chars_per_row; char_x++) {
            int char_index = char_y * chars_per_row + char_x;
            
            // Calculate position in the atlas
            int atlas_x = char_x * char_width;
            int atlas_y = char_y * char_height;
            
            // Get character data from font bitmap
            const unsigned char *char_data = font_data + (char_index * char_height * 2); // 2 bytes per row
            
            // Convert each pixel of the character
            for (int y = 0; y < char_height; y++) {
                for (int x = 0; x < char_width; x++) {
                    int atlas_pixel_x = atlas_x + x;
                    int atlas_pixel_y = atlas_y + y;
                    int atlas_pixel_index = (atlas_pixel_y * atlas_width + atlas_pixel_x) * 4;
                    
                    // Get bit from font data (10 bits per row, stored in 2 bytes)
                    int byte_index = x / 8;
                    int bit_index = 7 - (x % 8); // MSB first
                    unsigned char byte = char_data[y * 2 + byte_index];
                    bool pixel_on = (byte & (1 << bit_index)) != 0;
                    
                    // Set RGBA values (white text on transparent background)
                    rgba_data[atlas_pixel_index + 0] = pixel_on ? 255 : 0; // R
                    rgba_data[atlas_pixel_index + 1] = pixel_on ? 255 : 0; // G
                    rgba_data[atlas_pixel_index + 2] = pixel_on ? 255 : 0; // B
                    rgba_data[atlas_pixel_index + 3] = pixel_on ? 255 : 0; // A
                }
            }
        }
    }
    
    // Load the texture using the renderer
    if (!renderer_load_texture(manager->renderer, name, rgba_data, atlas_width, atlas_height)) {
        log_error("ASSET_MANAGER", "Failed to load font atlas texture");
        memory_manager.free(rgba_data);
        return false;
    }
    
    // Use the name as the texture ID for sprites
    const char *texture_id = name;
    
    // Create texture asset and add to manager
    _asset_manager_add_group(manager, "fonts");
    EseAsset *texture_asset = _asset_create();
    texture_asset->type = ASSET_TEXTURE;
    
    EseAssetTexture *texture_data = memory_manager.malloc(sizeof(EseAssetTexture), MMTAG_ASSET);
    texture_data->width = atlas_width;
    texture_data->height = atlas_height;
    texture_asset->data = texture_data;
    
    grouped_hashmap_set(manager->textures, "fonts", name, texture_asset);
    
    // Create sprites for each glyph
    for (int char_y = 0; char_y < (total_chars / chars_per_row); char_y++) {
        for (int char_x = 0; char_x < chars_per_row; char_x++) {
            int char_index = char_y * chars_per_row + char_x;
            
            // Create sprite name: "name_###" where ### is the ASCII code
            char sprite_name[64];
            snprintf(sprite_name, sizeof(sprite_name), "%s_%03d", name, char_index);
            
            // Calculate UV coordinates for this character
            float u1 = (float)(char_x * char_width) / atlas_width;
            float v1 = (float)(char_y * char_height) / atlas_height;
            float u2 = (float)((char_x + 1) * char_width) / atlas_width;
            float v2 = (float)((char_y + 1) * char_height) / atlas_height;
            
            // Create sprite
            EseSprite *sprite = sprite_create();
            if (sprite) {
                // Add the frame to the sprite
                sprite_add_frame(sprite, texture_id, u1, v1, u2, v2, char_width, char_height);
                
                EseAsset *sprite_asset = _asset_create();
                sprite_asset->type = ASSET_SPRITE;
                sprite_asset->data = sprite;
                grouped_hashmap_set(manager->sprites, "fonts", sprite_name, sprite_asset);
            }
        }
    }
    
    // Free the RGBA buffer
    memory_manager.free(rgba_data);
    
    log_debug("ASSET_MANAGER", "Created font atlas '%s' with %d glyphs", name, total_chars);
    return true;
}

void asset_manager_remove_group(
    EseAssetManager *manager,
    const char *group)
{
    if (!manager) {
        log_error("ASSET_MANAGER", "Error: asset_manager_remove_group called with NULL manager");
        return;
    }

    if (!group) {
        log_error("ASSET_MANAGER", "Error: asset_manager_remove_group called with NULL group");
        return;
    }

    grouped_hashmap_remove_group(manager->sprites, group);
    grouped_hashmap_remove_group(manager->textures, group);
    grouped_hashmap_remove_group(manager->atlases, group);
    grouped_hashmap_remove_group(manager->maps, group);

    _asset_manager_remove_group(manager, group);
}
