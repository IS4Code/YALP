#include "lua_api.h"
#include "lua_info.h"
#include "amxutils.h"
#include "timers.h"
#include <cstring>
#include <vector>
#include <tuple>

constexpr cell STKMARGIN = 16 * sizeof(cell);
constexpr char *LUA_MT_NATIVE = "native";
constexpr char *LUA_MT_PAWNRESULT = "pawnresult";
constexpr char *LUA_MT_BUFFER = "buffer";
constexpr char *LUA_MT_CONST_BUFFER = "cbuffer";
constexpr char *LUA_MT_HEAP = "heap";
constexpr char *LUA_PUBLIC_TABLE = "_public";
constexpr char *LUA_PUBLIC_IDX_TABLE = "_publici";
constexpr char *LUA_TIMER_TABLE = "_timer";

namespace lua
{
	template <class Type>
	void pushuserdata(lua_State *L, const Type &val)
	{
		auto data = reinterpret_cast<Type*>(lua_newuserdata(L, sizeof(Type)));
		*data = val;
	}

	template <class Type>
	void pushuserdata(lua_State *L, Type &&val)
	{
		auto data = reinterpret_cast<typename std::remove_reference<Type>::type*>(lua_newuserdata(L, sizeof(Type)));
		*data = std::move(val);
	}

	template <class Type>
	struct _udata
	{
		typedef Type &type;

		static Type &to(lua_State *L, int idx)
		{
			return *reinterpret_cast<Type*>(lua_touserdata(L, idx));
		}

		static Type &check(lua_State *L, int idx, const char *tname)
		{
			return *reinterpret_cast<Type*>(luaL_checkudata(L, idx, tname));
		}
	};

	template <class Type>
	struct _udata<Type[]>
	{
		typedef Type *type;

		static Type *to(lua_State *L, int idx)
		{
			return reinterpret_cast<Type*>(lua_touserdata(L, idx));
		}

		static Type *check(lua_State *L, int idx, const char *tname)
		{
			return reinterpret_cast<Type*>(luaL_checkudata(L, idx, tname));
		}
	};

	template <class Type>
	typename _udata<Type>::type touserdata(lua_State *L, int idx)
	{
		return _udata<Type>::to(L, idx);
	}

	template <class Type>
	typename _udata<Type>::type checkudata(lua_State *L, int idx, const char *tname)
	{
		return _udata<Type>::check(L, idx, tname);
	}

	int amx_error(lua_State *L, int error)
	{
		return luaL_error(L, "internal AMX error %d: %s", error, amx::StrError(error));
	}
}

namespace native
{
	int find(lua_State *L)
	{
		auto &info = *lua::get(L);
		auto name = luaL_checkstring(L, 1);
		auto it = info.natives.find(name);
		if(it != info.natives.end())
		{
			lua::pushuserdata(L, it->second);
			luaL_setmetatable(L, LUA_MT_NATIVE);
			return 1;
		}
		lua_pushnil(L);
		return 1;
	}

