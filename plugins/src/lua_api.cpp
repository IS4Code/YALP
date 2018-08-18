#include "lua_api.h"
#include "lua/timer.h"
#include "lua/interop.h"

void lua::init(lua_State *L)
{
	luaL_openlibs(L);
	if(lua_getglobal(L, "package") == LUA_TTABLE)
	{
		lua_getfield(L, -1, "preload");

		lua_pushcfunction(L, lua::timer::loader);
		lua_setfield(L, -2, "timer");

		lua_pushcfunction(L, lua::interop::loader);
		lua_setfield(L, -2, "interop");

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}
