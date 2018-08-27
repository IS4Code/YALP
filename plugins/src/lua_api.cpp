#include "lua_api.h"
#include "lua_utils.h"
#include "lua/timer.h"
#include "lua/interop.h"
#include "lua/remote.h"
#include "main.h"
#include <vector>

static int custom_print(lua_State *L)
{
	int n = lua_gettop(L);
	bool hastostring = lua_getglobal(L, "tostring") == LUA_TFUNCTION;
	int tostring = lua_absindex(L, -1);
	if(!hastostring) lua_pop(L, 1);
	
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	for(int i = 1; i <= n; i++)
	{
		if(i > 1) luaL_addlstring(&buf, "\t", 1);
		if(!hastostring)
		{
			if(lua_isstring(L, i))
			{
				lua_pushvalue(L, i);
				luaL_addvalue(&buf);
			}else{
				luaL_addstring(&buf, luaL_typename(L, i));
			}
		}else{
			lua_pushvalue(L, tostring);
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			if(!lua_isstring(L, -1))
			{
				return luaL_error(L, "'tostring' must return a string to 'print'");
			}
			luaL_addvalue(&buf);
		}
	}
	luaL_pushresult(&buf);
	logprintf("%s", lua_tostring(L, -1));
	return 0;
}

int take(lua_State *L)
{
	int numrets = (int)luaL_checkinteger(L, 1);
	int numargs = lua_gettop(L) - 2;
	lua_call(L, numargs, numrets);
	return numrets;
}

int bind(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushinteger(L, lua_gettop(L));
	lua_insert(L, 1);
	lua_pushcclosure(L, [](lua_State *L)
	{
		int num = lua_tointeger(L, lua_upvalueindex(1));
		for(int i = 0; i < num; i++)
		{
			lua_pushvalue(L, lua_upvalueindex(2 + i));
		}
		lua_rotate(L, 1, num);
		lua_call(L, lua_gettop(L) - 1, lua::numresults(L));
		return lua_gettop(L);
	}, lua_gettop(L));
	return 1;
}

int clear(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushnil(L);
	while(lua_next(L, 1))
	{
		lua_pop(L, 1);
		lua_pushvalue(L, -1);
		lua_pushnil(L);
		lua_settable(L, 1);
	}
	return 0;
}

int open_base(lua_State *L)
{
	luaopen_base(L);
	lua_pushcfunction(L, custom_print);
	lua_setfield(L, -2, "print");
	lua_pushcfunction(L, take);
	lua_setfield(L, -2, "take");
	lua_pushcfunction(L, bind);
	lua_setfield(L, -2, "bind");
	return 1;
}

int open_table(lua_State *L)
{
	luaopen_table(L);
	lua_pushcfunction(L, clear);
	lua_setfield(L, -2, "clear");
	return 1;
}

std::vector<std::pair<const char*, lua_CFunction>> libs = {
	{"_G", open_base},
	{LUA_LOADLIBNAME, luaopen_package},
	{LUA_COLIBNAME, luaopen_coroutine},
	{LUA_TABLIBNAME, open_table},
	{LUA_IOLIBNAME, luaopen_io},
	{LUA_OSLIBNAME, luaopen_os},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_MATHLIBNAME, luaopen_math},
	{LUA_UTF8LIBNAME, luaopen_utf8},
	{LUA_DBLIBNAME, luaopen_debug},
	{"interop", lua::interop::loader},
	{"timer", lua::timer::loader},
	{"remote", lua::remote::loader},
};

void lua::init(lua_State *L, int load, int preload)
{
	for(size_t i = 0; i < libs.size(); i++)
	{
		if(load & (1 << i))
		{
			const auto &lib = libs[i];
			luaL_requiref(L, lib.first, lib.second, 1);
			lua_pop(L, 1);
		}
	}

	if(lua_getglobal(L, "package") == LUA_TTABLE)
	{
		preload &= ~load;
		if(lua_getfield(L, -1, "preload") == LUA_TTABLE)
		{
			for(size_t i = 0; i < libs.size(); i++)
			{
				if(preload & (1 << i))
				{
					const auto &lib = libs[i];
					lua_pushcfunction(L, lib.second);
					lua_setfield(L, -2, lib.first);
				}
			}
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

void lua::report_error(lua_State *L, int error)
{
	const char *msg = lua_tostring(L, -1);
	bool pop = false;
	if(!msg)
	{
		if(lua_getglobal(L, "tostring") == LUA_TFUNCTION)
		{
			lua_pushvalue(L, -2);
			lua_call(L, 1, 1);
			msg = lua_tostring(L, -1);
			pop = true;
		}else{
			msg = luaL_typename(L, -1);
		}
	}
	logprintf("unhandled Lua error %d: %s", error, msg);
	if(pop)
	{
		lua_pop(L, 1);
	}
}
