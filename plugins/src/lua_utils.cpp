#include "lua_utils.h"
#include "amxutils.h"
#include "sdk/amx/amx.h"
#include "lua/lstate.h"

#include <unordered_map>

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

ptrdiff_t lua::checkoffset(lua_State *L, int idx)
{
	if(lua_isinteger(L, idx))
	{
		return (ptrdiff_t)(lua_tointeger(L, idx) - 1) * sizeof(cell);
	}else if(lua_islightuserdata(L, idx))
	{
		return reinterpret_cast<ptrdiff_t>(lua_touserdata(L, idx));
	}else if(lua_isnil(L, idx) || lua_isnone(L, idx))
	{
		return 0;
	}else{
		return (ptrdiff_t)lua::argerrortype(L, idx, "integer or light userdata");
	}
}

void *lua::checklightudata(lua_State *L, int idx)
{
	if(lua_islightuserdata(L, idx))
	{
		return lua_touserdata(L, idx);
	}
	return (void*)lua::argerrortype(L, idx, "light userdata");
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

int lua::pgetfield(lua_State *L, int idx, const char *k)
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

int lua::psetfield(lua_State *L, int idx, const char *k)
{
	idx = lua_absindex(L, idx);
	lua_pushcfunction(L, [](lua_State *L)
	{
		auto k = reinterpret_cast<const char*>(lua_touserdata(L, 2));
		lua_setfield(L, 1, k);
		return 0;
	});
	lua_pushvalue(L, idx);
	lua_pushlightuserdata(L, const_cast<char*>(k));
	lua_rotate(L, -4, 3);
	return lua_pcall(L, 3, 0, 0);
}

short lua::numresults(lua_State *L)
{
	return lua::numresults(L, 0);
}

short lua::numresults(lua_State *L, int level)
{
	CallInfo *ci;
	for(ci = L->ci; level > 0 && ci != &L->base_ci; ci = ci->previous)
	{
		level--;
	}
	if(level == 0 && ci != &L->base_ci)
	{
		return ci->nresults;
	}
	return 0;
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
	return lua::tailcall(L, n, lua::numresults(L));
}

int lua::tailcall(lua_State *L, int n, int r)
{
	int top = lua_gettop(L) - n - 1;
	lua_callk(L, n, r, top, [](lua_State *L, int status, lua_KContext top)
	{
		return lua_gettop(L) - top;
	});
	return lua_gettop(L) - top;
}

int lua::pcallk(lua_State *L, int nargs, int nresults, int errfunc, lua::KFunction &&k)
{
	if(errfunc != 0) errfunc = lua_absindex(L, errfunc);
	int kpos = lua_absindex(L, -nargs - 1);
	lua::pushuserdata(L, std::move(k));
	lua_insert(L, kpos);
	
	lua_KFunction kc = [](lua_State *L, int status, lua_KContext kpos)
	{
		lua::KFunction k = std::move(lua::touserdata<lua::KFunction>(L, kpos));
		lua_remove(L, kpos);
		return k(L, status);
	};
	return kc(L, lua_pcallk(L, nargs, nresults, errfunc, kpos, kc), kpos);
}

bool lua::isnumber(lua_State *L, int idx)
{
	return lua_type(L, idx) == LUA_TNUMBER;
}

bool lua::isstring(lua_State *L, int idx)
{
	return lua_type(L, idx) == LUA_TSTRING;
}

int lua::argerrortype(lua_State *L, int arg, const char *expected)
{
	return lua::argerror(L, arg, "%s expected, got %s", expected, luaL_typename(L, arg));
}

int lua::pgettable(lua_State *L, int idx)
{
	idx = lua_absindex(L, idx);
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_gettable(L, 1);
		return 1;
	});
	lua_pushvalue(L, idx);
	lua_rotate(L, -3, 2);
	return lua_pcall(L, 2, 1, 0);
}

int lua::psettable(lua_State *L, int idx)
{
	idx = lua_absindex(L, idx);
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_settable(L, 1);
		return 0;
	});
	lua_pushvalue(L, idx);
	lua_rotate(L, -4, 2);
	return lua_pcall(L, 3, 0, 0);
}

int lua::pcompare(lua_State *L, int idx1, int idx2, int op)
{
	idx1 = lua_absindex(L, idx1);
	idx2 = lua_absindex(L, idx2);
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_pushboolean(L, lua_compare(L, 1, 2, (int)lua_tointeger(L, 3)));
		return 1;
	});
	lua_pushvalue(L, idx1);
	lua_pushvalue(L, idx2);
	lua_pushinteger(L, op);
	return lua_pcall(L, 3, 1, 0);
}

int lua::parith(lua_State *L, int op)
{
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_arith(L, (int)lua_tointeger(L, 1));
		return 1;
	});
	lua_pushinteger(L, op);
	if(op != LUA_OPUNM && op != LUA_OPBNOT)
	{
		lua_rotate(L, -4, 2);
		return lua_pcall(L, 3, 1, 0);
	}else{
		lua_rotate(L, -3, 2);
		return lua_pcall(L, 2, 1, 0);
	}
}

int lua::pconcat(lua_State *L, int n)
{
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_concat(L, lua_gettop(L));
		return 1;
	});
	lua_insert(L, -n - 1);
	return lua_pcall(L, n, 1, 0);
}

int lua::plen(lua_State *L, int idx)
{
	idx = lua_absindex(L, idx);
	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_len(L, 1);
		return 1;
	});
	lua_pushvalue(L, idx);
	return lua_pcall(L, 1, 1, 0);
}

static std::unordered_map<void*, lua::Alloc> allocmap;

void lua::setallocf(lua_State *L, Alloc &&f)
{
	allocmap[L] = std::move(f);
	lua_setallocf(L, [](void *ud, void *ptr, size_t osize, size_t nsize)
	{
		return allocmap[ud](ptr, osize, nsize);
	}, L);
}

void lua::cleanup(lua_State *L)
{
	allocmap.erase(L);
}

bool lua::active(lua_State *L)
{
	return L->nCcalls > 0;
}

lua::jumpguard::jumpguard(lua_State *L) : L(L), jmp(L->errorJmp), gljmp(L->l_G->mainthread->errorJmp)
{
	L->errorJmp = nullptr;
	L->l_G->mainthread->errorJmp = nullptr;
}

lua::jumpguard::~jumpguard()
{
	L->errorJmp = reinterpret_cast<lua_longjmp*>(jmp);
	L->l_G->mainthread->errorJmp = reinterpret_cast<lua_longjmp*>(gljmp);
}

int lua::error(lua_State *L)
{
	if(lua_type(L, -1) == LUA_TSTRING)
	{
		luaL_where(L, 1);
		lua_insert(L, -2);
		lua_concat(L, 2);
	}
	return lua_error(L);
}

lua::stackguard::stackguard(lua_State *L) : L(L), top(lua_gettop(L))
{

}

lua::stackguard::~stackguard()
{
	assert(top == lua_gettop(L));
}
