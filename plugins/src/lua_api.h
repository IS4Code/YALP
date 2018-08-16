#ifndef LUA_API_H_INCLUDED
#define LUA_API_H_INCLUDED

#include "main.h"
#include "lua/lua.hpp"

namespace lua
{
	void init(lua_State *L);
	bool getpublic(lua_State *L, const char *name);
	bool getpublictable(lua_State *L);
}

#endif