	int __call(lua_State *L)
	{
		auto native = lua::checkudata<AMX_NATIVE>(L, 1, LUA_MT_NATIVE);
		if(native)
		{
			auto amx = lua::get(L)->amx;
			cell hea = amx->hea;
			cell stk = amx->stk;

			auto hdr = (AMX_HEADER*)amx->base;
			auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;

			std::vector<std::tuple<void*, void*, size_t>> storage;

			char result_as = 1;
			int paramcount = 0;
			for(int i = lua_gettop(L); i >= 2; i--)
			{
				if(amx->hea + STKMARGIN > amx->stk)
				{
					return lua::amx_error(L, AMX_ERR_STACKERR);
				}

				cell value = 0;
			
				if(lua_isinteger(L, i))
				{
					value = (cell)lua_tointeger(L, i);
				}else if(lua_isnumber(L, i))
				{
					float num = (float)lua_tonumber(L, i);
					value = amx_ftoc(num);
				}else if(lua_isboolean(L, i))
				{
					value = lua_toboolean(L, i);
				}else if(lua_isstring(L, i))
				{
					size_t len;
					auto str = lua_tolstring(L, i, &len);
					len++;

					if(amx->stk - amx->hea - len * sizeof(cell) < STKMARGIN)
					{
						return lua::amx_error(L, AMX_ERR_MEMORY);
					}

					value = amx->hea;
					auto addr = reinterpret_cast<cell*>(data + amx->hea);
					for(size_t j = 0; j < len; j++)
					{
						addr[j] = str[j];
					}
					amx->hea += len * sizeof(cell);
				}else if(luaL_testudata(L, i, LUA_MT_PAWNRESULT))
				{
					result_as = lua::touserdata<char>(L, i);
					continue;
				}else if(luaL_testudata(L, i, LUA_MT_BUFFER) || luaL_testudata(L, i, LUA_MT_CONST_BUFFER))
				{
					size_t len = lua_rawlen(L, i);

					if(amx->stk - amx->hea - len < STKMARGIN)
					{
						return lua::amx_error(L, AMX_ERR_MEMORY);
					}

					value = amx->hea;
					auto buf = lua_touserdata(L, i);
					auto addr = data + amx->hea;
					std::memcpy(addr, buf, len);
					if(luaL_testudata(L, i, LUA_MT_BUFFER))
					{
						storage.push_back(std::make_tuple(addr, buf, len));
					}
					amx->hea += len;
				}else if(lua_isnil(L, i))
				{
					value = 0;
				}else if(lua_islightuserdata(L, i))
				{
					value = reinterpret_cast<cell>(lua_touserdata(L, i));
				}else{
					return luaL_argerror(L, i - 1, "type not expected");
				}
				
				amx->stk -= sizeof(cell);
				paramcount++;
				*(cell*)(data + amx->stk) = value;
			}

			amx->stk -= sizeof(cell);
			auto params = (cell*)(data + amx->stk);
			*params = paramcount * sizeof(cell);

			amx->error = 0;
			cell result = native(amx, params);

			for(const auto &mem : storage)
			{
				std::memcpy(std::get<1>(mem), std::get<0>(mem), std::get<2>(mem));
			}

			amx->stk = stk;
			amx->hea = hea;

			if(amx->error)
			{
				int code = amx->error;
				amx->error = 0;
				return lua::amx_error(L, code);
			}

			switch(result_as)
			{
				default:
					return 0;
				case 1:
					lua_pushinteger(L, result);
					break;
				case 2:
					lua_pushboolean(L, !!result);
					break;
				case 3:
					lua_pushnumber(L, amx_ctof(result));
					break;
				case 4:
					lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
					break;
			}
			return 1;
		}
		return 0;
	}

	void init(lua_State *L)
	{
		luaL_newmetatable(L, LUA_MT_NATIVE);
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "__metatable");
		lua_pushcfunction(L, __call);
		lua_setfield(L, -2, "__call");
		lua_pop(L, 1);

		lua_register(L, "findfunc", find);
	}
}

namespace pawnresult
{
	int __tostring(lua_State *L)
	{
		char result_as = lua::checkudata<char>(L, 1, LUA_MT_PAWNRESULT);
		switch(result_as)
		{
			case 0:
				lua_pushstring(L, "none");
				return 1;
			case 1:
				lua_pushstring(L, "integer");
				return 1;
			case 2:
				lua_pushstring(L, "boolean");
				return 1;
			case 3:
				lua_pushstring(L, "float");
				return 1;
			case 4:
				lua_pushstring(L, "userdata");
				return 1;
		}
		return 0;
	}

	int __call(lua_State *L)
	{
		char result_as = lua::checkudata<char>(L, 1, LUA_MT_PAWNRESULT);
		luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
		auto value = reinterpret_cast<cell>(lua_touserdata(L, 2));
		switch(result_as)
		{
			default:
				return 0;
			case 1:
				lua_pushinteger(L, value);
				break;
			case 2:
				lua_pushboolean(L, !!value);
				break;
			case 3:
				lua_pushnumber(L, amx_ctof(value));
				break;
			case 4:
				lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
				break;
		}
		return 0;
	}

	void init(lua_State *L)
	{
		luaL_newmetatable(L, LUA_MT_PAWNRESULT);
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "__metatable");
		lua_pushcfunction(L, __tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, __call);
		lua_setfield(L, -2, "__call");
		lua_pop(L, 1);

		lua::pushuserdata<char>(L, 0);
		luaL_setmetatable(L, LUA_MT_PAWNRESULT);
		lua_setglobal(L, "as_none");

		lua::pushuserdata<char>(L, 1);
		luaL_setmetatable(L, LUA_MT_PAWNRESULT);
		lua_setglobal(L, "as_integer");

		lua::pushuserdata<char>(L, 2);
		luaL_setmetatable(L, LUA_MT_PAWNRESULT);
		lua_setglobal(L, "as_boolean");

		lua::pushuserdata<char>(L, 3);
		luaL_setmetatable(L, LUA_MT_PAWNRESULT);
		lua_setglobal(L, "as_float");

