#include "string.h"
#include "lua_utils.h"

int getstring(lua_State *L)
{
	size_t blen;
	bool isconst;
	auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, blen, isconst));
	if(!ptr)
	{
		return luaL_argerror(L, 1, "must be a buffer type");
	}
	size_t offset = lua::checkoffset(L, 2);
	lua_Integer len;
	if(lua_isnil(L, 3) || lua_isnone(L, 3))
	{
		len = -1;
	}else{
		len = luaL_checkinteger(L, 3);
	}

	ptr += offset;
	blen -= offset;

	auto buf = reinterpret_cast<cell*>(ptr);
	blen /= sizeof(cell);
	if(len >= 0 && len < blen)
	{
		blen = (size_t)len;
	}

	std::string str;
	for(size_t i = 0; i < blen; i++)
	{
		cell c = buf[i];
		if(c == 0)
		{
			blen = i;
			break;
		}
		str.push_back((char)c);
	}
	lua_pushlstring(L, str.data(), blen);
	lua_pushinteger(L, blen);
	return 2;
}

int setstring(lua_State *L)
{
	size_t blen;
	bool isconst;
	auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, blen, isconst));
	if(!ptr)
	{
		return luaL_argerror(L, 1, "must be a buffer type");
	}
	auto str = luaL_checkstring(L, 2);
	size_t offset = lua::checkoffset(L, 3);
	lua_Integer len;
	if(lua_isnil(L, 4) || lua_isnone(L, 4))
	{
		len = -1;
	}else{
		len = luaL_checkinteger(L, 4);
	}

	ptr += offset;
	blen -= offset;

	auto buf = reinterpret_cast<cell*>(ptr);
	blen /= sizeof(cell);
	if(len >= 0 && len < blen)
	{
		blen = (size_t)len;
	}

	for(size_t i = 0; i < blen; i++)
	{
		buf[i] = str[i];
	}
	return 0;
}

int asstring(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, lua_upvalueindex(2));
	lua_rotate(L, 1, 2);
	return lua::tailcall(L, lua_gettop(L) - 1);
}

void lua::interop::init_string(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	lua_pushcfunction(L, getstring);
	lua_setfield(L, table, "getstring");

	lua_pushcfunction(L, setstring);
	lua_setfield(L, table, "setstring");

	lua_getfield(L, table, "getstring");
	lua_getfield(L, table, "heap");
	lua_pushcclosure(L, asstring, 2);
	lua_setfield(L, table, "asstring");
}
