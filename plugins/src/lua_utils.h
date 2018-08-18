#ifndef LUA_UTILS_H_INCLUDED
#define LUA_UTILS_H_INCLUDED

#include "lua/lua.hpp"
#include <type_traits>

namespace lua
{
	int amx_error(lua_State *L, int error);

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

	template <class Type, class=std::enable_if<!std::is_lvalue_reference<T>::value>::type>
	void pushuserdata(lua_State *L, Type &&val)
	{
		newuserdata<typename std::remove_reference<Type>::type>(L) = std::move(val);
	}

	void *testudata(lua_State *L, int ud, int mt);

	void *tobuffer(lua_State *L, int idx, size_t &length, bool &isconst);

	size_t checkoffset(lua_State *L, int idx);

	void *checklightudata(lua_State *L, int idx);
}

#endif