		lua::pushuserdata<char>(L, 4);
		luaL_setmetatable(L, LUA_MT_PAWNRESULT);
		lua_setglobal(L, "as_ptr");
	}
}

namespace buffer
{
	int newbuffer(lua_State *L)
	{
		auto size = luaL_checkinteger(L, 1);
		auto buf = lua_newuserdata(L, (size_t)size * sizeof(cell));
		std::memset(buf, 0, (size_t)size * sizeof(cell));
		luaL_setmetatable(L, LUA_MT_BUFFER);
		return 1;
	}

	template <bool Const>
	int tobuffer(lua_State *L)
	{
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
		}else if(luaL_testudata(L, 1, LUA_MT_BUFFER) || luaL_testudata(L, 1, LUA_MT_CONST_BUFFER))
		{
			size_t len = lua_rawlen(L, 1);
			void *src = lua_touserdata(L, 1);
			void *dst = lua_newuserdata(L, len);
			std::memcpy(dst, src, len * sizeof(cell));
		}else if(lua_isnil(L, 1))
		{
			lua::pushuserdata(L, (cell)0);
		}else{
			luaL_argerror(L, 1, "type not expected");
			return 0;
		}
		luaL_setmetatable(L, Const ? LUA_MT_CONST_BUFFER : LUA_MT_BUFFER);
		return 1;
	}

	template <bool Const>
	int __len(lua_State *L)
	{
		luaL_checkudata(L, 1, Const ? LUA_MT_CONST_BUFFER : LUA_MT_BUFFER);
		lua_pushinteger(L, lua_rawlen(L, 1) / sizeof(cell));
		return 1;
	}

	template <bool Const>
	int __tostring(lua_State *L)
	{
		auto buf = lua::checkudata<cell[]>(L, 1, Const ? LUA_MT_CONST_BUFFER : LUA_MT_BUFFER);
		std::vector<char> str;
		size_t len = lua_rawlen(L, 1) / sizeof(cell);
		for(size_t i = 0; i < len; i++)
		{
			cell c = buf[i];
			if(c == 0)
			{
				len = i;
				break;
			}
			str.push_back((char)c);
		}
		lua_pushlstring(L, str.data(), len);
		return 1;
	}

	template <bool Const>
	int __index(lua_State *L)
	{
		auto index = luaL_checkinteger(L, 2);
		if(index >= 1 && index <= lua_rawlen(L, 1))
		{
			auto buf = lua::checkudata<cell[]>(L, 1, Const ? LUA_MT_CONST_BUFFER : LUA_MT_BUFFER);
			lua_pushinteger(L, buf[index - 1]);
			return 1;
		}
		lua_pushnil(L);
		return 1;
	}

	int __newindex(lua_State *L)
	{
		auto index = luaL_checkinteger(L, 2);
		auto value = luaL_checkinteger(L, 3);
		if(index >= 1 && index <= lua_rawlen(L, 1))
		{
			auto buf = lua::checkudata<cell[]>(L, 1, LUA_MT_BUFFER);
			buf[index - 1] = (cell)value;
		}
		return 0;
	}

	void init(lua_State *L)
	{
		luaL_newmetatable(L, LUA_MT_BUFFER);
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "__metatable");
		lua_pushcfunction(L, __len<false>);
		lua_setfield(L, -2, "__len");
		lua_pushcfunction(L, __tostring<false>);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, __index<false>);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, __newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pop(L, 1);

		luaL_newmetatable(L, LUA_MT_CONST_BUFFER);
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "__metatable");
		lua_pushcfunction(L, __len<true>);
		lua_setfield(L, -2, "__len");
		lua_pushcfunction(L, __tostring<true>);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, __index<true>);
		lua_setfield(L, -2, "__index");
		lua_pop(L, 1);

		lua_register(L, "newbuffer", newbuffer);
		lua_register(L, "tobuffer", tobuffer<false>);
		lua_register(L, "tocbuffer", tobuffer<true>);
	}
}

namespace heap
{
	int __index(lua_State *L)
	{
		luaL_checkudata(L, 1, LUA_MT_HEAP);

		lua_Integer index;
		if(lua_isinteger(L, 2))
		{
			index = lua_tointeger(L, 2) - 1;
		}else if(lua_islightuserdata(L, 2))
		{
			index = reinterpret_cast<lua_Integer>(lua_touserdata(L, 2));
		}else{
			return luaL_argerror(L, 2, "type not expected");
		}
		auto amx = lua::get(L)->amx;
		if(index >= 0 && index < amx->hea - amx->hlw)
		{
			auto hdr = (AMX_HEADER*)amx->base;
			auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
			lua_pushinteger(L, data[index + amx->hlw]);
			return 1;
		}
		lua_pushnil(L);
		return 1;
	}

