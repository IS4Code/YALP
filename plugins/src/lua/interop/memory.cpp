#include "memory.h"
#include "lua_utils.h"
#include "amxutils.h"

#include <memory>
#include <vector>
#include <cstring>
#include <functional>

int newbuffer(lua_State *L)
{
	auto isize = luaL_checkinteger(L, 1);
	if(isize < 0)
	{
		return luaL_argerror(L, 1, "out of range");
	}
	bool zero = luaL_opt(L, lua::checkboolean, 2, true);
	auto size = (size_t)isize * sizeof(cell);
	auto buf = lua_newuserdata(L, size);
	if(zero)
	{
		std::memset(buf, 0, size);
	}
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

bool toblock(lua_State *L, int idx, const std::function<void*(lua_State*, size_t)> &alloc)
{
	size_t len;
	bool isconst;
	cell value;
	if(lua_isinteger(L, idx))
	{
		value = (cell)lua_tointeger(L, idx);
	}else if(lua::isnumber(L, idx))
	{
		float num = (float)lua_tonumber(L, idx);
		value = amx_ftoc(num);
	}else if(lua_isboolean(L, idx))
	{
		value = (cell)lua_toboolean(L, idx);
	}else if(lua::isstring(L, idx))
	{
		size_t len;
		auto str = lua_tolstring(L, idx, &len);
		auto dlen = ((len + sizeof(cell)) / sizeof(cell)) * sizeof(cell);

		auto addr = reinterpret_cast<cell*>(alloc(L, dlen));
		if(!addr) return false;
		amx::SetString(addr, str, len, true);
		return true;
	}else if(lua_islightuserdata(L, idx))
	{
		value = reinterpret_cast<cell>(lua_touserdata(L, idx));
	}else if(auto src = lua::tobuffer(L, idx, len, isconst))
	{
		auto addr = reinterpret_cast<cell*>(alloc(L, len));
		if(!addr) return false;
		std::memcpy(addr, src, len);
		return true;
	}else{
		return false;
	}
	auto addr = reinterpret_cast<cell*>(alloc(L, 1 * sizeof(cell)));
	if(!addr) return false;
	*addr = value;
	return true;
}

int tobuffer(lua_State *L)
{
	int args = lua_gettop(L);
	auto numr = lua::numresults(L);
	if(numr >= 0 && numr < args) args = numr;
	for(int i = 1; i <= args; i++)
	{
		if(!toblock(L, i, lua_newuserdata))
		{
			return lua::argerrortype(L, i, "simple type");
		}
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_setmetatable(L, -2);
		lua_replace(L, i);
	}
	return args;
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
	ptrdiff_t index = lua::checkoffset(L, 2);
	size_t len;
	bool isconst;
	if(auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, len, isconst)))
	{
		if(index >= 0 && index + sizeof(cell) <= len)
		{
			lua_pushlightuserdata(L, reinterpret_cast<void*>(*reinterpret_cast<cell*>(ptr + index)));
			return 1;
		}
		lua_pushnil(L);
		return 1;
	}
	return 0;
}

