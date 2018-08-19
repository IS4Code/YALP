#include "lua_api.h"
#include "lua/timer.h"
#include "lua/interop.h"

static int custom_print(lua_State *L)
{
	int n = lua_gettop(L);
	lua_getglobal(L, "tostring");
	int tostring = lua_absindex(L, -1);
	
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	for(int i = 1; i <= n; i++)
	{
		lua_pushvalue(L, tostring);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);
		if(!lua_isstring(L, -1))
		{
			return luaL_error(L, "'tostring' must return a string to 'print'");
		}
		if(i > 1) luaL_addlstring(&buf, "\t", 1);
		luaL_addvalue(&buf);
	}
	luaL_pushresult(&buf);
	logprintf("%s", lua_tostring(L, -1));
	return 0;
}

void lua::init(lua_State *L)
{
	luaL_openlibs(L);
	lua_register(L, "print", custom_print);
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