	int __newindex(lua_State *L)
	{
		luaL_checkudata(L, 1, LUA_MT_HEAP);

		lua_Integer index;
		if(lua_isinteger(L, 2))
		{
			index = lua_tointeger(L, 2) - 1;
		}else if(lua_islightuserdata(L, 2))
		{
			index = reinterpret_cast<lua_Integer>(lua_touserdata(L, 2));
		}else{
			return luaL_argerror(L, 2, "type not expected");
		}
		auto value = luaL_checkinteger(L, 3);
		auto amx = lua::get(L)->amx;
		if(index >= 0 && index < amx->hea - amx->hlw)
		{
			auto hdr = (AMX_HEADER*)amx->base;
			auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
			data[index + amx->hlw] = (cell)value;
		}
		return 0;
	}

	int __len(lua_State *L)
	{
		luaL_checkudata(L, 1, LUA_MT_HEAP);

		auto amx = lua::get(L)->amx;
		lua_pushinteger(L, amx->hea - amx->hlw);
		return 1;
	}

	void init(lua_State *L)
	{
		luaL_newmetatable(L, LUA_MT_HEAP);
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "__metatable");
		lua_pushcfunction(L, __len);
		lua_setfield(L, -2, "__len");
		lua_pushcfunction(L, __index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, __newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pop(L, 1);

		lua_newuserdata(L, 0);
		luaL_setmetatable(L, LUA_MT_HEAP);
		lua_setglobal(L, "heap");
	}
}

namespace timer
{
	int settimer(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TFUNCTION);
		auto interval = luaL_checkinteger(L, 2);

		lua_getfield(L, LUA_REGISTRYINDEX, LUA_TIMER_TABLE);
		lua_pushvalue(L, 1);
		int ref = luaL_ref(L, -2);
		lua_pop(L, 1);

		std::weak_ptr<lua_info> handle = lua::get(L);

		timers::register_timer((unsigned int)interval, [=]()
		{
			if(auto info = handle.lock())
			{
				auto L = info->state;
				lua_getfield(L, LUA_REGISTRYINDEX, LUA_TIMER_TABLE);
				lua_rawgeti(L, -1, ref);
				luaL_unref(L, -2, ref);
				if(lua_pcall(L, 0, 0, 0) != 0)
				{
					lua_pop(L, 1);
				}
			}
		});

		return 0;
	}

	int setticks(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TFUNCTION);
		auto interval = luaL_checkinteger(L, 2);

		lua_getfield(L, LUA_REGISTRYINDEX, LUA_TIMER_TABLE);
		lua_pushvalue(L, 1);
		int ref = luaL_ref(L, -2);
		lua_pop(L, 1);

		std::weak_ptr<lua_info> handle = lua::get(L);

		timers::register_tick((unsigned int)interval, [=]()
		{
			if(auto info = handle.lock())
			{
				auto L = info->state;
				lua_getfield(L, LUA_REGISTRYINDEX, LUA_TIMER_TABLE);
				lua_rawgeti(L, -1, ref);
				luaL_unref(L, -2, ref);
				if(lua_pcall(L, 0, 0, 0) != 0)
				{
					lua_pop(L, 1);
				}
			}
		});

		return 0;
	}

	void init(lua_State *L)
	{
		lua_register(L, "settimer", settimer);
		lua_register(L, "setticks", setticks);
	}
}

void lua::init(lua_State *L)
{
	native::init(L);
	pawnresult::init(L);
	buffer::init(L);
	heap::init(L);
	timer::init(L);

	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_PUBLIC_IDX_TABLE);

	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_TIMER_TABLE);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "public");
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_PUBLIC_TABLE);
}

bool lua::getpublic(lua_State *L, const char *name)
{
	if(lua_getfield(L, LUA_REGISTRYINDEX, LUA_PUBLIC_TABLE) == LUA_TTABLE)
	{
		if(lua_getfield(L, -1, name) == LUA_TFUNCTION)
		{
			lua_remove(L, -2);
			return true;
		}
		lua_pop(L, 2);
		return false;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::getpublictable(lua_State *L)
{
	if(lua_getfield(L, LUA_REGISTRYINDEX, LUA_PUBLIC_IDX_TABLE) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}
