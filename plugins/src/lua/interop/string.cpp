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
		return luaL_argerror(L, 1, "must be a buffer type");
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


	std::string str = amx::GetString(buf, blen);
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
		return luaL_argerror(L, 1, "must be a buffer type");
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
