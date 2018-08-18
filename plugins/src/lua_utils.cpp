#include "lua_utils.h"
#include "amxutils.h"
#include "sdk/amx/amx.h"
#include <string>

int lua::amx_error(lua_State *L, int error)
{
	return luaL_error(L, "internal AMX error %d: %s", error, amx::StrError(error));
}

void *lua::testudata(lua_State *L, int ud, int mt)
{
	void *p = lua_touserdata(L, ud);
	if(p)
	{
		mt = lua_absindex(L, mt);
		if(lua_getmetatable(L, ud))
		{
			if(!lua_rawequal(L, -1, mt)) p = nullptr;
			lua_pop(L, 1);
			return p;
		}
	}
	return nullptr;
}

void *lua::tobuffer(lua_State *L, int idx, size_t &length, bool &isconst)
{
	idx = lua_absindex(L, idx);
	if(lua_getmetatable(L, idx))
	{
		if(lua_getfield(L, -1, "__buf") != LUA_TNIL)
		{
			lua_pushvalue(L, idx);
			lua_call(L, 1, 3);
			void *ptr = lua_touserdata(L, -3);
			if(ptr)
			{
				length = (int)luaL_checkinteger(L, -2);
				isconst = !lua_toboolean(L, -1);
			}
			lua_pop(L, 4);
			return ptr;
		}
		lua_pop(L, 2);
	}
	return nullptr;
}

size_t lua::checkoffset(lua_State *L, int idx)
{
	if(lua_isinteger(L, idx))
	{
		return (size_t)(lua_tointeger(L, idx) - 1) * sizeof(cell);
	}else if(lua_islightuserdata(L, idx))
	{
		return reinterpret_cast<size_t>(lua_touserdata(L, idx));
	}else if(lua_isnil(L, idx) || lua_isnone(L, idx))
	{
		return 0;
	}else{
		return (size_t)luaL_argerror(L, idx, "type not expected");
	}
}

void *lua::checklightudata(lua_State *L, int idx)
{
	if(lua_islightuserdata(L, idx))
	{
		return lua_touserdata(L, idx);
	}
	std::string errmsg("light userdata expected, got ");
	errmsg.append(luaL_typename(L, idx));
	luaL_argerror(L, idx, errmsg.c_str());
	return nullptr;
}
