#ifndef ESE_GUI_STYLE_LUA_H
#define ESE_GUI_STYLE_LUA_H

typedef struct EseLuaEngine EseLuaEngine;

void _ese_gui_style_lua_remove_from_registry(EseGuiStyle *style);

void _ese_gui_style_lua_init(EseLuaEngine *engine);

#endif // ESE_GUI_STYLE_LUA_H
