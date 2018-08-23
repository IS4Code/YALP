#include "sleep.h"
#include "lua_utils.h"
#include "amxutils.h"

int sleep(lua_State *L)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);
	cell value;
	if(lua_islightuserdata(L, 1))
	{
		return lua::amx_sleep(L, 1, 2);
	}else if(lua_isinteger(L, 1))
	{
		value = (cell)lua_tointeger(L, 1);
	}else if(lua_isnumber(L, 1))
	{
		float num = (float)lua_tonumber(L, 1);
		value = amx_ftoc(num);
	}else if(lua_isboolean(L, 1))
	{
		value = (cell)lua_toboolean(L, 1);
	}else{
		if(lua_isfunction(L, 1))
		{
			lua_pushvalue(L, 1);
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
								lua_pushvalue(L, 2);
								lua_setfield(L, -3, "__cont");
							}
						}
						lua_pop(L, 1);
					}
					return lua_error(L);
			}
		}
		return luaL_argerror(L, 1, "type not expected");
	}
	lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
	return lua::amx_sleep(L, -1, 2);
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
			amx->cip = luaL_ref(L, -2);
		}else{
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}