int buffer_newindex(lua_State *L)
{
	ptrdiff_t index = lua::checkoffset(L, 2);
	cell value;
	if(lua_isinteger(L, 3))
	{
		value = (cell)lua_tointeger(L, 3);
	}else if(lua::isnumber(L, 3))
	{
		float num = (float)lua_tonumber(L, 3);
		value = amx_ftoc(num);
	}else if(lua_isboolean(L, 3))
	{
		value = (cell)lua_toboolean(L, 3);
	}else if(lua_islightuserdata(L, 3))
	{
		value = reinterpret_cast<cell>(lua_touserdata(L, 3));
	}else{
		return lua::argerrortype(L, 3, "primitive type");
	}
	size_t len;
	bool isconst;
	if(auto ptr = reinterpret_cast<char*>(lua::tobuffer(L, 1, len, isconst)))
	{
		if(isconst)
		{
			return luaL_error(L, "buffer is read-only");
		}
		if(index >= 0 && index + sizeof(cell) <= len)
		{
			*reinterpret_cast<cell*>(ptr + index) = value;
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
			if(ofs > len)
			{
				ofs = len;
			}
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
		return lua::argerrortype(L, 1, "buffer type");
	}
	ptrdiff_t offset = lua::checkoffset(L, 2);
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
		lua_pushnil(L);
		return 1;
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

int heapalloc(lua_State *L)
{
	auto size = luaL_checkinteger(L, 1) * sizeof(cell);
	if(size < 0)
	{
		return luaL_argerror(L, 1, "out of range");
	}
	bool zero = luaL_opt(L, lua::checkboolean, 2, true);

	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	if(!amx::MemCheck(amx, (size_t)size))
	{
		return lua::amx_error(L, AMX_ERR_MEMORY);
	}
	cell offset = amx->hea;
	amx->hea += (size_t)size;
	auto hdr = (AMX_HEADER*)amx->base;
	auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
	if(zero)
	{
		std::memset(data + offset, 0, (size_t)size);
	}

	if(lua::numresults(L) == 0) return 0;

	lua_pushvalue(L, lua_upvalueindex(2));
	lua_pushvalue(L, lua_upvalueindex(3));
	lua_pushlightuserdata(L, reinterpret_cast<void*>(offset));
	lua_pushinteger(L, size / sizeof(cell));
	lua_call(L, 3, 1);
	return 1;
}

int heapfree(lua_State *L)
{
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto hdr = (AMX_HEADER*)amx->base;
	auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;

	size_t length;
	bool isconst;
	auto buf = reinterpret_cast<unsigned char*>(lua::tobuffer(L, 1, length, isconst));
	if(!buf)
	{
		if(lua_islightuserdata(L, 1))
		{
			buf = data + reinterpret_cast<cell>(lua_touserdata(L, 1));
		}else{
			return lua::argerrortype(L, 1, "buffer type or light userdata");
		}
	}

	if(buf >= data + amx->hlw && buf < data + amx->hea)
	{
		amx->hea = buf - data;
		lua_pushboolean(L, true);
		return 1;
	}
	lua_pushboolean(L, false);
	return 1;
}

int toheap(lua_State *L)
{
	int args = lua_gettop(L);
	for(int i = 1; i <= args; i++)
	{
		if(!toblock(L, i, [](lua_State *L, size_t size)
		{
			lua_pushvalue(L, lua_upvalueindex(1));
			lua_pushinteger(L, size / sizeof(cell));
			lua_call(L, 1, 1);
			bool isconst;
			if(auto buf = lua::tobuffer(L, -1, size, isconst))
			{
				return buf;
			}
			return static_cast<void*>(nullptr);
		}))
		{
			return lua::argerrortype(L, i, "simple type");
		}
		lua_replace(L, i);
	}
	return args;
}

int _struct(lua_State *L)
{
	int args = lua_gettop(L);
	auto numr = lua::numresults(L);
	if(numr >= 0 && numr < args) args = numr;
	std::vector<cell> data;
	std::vector<std::pair<size_t, size_t>> sizes;
	for(int i = 1; i <= args; i++)
	{
		if(!toblock(L, i, [&](lua_State *L, size_t size)
		{
			auto oldsize = data.size();
			data.resize(oldsize + size / sizeof(cell));
			sizes.push_back(std::make_pair(oldsize * sizeof(cell), size / sizeof(cell)));
			return &data[oldsize];
		}))
		{
			return lua::argerrortype(L, i, "simple type");
		}
	}
	lua_settop(L, 0);

	size_t size = data.size() * sizeof(cell);
	void *addr = lua_newuserdata(L, size);
	std::memcpy(addr, &data[0], size);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	int buffer = lua_absindex(L, -1);

	luaL_checkstack(L, args - 1 + 4, nullptr);
	for(const auto &pair : sizes)
	{
		lua_pushvalue(L, lua_upvalueindex(2));
		lua_pushvalue(L, buffer);
		lua_pushlightuserdata(L, reinterpret_cast<void*>(pair.first));
		lua_pushinteger(L, pair.second);
		lua_call(L, 3, 1);
	}
	return args;
}

int heapargs(lua_State *L)
{
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto hdr = (AMX_HEADER*)amx->base;
	auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;

	int args = lua_gettop(L);
	for(int i = 1; i <= args; i++)
	{
		if(!amx::MemCheck(amx, 0))
		{
			return lua::amx_error(L, AMX_ERR_STACKERR);
		}
		if(!toblock(L, i, [=](lua_State *L, size_t size)
		{
			if(!amx::MemCheck(amx, size))
			{
				lua::amx_error(L, AMX_ERR_MEMORY);
				return static_cast<void*>(nullptr);
			}
			cell offset = amx->hea;
			amx->hea += (size_t)size;
			lua_pushlightuserdata(L, reinterpret_cast<void*>(offset));
			return static_cast<void*>(data + offset);
		}))
		{
			return lua::argerrortype(L, i, "simple type");
		}
		lua_replace(L, i);
	}
	return args;
}

int vacall(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushinteger(L, lua_gettop(L) - 1);
	lua_rotate(L, 1, 2);
	lua_pushcclosure(L, [](lua_State *L)
	{
		auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
		auto hdr = (AMX_HEADER*)amx->base;
		auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
		cell old_hea = amx->hea;
		auto heap = reinterpret_cast<cell*>(data + amx->hea);

		bool restore = lua::numresults(L) != 0;

		auto restorers = std::make_shared<std::vector<void(*)(lua_State *L, cell value)>>();

		int args = lua_gettop(L);
		for(int i = 1; i <= args; i++)
		{
			if(!amx::MemCheck(amx, 0))
			{
				return lua::amx_error(L, AMX_ERR_STACKERR);
			}

			cell value = 0;
			
			if(lua_isinteger(L, i))
			{
				value = (cell)lua_tointeger(L, i);
				if(restore) restorers->push_back([](lua_State *L, cell value){ lua_pushinteger(L, value); });
			}else if(lua::isnumber(L, i))
			{
				float num = (float)lua_tonumber(L, i);
				value = amx_ftoc(num);
				if(restore) restorers->push_back([](lua_State *L, cell value){ lua_pushnumber(L, amx_ctof(value)); });
			}else if(lua_isboolean(L, i))
			{
				value = lua_toboolean(L, i);
				if(restore) restorers->push_back([](lua_State *L, cell value){ lua_pushboolean(L, value); });
			}else if(lua_islightuserdata(L, i))
			{
				value = reinterpret_cast<cell>(lua_touserdata(L, i));
				if(restore) restorers->push_back([](lua_State *L, cell value){ lua_pushlightuserdata(L, reinterpret_cast<void*>(value)); });
			}else{
				continue;
			}

			lua_pushlightuserdata(L, reinterpret_cast<void*>(amx->hea));
			*(heap++) = value;
			amx->hea += sizeof(cell);

			lua_replace(L, i);
		}

		int num = (int)lua_tointeger(L, lua_upvalueindex(2));
		luaL_checkstack(L, num, nullptr);
		for(int i = 1; i <= num; i++)
		{
			lua_pushvalue(L, lua_upvalueindex(2 + i));
		}
		lua_rotate(L, 1, num);
		return lua::pcallk(L, lua_gettop(L) - 1, restore ? LUA_MULTRET : 0, 0, [=](lua_State *L, int status)
		{
			int heapsize = (amx->hea - old_hea) / sizeof(cell);
			amx->hea = old_hea;
			switch(status)
			{
				case LUA_OK:
				case LUA_YIELD:
				{
					int nr = lua::numresults(L);
					if(nr == LUA_MULTRET)
					{
						nr = restorers->size();
					}else{
						nr -= lua_gettop(L);
					}
					if(nr > heapsize)
					{
						nr = heapsize;
					}
					auto heap = reinterpret_cast<cell*>(data + amx->hea);
					luaL_checkstack(L, nr, nullptr);
					for(int i = 0; i < nr; i++)
					{
						(*restorers)[i](L, heap[i]);
					}
					return lua_gettop(L);
				}
				default:
				{
					return lua_error(L);
				}
			}
		});
	}, lua_gettop(L));
	return 1;
}

int asbuffer(lua_State *L)
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

	size_t dlen;
	if(static_cast<ucell>(*addr) > UNPACKEDMAX)
	{
		dlen = ((len + sizeof(cell) - 1) / sizeof(cell)) * sizeof(cell);
	}else{
		dlen = len * sizeof(cell);
	}

	auto buf = lua_newuserdata(L, dlen);
	std::memcpy(buf, addr, dlen);
	lua_pushvalue(L, lua_upvalueindex(2));
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

	lua_pushlightuserdata(L, amx);
	lua_pushvalue(L, buffer);
	lua_pushcclosure(L, asbuffer, 2);
	lua_setfield(L, table, "asbuffer");

	lua::pushuserdata(L, amx); //do not change to lightuserdata

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

	lua_pushlightuserdata(L, amx);
	lua_getfield(L, table, "span");
	lua_getfield(L, table, "heap");
	lua_pushcclosure(L, heapalloc, 3);
	lua_setfield(L, table, "heapalloc");

	lua_pushlightuserdata(L, amx);
	lua_pushcclosure(L, heapfree, 1);
	lua_setfield(L, table, "heapfree");

	lua_getfield(L, table, "heapalloc");
	lua_pushcclosure(L, toheap, 1);
	lua_setfield(L, table, "toheap");

	lua_pushvalue(L, buffer);
	lua_getfield(L, table, "span");
	lua_pushcclosure(L, _struct, 2);
	lua_setfield(L, table, "struct");

	lua_pushlightuserdata(L, amx);
	lua_pushcclosure(L, heapargs, 1);
	lua_setfield(L, table, "heapargs");

	lua_pushlightuserdata(L, amx);
	lua_pushcclosure(L, vacall, 1);
	lua_setfield(L, table, "vacall");
	
	lua_settop(L, table);
}
