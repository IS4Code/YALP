#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

template <class Type>
struct lua_adapt_return
{

};

template <>
struct lua_adapt_return<int>
{
	static cell convert(int arg)
	{
		return arg;
	}
};

template <>
struct lua_adapt_return<size_t>
{
	static cell convert(size_t arg)
	{
		return static_cast<cell>(arg);
	}
};

template <>
struct lua_adapt_return<lua_Integer>
{
	static cell convert(lua_Integer arg)
	{
		return static_cast<cell>(arg);
	}
};

template <>
struct lua_adapt_return<lua_Number>
{
	static cell convert(lua_Number arg)
	{
		float fval = static_cast<float>(arg);
		return amx_ftoc(fval);
	}
};

template <>
struct lua_adapt_return<void*>
{
	static cell convert(void *arg)
	{
		return reinterpret_cast<cell>(arg);
	}
};

template <>
struct lua_adapt_return<const void*>
{
	static cell convert(const void *arg)
	{
		return reinterpret_cast<cell>(arg);
	}
};

template <>
struct lua_adapt_return<lua_State*>
{
	static cell convert(lua_State *arg)
	{
		return reinterpret_cast<cell>(arg);
	}
};

template <>
struct lua_adapt_return<const lua_Number*>
{
	static cell convert(const lua_Number *arg)
	{
		return static_cast<cell>(*arg);
	}
};

template <class Type>
struct lua_adapt_arg
{

};

template <>
struct lua_adapt_arg<int>
{
	static int convert(cell arg)
	{
		return arg;
	}
};

template <>
struct lua_adapt_arg<size_t>
{
	static size_t convert(cell arg)
	{
		return arg;
	}
};

template <>
struct lua_adapt_arg<lua_Integer>
{
	static lua_Integer convert(cell arg)
	{
		return arg;
	}
};

template <>
struct lua_adapt_arg<lua_Number>
{
	static lua_Number convert(cell arg)
	{
		return amx_ctof(arg);
	}
};

template <>
struct lua_adapt_arg<void*>
{
	static void *convert(cell arg)
	{
		return reinterpret_cast<void*>(arg);
	}
};

template <>
struct lua_adapt_arg<const void*>
{
	static const void *convert(cell arg)
	{
		return reinterpret_cast<const void*>(arg);
	}
};

template <>
struct lua_adapt_arg<lua_State*>
{
	static lua_State *convert(cell arg)
	{
		return reinterpret_cast<lua_State*>(arg);
	}
};

template <class FType, FType Func>
struct lua_adapt
{

};

template <void(Func)(lua_State *L)>
struct lua_adapt<void(lua_State *L), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		Func(L);
		return 0;
	}
};


template <class Return, Return(Func)(lua_State *L)>
struct lua_adapt<Return(lua_State *L), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		return lua_adapt_return<Return>::convert(Func(L));
	}
};

template <class Arg1, void(Func)(lua_State *L, Arg1)>
struct lua_adapt<void(lua_State *L, Arg1), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		Func(L, lua_adapt_arg<Arg1>::convert(params[2]));
		return 0;
	}
};

template <class Return, class Arg1, Return(Func)(lua_State *L, Arg1)>
struct lua_adapt<Return(lua_State *L, Arg1), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		return lua_adapt_return<Return>::convert(Func(L, lua_adapt_arg<Arg1>::convert(params[2])));
	}
};

template <class Arg1, class Arg2, void(Func)(lua_State *L, Arg1, Arg2)>
struct lua_adapt<void(lua_State *L, Arg1, Arg2), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		Func(L, lua_adapt_arg<Arg1>::convert(params[2]), lua_adapt_arg<Arg2>::convert(params[3]));
		return 0;
	}
};

template <class Return, class Arg1, class Arg2, Return(Func)(lua_State *L, Arg1, Arg2)>
struct lua_adapt<Return(lua_State *L, Arg1, Arg2), Func>
{
	static cell AMX_NATIVE_CALL native(AMX *amx, cell *params)
	{
		auto L = reinterpret_cast<lua_State*>(params[1]);
		return lua_adapt_return<Return>::convert(Func(L, lua_adapt_arg<Arg1>::convert(params[2]), lua_adapt_arg<Arg2>::convert(params[3])));
	}
};

