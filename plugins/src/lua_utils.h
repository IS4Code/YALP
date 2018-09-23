#ifndef LUA_UTILS_H_INCLUDED
#define LUA_UTILS_H_INCLUDED

#include "lua/lualibs.h"
#include <type_traits>
#include <functional>

#if __GNUG__ && __GNUC__ < 5
#define is_trivially_constructible(T) __has_trivial_constructor(T)
#define is_trivially_destructible(T) __has_trivial_destructor(T)
#else
#define is_trivially_constructible(T) std::is_trivially_constructible<T>::value
#define is_trivially_destructible(T) std::is_trivially_destructible<T>::value
#endif

namespace lua
{
	template <class Type>
	struct mt_ctor
	{
		bool operator()(lua_State *L)
		{
			return false;
		}
	};

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

	template <class Type>
	Type &newuserdata(lua_State *L)
	{
		auto data = reinterpret_cast<Type*>(lua_newuserdata(L, sizeof(Type)));
		if(!is_trivially_constructible(Type))
		{
			new (data) Type();
		}
		bool hasmt = mt_ctor<Type>()(L);
		if(!is_trivially_destructible(Type))
		{
			if(!hasmt)
			{
				lua_createtable(L, 0, 2);
				hasmt = true;
			}
			lua_pushboolean(L, false);
			lua_setfield(L, -2, "__metatable");
			lua_pushcfunction(L, [](lua_State *L) {
				lua::touserdata<Type>(L, -1).~Type();
				return 0;
			});
			lua_setfield(L, -2, "__gc");
		}
		if(hasmt)
		{
			lua_setmetatable(L, -2);
		}
		return *data;
	}

	template <class Type>
	void pushuserdata(lua_State *L, const Type &val)
	{
		newuserdata<Type>(L) = val;
	}

	template <class Type, class=typename std::enable_if<!std::is_lvalue_reference<Type>::value>::type>
	void pushuserdata(lua_State *L, Type &&val)
	{
		newuserdata<typename std::remove_reference<Type>::type>(L) = std::move(val);
	}

	void *testudata(lua_State *L, int ud, int mt);

	void *tobuffer(lua_State *L, int idx, size_t &length, bool &isconst);

	ptrdiff_t checkoffset(lua_State *L, int idx);

	void *checklightudata(lua_State *L, int idx);

	typedef std::function<const char*(lua_State *L, size_t *size)> Reader;

	int load(lua_State *L, const lua::Reader &reader, const char *chunkname, const char *mode);

	void pushcfunction(lua_State *L, const std::function<int(lua_State *L)> &fn);

	int pgetfield(lua_State *L, int idx, const char *k);

	int psetfield(lua_State *L, int idx, const char *k);

	short numresults(lua_State *L);
	short numresults(lua_State *L, int level);

	lua_State *mainthread(lua_State *L);

	template <size_t Len>
	const char *pushstring(lua_State *L, const char(&s)[Len])
	{
		return lua_pushlstring(L, s, Len - 1);
	}
	
	bool checkboolean(lua_State *L, int arg);

	int tailcall(lua_State *L, int n);
	int tailcall(lua_State *L, int n, int r);

	typedef std::function<int(lua_State *L, int status)> KFunction;

	int pcallk(lua_State *L, int nargs, int nresults, int errfunc, KFunction &&k);

	void callk(lua_State *L, int nargs, int nresults, KFunction &&k);

	bool isnumber(lua_State *L, int idx);

	bool isstring(lua_State *L, int idx);

	template <class... Args>
	int argerror(lua_State *L, int arg, const char *format, Args&&... args)
	{
		return luaL_argerror(L, arg, lua_pushfstring(L, format, std::forward<Args>(args)...));
	}

	int argerrortype(lua_State *L, int arg, const char *expected);

	int pgettable(lua_State *L, int idx);

	int psettable(lua_State *L, int idx);

	int pcompare(lua_State *L, int idx1, int idx2, int op);

	int parith(lua_State *L, int op);

	int pconcat(lua_State *L, int n);

	int plen(lua_State *L, int idx);

	typedef std::function<void*(void *ptr, size_t osize, size_t nsize)> Alloc;

	void setallocf(lua_State *L, Alloc &&f);
	void cleanup(lua_State *L);

	bool active(lua_State *L);

	class jumpguard
	{
		lua_State *L;
		void *jmp;
		void *gljmp;

	public:
		jumpguard(lua_State *L);
		~jumpguard();
	};

	int error(lua_State *L);

	class stackguard
	{
		lua_State *L;
		int top;

	public:
		stackguard(lua_State *L);
		~stackguard();
	};
}

#endif
