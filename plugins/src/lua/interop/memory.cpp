#include "memory.h"
#include "lua_utils.h"

#include <vector>
#include <cstring>

int newbuffer(lua_State *L)
{
	auto size = (size_t)luaL_checkinteger(L, 1) * sizeof(cell);
	auto buf = lua_newuserdata(L, size);
	std::memset(buf, 0, size);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

int tobuffer(lua_State *L)
{
	size_t len;
	bool isconst;
	if(lua_isinteger(L, 1))
	{
		lua::pushuserdata(L, (cell)lua_tointeger(L, 1));
	}else if(lua_isnumber(L, 1))
	{
		float num = (float)lua_tonumber(L, 1);
		lua::pushuserdata(L, amx_ftoc(num));
	}else if(lua_isboolean(L, 1))
	{
		lua::pushuserdata(L, (cell)lua_toboolean(L, 1));
	}else if(lua_isstring(L, 1))
	{
		size_t len;
		auto str = lua_tolstring(L, 1, &len);
		len++;

		auto addr = reinterpret_cast<cell*>(lua_newuserdata(L, len * sizeof(cell)));
		for(size_t i = 0; i < len; i++)
		{
			addr[i] = str[i];
		}
	}else if(auto src = lua::tobuffer(L, 1, len, isconst))
	{
		void *dst = lua_newuserdata(L, len);
		std::memcpy(dst, src, len);
	}else if(lua_isnil(L, 1))
	{
		lua::pushuserdata(L, (cell)0);
	}else{
		luaL_argerror(L, 1, "type not expected");
		return 0;
	}
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

int buffer_len(lua_State *L)
{
	size_t len;
	bool isconst;
	if(lua::tobuffer(L, 1, len, isconst))
	{
		lua_pushinteger(L, len / sizeof(cell));
		return 1;
	}
	return 0;
}

int buffer_index(lua_State *L)
{
	size_t index = lua::checkoffset(L, 2);
	size_t len;
	bool isconst;
	if(auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, len, isconst)))
	{
		if(index >= 0 && index <= len - sizeof(cell))
		{
			lua_pushinteger(L, *reinterpret_cast<cell*>(ptr + index));
			return 1;
		}
		lua_pushnil(L);
		return 1;
	}
	return 0;
}

int buffer_newindex(lua_State *L)
{
	size_t index = lua::checkoffset(L, 2);
	auto value = luaL_checkinteger(L, 3);
	size_t len;
	bool isconst;
	if(auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, len, isconst)))
	{
		if(isconst)
		{
			return luaL_error(L, "buffer is read-only");
		}
		if(index >= 0 && index <= len - sizeof(cell))
		{
			*reinterpret_cast<cell*>(ptr + index) = (cell)value;
		}
	}
	return 0;
}

int buffer_buf(lua_State *L)
{
	lua_pushlightuserdata(L, lua_touserdata(L, 1));
	lua_pushinteger(L, lua_rawlen(L, 1));
	lua_pushvalue(L, lua_upvalueindex(1));
	return 3;
}

int heap_buf(lua_State *L)
{
	auto amx = lua::touserdata<AMX*>(L, 1);
	auto hdr = (AMX_HEADER*)amx->base;
	auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
	lua_pushlightuserdata(L, data + amx->hlw);
	lua_pushinteger(L, amx->hea - amx->hlw);
	lua_pushboolean(L, true);
	return 3;
}

int span_buf(lua_State *L)
{
	if(lua_getmetatable(L, 1))
	{
		int mt = lua_absindex(L, -1);

		size_t len;
		bool isconst;
		lua_getfield(L, mt, "__base");
		if(auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, -1, len, isconst)))
		{
			lua_pop(L, 1);

			lua_getfield(L, mt, "__ofs");
			auto ofs = lua_tointeger(L, -1);
			lua_pop(L, 1);
			ptr += ofs;
			len -= (size_t)ofs;

			lua_getfield(L, mt, "__sublen");
			int islen;
			auto sublen = lua_tointegerx(L, -1, &islen);
			lua_pop(L, 1);
			if(islen && sublen >= 0 && sublen < len)
			{
				len = (size_t)sublen;
			}

			lua_getfield(L, mt, "__const");
			if(lua_toboolean(L, -1)) isconst = true;
			lua_pop(L, 1);

			lua_pop(L, 1);

			lua_pushlightuserdata(L, ptr);
			lua_pushinteger(L, len);
			lua_pushboolean(L, !isconst);
			return 3;
		}
		lua_pop(L, 2);
	}
	return 0;
}

