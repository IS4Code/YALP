#include "lua_utils.h"
#include "amxutils.h"
#include "sdk/amx/amx.h"
#include "lua/lstate.h"

void errortable(lua_State *L, int error)
{
	lua_createtable(L, 0, 3);
	lua_pushinteger(L, error);
	lua_setfield(L, -2, "__amxerr");
	luaL_where(L, 1);
	lua_setfield(L, -2, "__where");
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_getfield(L, 1, "__amxerr");
		int error = (int)lua_tointeger(L, -1);
		lua_getfield(L, 1, "__where");
		lua_pushfstring(L, "%sinternal AMX error %d: %s", lua_tostring(L, -1), error, amx::StrError(error));
		return 1;
	});
	lua_setfield(L, -2, "__tostring");
	lua_setmetatable(L, -2);
}

int lua::amx_error(lua_State *L, int error)
{
	errortable(L, error);
	return lua_error(L);
}

int lua::amx_error(lua_State *L, int error, int retval)
{
	errortable(L, error);
	lua_pushlightuserdata(L, reinterpret_cast<void*>(retval));
	lua_setfield(L, -2, "__retval");
	return lua_error(L);
}

int lua::amx_sleep(lua_State *L, int value, int cont)
{
	value = lua_absindex(L, value);
	cont = lua_absindex(L, cont);
	errortable(L, AMX_ERR_SLEEP);
	lua_pushvalue(L, value);
	lua_setfield(L, -2, "__retval");
	lua_pushvalue(L, cont);
	lua_setfield(L, -2, "__cont");
	return lua_error(L);
}

void *lua::testudata(lua_State *L, int ud, int mt)
{
	void *p = lua_touserdata(L, ud);
	if(p)
	{
		mt = lua_absindex(L, mt);
		if(lua_getmetatable(L, ud))
		{
			if(!lua_rawequal(L, -1, mt)) p = nullptr;
			lua_pop(L, 1);
			return p;
		}
	}
	return nullptr;
}

void *lua::tobuffer(lua_State *L, int idx, size_t &length, bool &isconst)
{
	idx = lua_absindex(L, idx);
	if(lua_getmetatable(L, idx))
	{
		if(lua_getfield(L, -1, "__buf") != LUA_TNIL)
		{
			lua_pushvalue(L, idx);
			lua_call(L, 1, 3);
			void *ptr = lua_touserdata(L, -3);
			if(ptr)
			{
				length = (int)luaL_checkinteger(L, -2);
				isconst = !lua_toboolean(L, -1);
			}
			lua_pop(L, 4);
			return ptr;
		}
		lua_pop(L, 2);
	}
	return nullptr;
}

size_t lua::checkoffset(lua_State *L, int idx)
{
	if(lua_isinteger(L, idx))
	{
		return (size_t)(lua_tointeger(L, idx) - 1) * sizeof(cell);
	}else if(lua_islightuserdata(L, idx))
	{
		return reinterpret_cast<size_t>(lua_touserdata(L, idx));
	}else if(lua_isnil(L, idx) || lua_isnone(L, idx))
	{
		return 0;
	}else{
		return (size_t)luaL_argerror(L, idx, "type not expected");
	}
}

void *lua::checklightudata(lua_State *L, int idx)
{
	if(lua_islightuserdata(L, idx))
	{
		return lua_touserdata(L, idx);
	}
	auto msg = lua_pushfstring(L, "light userdata expected, got %s", luaL_typename(L, idx));
	luaL_argerror(L, idx, msg);
	return nullptr;
}

int lua::load(lua_State *L, const lua::Reader &reader, const char *chunkname, const char *mode)
{
	return lua_load(L, [](lua_State *L, void *data, size_t *size) {
		return (*reinterpret_cast<const lua::Reader*>(data))(L, size);
	}, const_cast<lua::Reader*>(&reader), chunkname, mode);
}

void lua::pushcfunction(lua_State *L, const std::function<int(lua_State *L)> &fn)
{
	lua_pushlightuserdata(L, const_cast<std::function<int(lua_State *L)>*>(&fn));
	lua_pushcclosure(L, [](lua_State *L)
	{
		auto f = lua_touserdata(L, lua_upvalueindex(1));
		return (*reinterpret_cast<const std::function<int(lua_State *L)>*>(f))(L);
	}, 1);
}

int lua::getfieldprotected(lua_State *L, int idx, const char *k)
{
	idx = lua_absindex(L, idx);
	lua_pushcfunction(L, [](lua_State *L)
	{
		auto k = reinterpret_cast<const char*>(lua_touserdata(L, 2));
		lua_getfield(L, 1, k);
		return 1;
	});
	lua_pushvalue(L, idx);
	lua_pushlightuserdata(L, const_cast<char*>(k));
	return lua_pcall(L, 2, 1, 0);
}

short lua::numresults(lua_State *L)
{
	if(!L->ci) return 0;
	return L->ci->nresults;
}

lua_State *lua::mainthread(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	auto GL = lua_tothread(L, -1);
	lua_pop(L, 1);
	return GL;
}

bool lua::checkboolean(lua_State *L, int arg)
{
	luaL_checktype(L, arg, LUA_TBOOLEAN);
	return lua_toboolean(L, arg);
}

int lua::tailcall(lua_State *L, int n)
{
	int top = lua_gettop(L) - n - 1;
	lua_call(L, n, lua::numresults(L));
	return lua_gettop(L) - top;
}
