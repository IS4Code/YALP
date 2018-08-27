#ifndef LUA_UTILS_H_INCLUDED
#define LUA_UTILS_H_INCLUDED

#include "lua/lualibs.h"
#include <type_traits>
#include <functional>

namespace lua
{
	int amx_error(lua_State *L, int error);
	int amx_error(lua_State *L, int error, int retval);
	int amx_sleep(lua_State *L, int value, int cont);

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

	template <class Type, class... Args>
	Type &newuserdata(lua_State *L, Args &&...args)
	{
		auto data = reinterpret_cast<Type*>(lua_newuserdata(L, sizeof(Type)));
		if(!std::is_trivially_constructible<Type, Args...>::value)
		{
			new (data) Type(std::forward<Args>(args)...);
		}
		if(!std::is_trivially_destructible<Type>::value)
		{
			lua_createtable(L, 0, 2);
			lua_pushboolean(L, false);
			lua_setfield(L, -2, "__metatable");
			lua_pushcfunction(L, [](lua_State *L) {
				lua::touserdata<Type>(L, -1).~Type();
				return 0;
			});
			lua_setfield(L, -2, "__gc");
			lua_setmetatable(L, -2);
		}
		return *data;
	}

	template <class Type>
	void pushuserdata(lua_State *L, const Type &val)
	{
		newuserdata<Type>(L) = val;
	}

	template <class Type, class=std::enable_if<!std::is_lvalue_reference<Type>::value>::type>
	void pushuserdata(lua_State *L, Type &&val)
	{
		newuserdata<typename std::remove_reference<Type>::type>(L) = std::move(val);
	}

	void *testudata(lua_State *L, int ud, int mt);

	void *tobuffer(lua_State *L, int idx, size_t &length, bool &isconst);

	size_t checkoffset(lua_State *L, int idx);

	void *checklightudata(lua_State *L, int idx);

	typedef std::function<const char*(lua_State *L, size_t *size)> Reader;

	int load(lua_State *L, const lua::Reader &reader, const char *chunkname, const char *mode);

	void pushcfunction(lua_State *L, const std::function<int(lua_State *L)> &fn);

	int getfieldprotected(lua_State *L, int idx, const char *k);

	short numresults(lua_State *L);

	lua_State *mainthread(lua_State *L);

	template <size_t Len>
	const char *pushstring(lua_State *L, const char(&s)[Len])
	{
		return lua_pushlstring(L, s, Len - 1);
	}
	
	bool checkboolean(lua_State *L, int arg);

	int tailcall(lua_State *L, int n);
	int tailcall(lua_State *L, int n, int r);
}

#endif