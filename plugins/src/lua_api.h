#ifndef LUA_API_H_INCLUDED
#define LUA_API_H_INCLUDED

#include "main.h"
#include "lua/lualibs.h"

namespace lua
{
	void initlibs(lua_State *L, int load, int preload);
	cell init_bind(lua_State *L, AMX *amx);
	int bind(AMX *amx, cell *retval, int index);
	void report_error(lua_State *L, int error);
	AMX *bound_amx(lua_State *L);
}

#endif
