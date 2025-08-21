// asset_manager.h
#ifndef ESE_ASSET_MANAGER_H
#define ESE_ASSET_MANAGER_H

#include <stdbool.h>

// Forward declarations
typedef struct EseAssetManager EseAssetManager;
typedef struct EseRenderer EseRenderer;
typedef struct EseLuaEngine EseLuaEngine;
typedef struct EseSprite EseSprite;

// EseAsset Manager
EseAssetManager* asset_manager_create(EseRenderer* renderer);
void asset_manager_destroy(EseAssetManager* manager);

// EseAsset File Loaders
bool asset_manager_load_sprite_atlas(EseAssetManager* manager, const char* filename, const char* group);
bool asset_manager_load_map(EseAssetManager* manager, EseLuaEngine *lua, const char* filename, const char* group);

// EseAsset Retrieval
EseSprite* asset_manager_get_sprite(EseAssetManager* manager, const char* asset_id);
void asset_manager_get_texture_size(EseAssetManager* manager, const char* asset_id, int **out_width, int **out_height);

// EseAsset Manager Management
void asset_manager_remove_group(EseAssetManager *manager, const char *group);

#endif // ESE_ASSET_MANAGER_H