int span(lua_State *L)
{
	size_t blen;
	bool isconst;
	if(!lua::tobuffer(L, 1, blen, isconst))
	{
		return luaL_argerror(L, 1, "must be a buffer type");
	}
	size_t offset = lua::checkoffset(L, 2);
	lua_Integer len;
	if(lua_isnil(L, 3) || lua_isnone(L, 3))
	{
		len = -1;
	}else{
		len = luaL_checkinteger(L, 3) * sizeof(cell);
	}
	isconst = !!lua_toboolean(L, 4);

	if(offset < 0)
	{
		len += offset;
		offset = 0;
	}

	lua_newtable(L);

	lua_createtable(L, 0, 10);
	lua_pushstring(L, "span");
	lua_setfield(L, -2, "__name");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "__metatable");
	lua_pushcfunction(L, buffer_len);
	lua_setfield(L, -2, "__len");
	lua_pushcfunction(L, buffer_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, buffer_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, span_buf);
	lua_setfield(L, -2, "__buf");

	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "__base");
	lua_pushinteger(L, offset);
	lua_setfield(L, -2, "__ofs");
	if(len >= 0)
	{
		lua_pushinteger(L, len);
		lua_setfield(L, -2, "__sublen");
	}
	if(isconst)
	{
		lua_pushboolean(L, true);
		lua_setfield(L, -2, "__const");
	}
	lua_setmetatable(L, -2);
	return 1;
}

void lua::interop::init_memory(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	lua_createtable(L, 0, 7);
	lua_pushstring(L, "buffer");
	lua_setfield(L, -2, "__name");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "__metatable");
	lua_pushcfunction(L, buffer_len);
	lua_setfield(L, -2, "__len");
	lua_pushcfunction(L, buffer_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, buffer_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushboolean(L, true);
	lua_pushcclosure(L, buffer_buf, 1);
	lua_setfield(L, -2, "__buf");

	int buffer = lua_absindex(L, -1);

	lua_createtable(L, 0, 7);
	lua_pushstring(L, "cbuffer");
	lua_setfield(L, -2, "__name");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "__metatable");
	lua_pushcfunction(L, buffer_len);
	lua_setfield(L, -2, "__len");
	lua_pushcfunction(L, buffer_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, buffer_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushboolean(L, false);
	lua_pushcclosure(L, buffer_buf, 1);
	lua_setfield(L, -2, "__buf");

	int cbuffer = lua_absindex(L, -1);

	lua_pushvalue(L, buffer);
	lua_pushcclosure(L, newbuffer, 1);
	lua_setfield(L, table, "newbuffer");

	lua_pushvalue(L, buffer);
	lua_pushcclosure(L, ::tobuffer, 1);
	lua_setfield(L, table, "tobuffer");
	
	lua_pushvalue(L, cbuffer);
	lua_pushcclosure(L, ::tobuffer, 2);
	lua_setfield(L, table, "tocbuffer");

	lua::pushuserdata(L, amx);

	lua_createtable(L, 0, 6);
	lua_pushstring(L, "heap");
	lua_setfield(L, -2, "__name");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "__metatable");
	lua_pushcfunction(L, buffer_len);
	lua_setfield(L, -2, "__len");
	lua_pushcfunction(L, buffer_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, buffer_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, heap_buf);
	lua_setfield(L, -2, "__buf");

	lua_setmetatable(L, -2);
	lua_setfield(L, table, "heap");


	lua_pushcfunction(L, span);
	lua_setfield(L, table, "span");

	lua_settop(L, table);
}
