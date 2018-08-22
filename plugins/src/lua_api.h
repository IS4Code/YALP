#ifndef LUA_API_H_INCLUDED
#define LUA_API_H_INCLUDED

#include "main.h"
#include "lua/lualibs.h"

namespace lua
{
	void init(lua_State *L, int load, int preload);
	void report_error(lua_State *L, int error);
}

#endif
