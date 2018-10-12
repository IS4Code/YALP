#include "string.h"
#include "lua_utils.h"
#include "amxutils.h"

int getstring(lua_State *L)
{
	size_t blen;
	bool isconst;
	auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, blen, isconst));
	if(!ptr)
	{
		return lua::argerrortype(L, 1, "buffer type");
	}
	ptrdiff_t offset = lua::checkoffset(L, 2);
	lua_Integer len;
	if(lua_isnil(L, 3) || lua_isnone(L, 3))
	{
		len = -1;
	}else{
		len = luaL_checkinteger(L, 3);
	}

	if(offset < 0 || (size_t)offset >= blen)
	{
		lua_pushnil(L);
		return 1;
	}

	ptr += offset;
	blen -= offset;

	auto buf = reinterpret_cast<cell*>(ptr);
	blen /= sizeof(cell);
	if(len >= 0 && len < blen)
	{
		blen = (size_t)len;
	}


	std::string str = amx::GetString(buf, blen, true);
	lua_pushlstring(L, str.data(), str.size());
	return 1;
}

int setstring(lua_State *L)
{
	size_t blen;
	bool isconst;
	auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, blen, isconst));
	if(!ptr)
	{
		return lua::argerrortype(L, 1, "buffer type");
	}
	size_t slen;
	auto str = luaL_checklstring(L, 2, &slen);
	ptrdiff_t offset = lua::checkoffset(L, 3);
	lua_Integer len;
	if(lua_isnil(L, 4) || lua_isnone(L, 4))
	{
		len = -1;
	}else{
		len = luaL_checkinteger(L, 4);
	}
	
	bool pack = luaL_opt(L, lua::checkboolean, 5, true);

	if(offset < 0 || (size_t)offset >= blen)
	{
		return 0;
	}

	ptr += offset;
	blen -= offset;

	auto buf = reinterpret_cast<cell*>(ptr);
	blen /= sizeof(cell);
	if(len >= 0 && len < blen)
	{
		blen = (size_t)len;
	}

	if(pack)
	{
		blen *= sizeof(cell);
	}
	if(slen >= blen)
	{
		slen = blen - 1;
	}

	amx::SetString(buf, str, slen, pack);
	return 0;
}

int asstring(lua_State *L)
{
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto ptr = lua::checklightudata(L, 1);
	cell *addr;
	int len;
	if(amx_GetAddr(amx, reinterpret_cast<cell>(ptr), &addr) != AMX_ERR_NONE || amx_StrLen(addr, &len) != AMX_ERR_NONE)
	{
		lua_pushnil(L);
		return 1;
	}

	std::string str = amx::GetString(addr, (size_t)len, len < 0 ? true : false);
	lua_pushlstring(L, str.data(), str.size());
	return 1;
}

int tocellstring(lua_State *L)
{
	size_t length;
	const char *str = luaL_checklstring(L, 1, &length);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushinteger(L, length + 1);
	lua_pushboolean(L, false);
	lua_call(L, 2, 1);
	auto ptr = reinterpret_cast<cell*>(lua_touserdata(L, -1));
	amx::SetString(ptr, str, length, false);
	return 1;
}

void lua::interop::init_string(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	lua_pushcfunction(L, getstring);
	lua_setfield(L, table, "getstring");

	lua_pushcfunction(L, setstring);
	lua_setfield(L, table, "setstring");

	lua_pushlightuserdata(L, amx);
	lua_pushcclosure(L, asstring, 1);
	lua_setfield(L, table, "asstring");

	lua_getfield(L, table, "newbuffer");
	lua_pushcclosure(L, tocellstring, 1);
	lua_setfield(L, table, "tocellstring");
}
