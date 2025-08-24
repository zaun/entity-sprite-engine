// asset_manager.h
#ifndef ESE_ASSET_MANAGER_H
#define ESE_ASSET_MANAGER_H

#include <stdbool.h>

// Forward declarations
typedef struct EseAssetManager EseAssetManager;
typedef struct EseRenderer EseRenderer;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;
typedef struct EseMap EseMap;

// EseAsset Manager
EseAssetManager* asset_manager_create(EseRenderer* renderer);
void asset_manager_destroy(EseAssetManager* manager);

// EseAsset File Loaders
bool asset_manager_load_sprite_atlas(EseAssetManager* manager, const char* filename, const char* group, bool indexed);
bool asset_manager_load_map(EseAssetManager* manager, EseLuaEngine *lua, const char* filename, const char* group);

// EseAsset Creation
bool asset_manager_create_font_atlas(EseAssetManager* manager, const char* name, const unsigned char* font_data, int total_chars, int char_width, int char_height);

// EseAsset Retrieval
EseSprite* asset_manager_get_sprite(EseAssetManager* manager, const char* asset_id);
void asset_manager_get_texture_size(EseAssetManager* manager, const char* asset_id, int **out_width, int **out_height);
EseMap* asset_manager_get_map(EseAssetManager* manager, const char* asset_id);

// EseAsset Manager Management
void asset_manager_remove_group(EseAssetManager *manager, const char *group);

#endif // ESE_ASSET_MANAGER_H
