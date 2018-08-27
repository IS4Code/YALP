#include "sleep.h"
#include "lua_utils.h"
#include "amxutils.h"

#include <cstdlib>

int sleep(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	cell value;
	if(lua_islightuserdata(L, 2))
	{
		return lua::amx_sleep(L, 2, 1);
	}else if(lua_isinteger(L, 2))
	{
		value = (cell)lua_tointeger(L, 2);
	}else if(lua_isnumber(L, 2))
	{
		float num = (float)lua_tonumber(L, 2);
		value = amx_ftoc(num);
	}else if(lua_isboolean(L, 2))
	{
		value = (cell)lua_toboolean(L, 2);
	}else{
		if(lua_isfunction(L, 2))
		{
			lua_pushvalue(L, 2);
			switch(lua_pcall(L, 0, 1, 0))
			{
				case LUA_OK:
					return 1;
				default:
					if(lua_istable(L, -1))
					{
						if(lua_getfield(L, -1, "__amxerr") == LUA_TNUMBER)
						{
							if(lua_tointeger(L, -1) == AMX_ERR_SLEEP)
							{
								lua_pushvalue(L, 1);
								lua_setfield(L, -3, "__cont");
							}
						}
						lua_pop(L, 1);
					}
					return lua_error(L);
			}
		}
		return luaL_argerror(L, 2, "type not expected");
	}
	lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
	return lua::amx_sleep(L, -1, 1);
}

void lua::interop::init_sleep(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);
	lua_pushcfunction(L, sleep);
	lua_setfield(L, table, "sleep");
}

void lua::interop::handle_sleep(lua_State *L, AMX *amx, int contlist)
{
	if(lua_getfield(L, -1, "__retval") == LUA_TLIGHTUSERDATA)
	{
		amx->pri = reinterpret_cast<cell>(lua_touserdata(L, -1));
	}
	lua_pop(L, 1);

	if(lua_rawgeti(L, LUA_REGISTRYINDEX, contlist) == LUA_TTABLE)
	{
		if(lua_getfield(L, -2, "__cont") == LUA_TFUNCTION)
		{
			lua_createtable(L, 2, 0);
			lua_insert(L, -2);
			lua_rawseti(L, -2, 1);
			int cookie = std::rand();
			lua_pushinteger(L, cookie);
			lua_rawseti(L, -2, 2);
			amx->alt = cookie;
			amx->cip = luaL_ref(L, -2);
		}else{
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}
