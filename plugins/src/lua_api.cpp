#include "lua_api.h"
#include "lua_utils.h"
#include "lua/timer.h"
#include "lua/interop.h"
#include "lua/remote.h"
#include "main.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>

static int custom_print(lua_State *L)
{
	int n = lua_gettop(L);
	
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	for(int i = 1; i <= n; i++)
	{
		if(i > 1) luaL_addlstring(&buf, "\t", 1);
		luaL_tolstring(L, i, nullptr);
		luaL_addvalue(&buf);
	}
	luaL_pushresult(&buf);
	logprintf("%s", lua_tostring(L, -1));
	return 0;
}

static int take(lua_State *L)
{
	int numrets = (int)luaL_checkinteger(L, 1);
	if(numrets < -1)
	{
		return luaL_argerror(L, 1, "out of range");
	}
	if(numrets == -1)
	{
		numrets = LUA_MULTRET;
	}
	return lua::tailcall(L, lua_gettop(L) - 2, numrets);
}

static int bind(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	int nups = lua::packupvals(L, 1, lua_gettop(L));
	lua_pushcclosure(L, [](lua_State *L)
	{
		int num = lua::unpackupvals(L, 1);
		lua_rotate(L, 1, num);
		return lua::tailcall(L, lua_gettop(L) - 1);
	}, nups);
	return 1;
}

static int table_clear(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushnil(L);
	while(lua_next(L, 1))
	{
		lua_pop(L, 1);
		lua_pushvalue(L, -1);
		lua_pushnil(L);
		lua_settable(L, 1);
	}
	return 0;
}

static int table_copy(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_pushnil(L);
	while(lua_next(L, 1))
	{
		lua_pushvalue(L, -2);
		lua_insert(L, -3);
		lua_settable(L, 2);
	}
	return 0;
}

static int async(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_newthread(L);
	lua_pushnil(L);
	lua_pushcclosure(L, [](lua_State *L)
	{
		auto thread = lua_tothread(L, lua_upvalueindex(1));
		int num = lua_gettop(L);
		if(!lua_checkstack(thread, num))
		{
			return luaL_error(L, "stack overflow");
		}
		lua_xmove(L, thread, num);
		if(lua_status(thread) == LUA_OK)
		{
			num--;
		}
		switch(lua_resume(thread, L, num))
		{
			case LUA_OK:
				num = lua_gettop(thread);
				luaL_checkstack(L, num, nullptr);
				lua_xmove(thread, L, num);
				return num;
			case LUA_YIELD:
				num = lua_gettop(thread);
				if(num == 0 || !lua_isfunction(thread, 1))
				{
					if(!lua::timer::pushyielded(L, thread))
					{
						return luaL_error(L, "inner function must yield a function to register the continuation");
					}
				}else{
					luaL_checkstack(L, num + 2, nullptr);
					lua_xmove(thread, L, num);
				}
				lua_pushvalue(L, lua_upvalueindex(2));
				lua_insert(L, 2);
				return lua::tailcall(L, lua_gettop(L) - 1);
			default:
				lua_xmove(thread, L, 1);
				return lua_error(L);
		}
	}, 2);
	lua_insert(L, 1);
	lua_pushvalue(L, 1);
	lua_setupvalue(L, 1, 2);
	return lua::tailcall(L, lua_gettop(L) - 1);
}

static int import(lua_State *L)
{
	lua_Debug ar;
	if(!lua_getstack(L, 1, &ar))
	{
		return luaL_error(L, "stack not available");
	}
	lua_getinfo(L, "S", &ar);
	if(ar.what[0] == 'C' && ar.what[1] == '\0')
	{
		return luaL_error(L, "must be called from a Lua function");
	}
	
	int numlibs = lua_gettop(L);
	int idx = 0;
	luaL_checkstack(L, 3, nullptr);
	while(auto name = lua_getlocal(L, &ar, ++idx))
	{
		if(lua_isnil(L, -1))
		{
			lua_pushstring(L, name);
			for(int i = 1; i <= numlibs; i++)
			{
				lua_pushvalue(L, -1);
				if(lua_gettable(L, i) != LUA_TNIL)
				{
					lua_setlocal(L, &ar, idx);
					break;
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
	}
	return 0;
}

static int argcheck(lua_State *L)
{
	luaL_checkstring(L, 2);
	int arg = (int)luaL_optinteger(L, 3, -1);
	lua_settop(L, 2);
	lua_rawget(L, lua_upvalueindex(1));
	int typecheck = (int)lua_tointeger(L, -1);
	int argtype = lua_type(L, 1);
	if((typecheck <= -2 || typecheck == LUA_TNUMBER) && argtype == LUA_TSTRING)
	{
		if(lua_isnumber(L, 1))
		{
			argtype = LUA_TNUMBER;
		}
	}
	if(typecheck == argtype)
	{
		lua_settop(L, 1);
		return 1;
	}
	if(typecheck == -2 && argtype == LUA_TNUMBER)
	{
		if(lua_isinteger(L, 1))
		{
			lua_settop(L, 1);
			return 1;
		}
		auto num = lua_tonumber(L, 1);
		auto intval = static_cast<lua_Integer>(num);
		if(intval == num)
		{
			lua_pushinteger(L, intval);
			return 1;
		}
	}else if(typecheck == -3 && argtype == LUA_TNUMBER)
	{
		if(!lua_isinteger(L, 1))
		{
			lua_settop(L, 1);
			return 1;
		}
		lua_pushnumber(L, static_cast<lua_Number>(lua_tointeger(L, 1)));
		return 1;
	}

	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	if(arg == -1)
	{
		lua_getinfo(L, "un", &ar);
		bool found = false;
		for(unsigned char i = 1; i <= ar.nparams; i++)
		{
			if(lua_getlocal(L, &ar, i))
			{
				if(lua_rawequal(L, 1, -1))
				{
					if(!found)
					{
						arg = i;
						found = true;
					}else{
						arg = -1;
						lua_pop(L, 1);
						break;
					}
				}
				lua_pop(L, 1);
			}
		}
	}else{
		lua_getinfo(L, "n", &ar);
	}
	const char *typecheckname, *argtypename;
	if(typecheck >= LUA_TNIL)
	{
		typecheckname = typecheck == LUA_TLIGHTUSERDATA ? "light userdata" : lua_typename(L, typecheck);
		argtypename = argtype == LUA_TLIGHTUSERDATA ? "light userdata" : lua_typename(L, argtype);
	}else{
		if(typecheck == -2)
		{
			typecheckname = "integer";
			if(argtype == LUA_TNUMBER)
			{
				argtypename = "float";
			}else{
				argtypename = lua_typename(L, argtype);
			}
		}else if(typecheck == -3)
		{
			typecheckname = "float";
			if(argtype == LUA_TNUMBER)
			{
				argtypename = "integer";
			}else{
				argtypename = lua_typename(L, argtype);
			}
		}
	}

	luaL_where(L, 2);
	if(arg == -1)
	{
		lua_pushfstring(L, "bad argument to '%s' (%s expected, got %s)", ar.name, typecheckname, argtypename);
	}else{
		lua_pushfstring(L, "bad argument #%d to '%s' (%s expected, got %s)", arg, ar.name, typecheckname, argtypename);
	}
	lua_concat(L, 2);
	return lua_error(L);
}

static int optcheck(lua_State *L)
{
	if(lua_toboolean(L, 1) || (!lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TBOOLEAN))
	{
		lua_settop(L, 1);
		return 1;
	}

	int arg = (int)luaL_optinteger(L, 3, -1);
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	if(arg == -1)
	{
		lua_getinfo(L, "un", &ar);
		bool found = false;
		for(unsigned char i = 1; i <= ar.nparams; i++)
		{
			if(lua_getlocal(L, &ar, i))
			{
				if(lua_rawequal(L, 1, -1))
				{
					if(!found)
					{
						arg = i;
						found = true;
					}else{
						arg = -1;
						lua_pop(L, 1);
						break;
					}
				}
				lua_pop(L, 1);
			}
		}
	}else{
		lua_getinfo(L, "n", &ar);
	}

	auto option = lua_tostring(L, 2);
	if(!option)
	{
		option = luaL_typename(L, 2);
	}
	luaL_where(L, 2);
	if(arg == -1)
	{
		lua_pushfstring(L, "bad argument to '%s' (invalid option '%s')", ar.name, option);
	}else{
		lua_pushfstring(L, "bad argument #%d to '%s' (invalid option '%s')", arg, ar.name, option);
	}
	lua_concat(L, 2);
	return lua_error(L);
}

static int map_cont(lua_State *L, int status, lua_KContext ctx)
{
	int numret = lua_gettop(L) - (ctx & 0xFF);
	int next = (ctx & ~0xFF) >> 8;
	while(next <= lua_gettop(L))
	{
		if(next == 1)
		{
			next = 2;
		}else{
			lua_rotate(L, next, numret);
			next += numret;
		}
		if(next > lua_gettop(L) || (lua::numresults(L) != LUA_MULTRET && lua::numresults(L) <= next - 2))
		{
			break;
		}
		lua_pushvalue(L, 1);
		lua_rotate(L, next, -1);
		int top = lua_gettop(L) - 2;
		lua_pushinteger(L, next - 1);
		lua_callk(L, 2, LUA_MULTRET, (next << 8) | top, map_cont);
		numret = lua_gettop(L) - top;
	}
	return lua_gettop(L) - 1;
}

static int map(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	return map_cont(L, LUA_OK, 1 << 8);
}

static int concat(lua_State *L)
{
	int nups = lua::packupvals(L, 1, lua_gettop(L));
	lua_pushcclosure(L, [](lua_State *L)
	{
		int num = lua::unpackupvals(L, 1);
		lua_rotate(L, 1, num);
		return lua_gettop(L);
	}, nups);
	return 1;
}

static int debug_numresults(lua_State *L)
{
	int level = (int)luaL_optinteger(L, 1, 1);
	int num = lua::numresults(L, level);
	lua_pushinteger(L, num == LUA_MULTRET ? -1 : num);
	return 1;
}

std::queue<std::weak_ptr<lua_State*>> exit_queue;

static int exit(lua_State *L)
{
	if(lua::mainthread(L) != L)
	{
		return luaL_error(L, "must be called in the main thread");
	}
	lua_Hook hook = [](lua_State *L, lua_Debug *ar)
	{
		luaL_error(L, "exit requested");
	};

	auto ptr = std::make_shared<lua_State*>(L);
	exit_queue.push(ptr);
	lua::pushuserdata(L, std::move(ptr));
	luaL_ref(L, LUA_REGISTRYINDEX);

	lua_sethook(L, hook, LUA_MASKCALL | LUA_MASKCOUNT | LUA_MASKRET, 1);
	hook(L, nullptr);
	return 0;
}

static int array(lua_State *L)
{
	int top = lua_gettop(L);
	if(top == 0)
	{
		lua_createtable(L, 0, 1);
	}else{
		lua_createtable(L, top - 1, 2);
	}
	lua_insert(L, 1);
	lua_pushinteger(L, top);
	lua_setfield(L, 1, "n");
	while(top > 0)
	{
		lua_seti(L, 1, top - 1);
		top--;
	}
	return 1;
}

static int open_package(lua_State *L)
{
	luaopen_package(L);
	lua::pushliteral(L, "scriptfiles" LUA_DIRSEP "lua" LUA_DIRSEP "?.lua");
	lua_setfield(L, -2, "path");
#ifdef _WIN32
	lua::pushliteral(L, "plugins" LUA_DIRSEP "lua" LUA_DIRSEP "?.dll");
#else
	lua::pushliteral(L, "plugins" LUA_DIRSEP "lua" LUA_DIRSEP "?.so");
#endif
	lua_setfield(L, -2, "cpath");
	return 1;
}

static int open_base(lua_State *L)
{
	luaopen_base(L);
	int table = lua_absindex(L, -1);

	lua::pushliteral(L, "YALP 1.0");
	lua_setfield(L, table, "YALP_VERSION");

	lua_pushcfunction(L, custom_print);
	lua_setfield(L, table, "print");
	lua_pushcfunction(L, take);
	lua_setfield(L, table, "take");
	lua_pushcfunction(L, bind);
	lua_setfield(L, table, "bind");
	lua_pushcfunction(L, async);
	lua_setfield(L, table, "async");
	lua_pushcfunction(L, import);
	lua_setfield(L, table, "import");

	lua_createtable(L, 0, LUA_NUMTAGS + 2);
	for(int i = 0; i < LUA_NUMTAGS; i++)
	{
		lua_pushinteger(L, i);
		lua_setfield(L, -2, i == LUA_TLIGHTUSERDATA ? "light userdata" : lua_typename(L, i));
	}
	lua_pushinteger(L, -2);
	lua_setfield(L, -2, "integer");
	lua_pushinteger(L, -3);
	lua_setfield(L, -2, "float");

	lua_pushcclosure(L, argcheck, 1);
	lua_setfield(L, table, "argcheck");

	lua_pushcfunction(L, optcheck);
	lua_setfield(L, table, "optcheck");

	lua_pushcfunction(L, map);
	lua_setfield(L, table, "map");

	lua_pushcfunction(L, concat);
	lua_setfield(L, table, "concat");

	lua_pushcfunction(L, exit);
	lua_setfield(L, table, "exit");

	lua_pushcfunction(L, array);
	lua_setfield(L, table, "array");

	lua::loadbufferx(L, R"END(
local _ENV = ...
local m = map
function apply(t, ...)
  return m(function(v, i)
    return t[i](v)
  end, ...)
end
)END", "='_G'", nullptr);
	lua_pushvalue(L, table);
	lua_call(L, 1, 0);

	open_package(L);
	lua_pop(L, 1);

	return 1;
}

static int open_table(lua_State *L)
{
	luaopen_table(L);
	lua_pushcfunction(L, table_clear);
	lua_setfield(L, -2, "clear");
	lua_pushcfunction(L, table_copy);
	lua_setfield(L, -2, "copy");
	return 1;
}

static int open_debug(lua_State *L)
{
	luaopen_debug(L);
	lua_pushcfunction(L, debug_numresults);
	lua_setfield(L, -2, "numresults");
	return 1;
}

static const char HOOKKEY = 0;

void coroutine_hookfunc(lua_State *L, lua_Debug *ar)
{
	luaL_checkstack(L, 2, nullptr);
	if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) == LUA_TTABLE)
	{
		lua_pushthread(L);
		if(lua_rawget(L, -2) == LUA_TTHREAD)
		{
			auto parent = lua_tothread(L, -1);
			lua_pop(L, 2);
			if(auto hook = lua_gethook(parent))
			{
				if(lua_gethookmask(parent) & (1 << ar->event))
				{
					hook(L, ar);
				}
			}
			return;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static int coroutine_resumehooked(lua_State *L)
{
	lua_pushvalue(L, 1);
	lua_insert(L, 1);
	lua_State *thread = lua_tothread(L, 1);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 2);
	if(thread)
	{
		lua_sethook(thread, coroutine_hookfunc, LUA_MASKCALL | LUA_MASKCOUNT | LUA_MASKLINE | LUA_MASKRET, 1);

		if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) != LUA_TTABLE)
		{
			lua_pop(L, 1);
			lua_createtable(L, 0, 2);
			lua_pushvalue(L, -1);
			lua_rawsetp(L, LUA_REGISTRYINDEX, &HOOKKEY);
			lua::pushliteral(L, "k");
			lua_setfield(L, -2, "__mode");
			lua_pushvalue(L, -1);
			lua_setmetatable(L, -2);
		}
		lua_pushvalue(L, 1);
		lua_pushthread(L);
		lua_rawset(L, -3);
		lua_pop(L, 1);
	}
	return lua::pcallk(L, lua_gettop(L) - 2, lua::numresults(L), 0, [=](lua_State *L, int status)
	{
		if(lua_gethook(thread) == coroutine_hookfunc)
		{
			lua_sethook(thread, nullptr, 0, 0);
		}
		if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) == LUA_TTABLE)
		{
			lua_pushvalue(L, 1);
			lua_pushnil(L);
			lua_rawset(L, -3);
		}
		lua_pop(L, 1);
		switch(status)
		{
			case LUA_OK:
			case LUA_YIELD:
				return lua_gettop(L) - 1;
			default:
				return lua_error(L);
		}
	});
}

static int open_coroutine(lua_State *L)
{
	luaopen_coroutine(L);
	lua_getfield(L, -1, "resume");
	lua_pushcclosure(L, coroutine_resumehooked, 1);
	lua_setfield(L, -2, "resumehooked");
	return 1;
}

static std::vector<std::pair<const char*, lua_CFunction>> libs = {
	{"_G", open_base},
	{LUA_LOADLIBNAME, open_package},
	{LUA_COLIBNAME, open_coroutine},
	{LUA_TABLIBNAME, open_table},
	{LUA_IOLIBNAME, luaopen_io},
	{LUA_OSLIBNAME, luaopen_os},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_MATHLIBNAME, luaopen_math},
	{LUA_UTF8LIBNAME, luaopen_utf8},
	{LUA_DBLIBNAME, open_debug},
	{"interop", lua::interop::loader},
	{"timer", lua::timer::loader},
	{"remote", lua::remote::loader},
};

void lua::initlibs(lua_State *L, int load, int preload)
{
	for(size_t i = 0; i < libs.size(); i++)
	{
		if(load & (1 << i))
		{
			const auto &lib = libs[i];
			luaL_requiref(L, lib.first, lib.second, 1);
			lua_pop(L, 1);
		}
	}

	if(lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE) == LUA_TTABLE)
	{
		preload &= ~load;
		for(size_t i = 0; i < libs.size(); i++)
		{
			if(preload & (1 << i))
			{
				const auto &lib = libs[i];
				lua_pushcfunction(L, lib.second);
				lua_setfield(L, -2, lib.first);
			}
		}
	}
	lua_pop(L, 1);
}

void lua::report_error(lua_State *L, int error)
{
	const char *msg = lua_tostring(L, -1);
	bool pop = false;
	if(!msg)
	{
		msg = luaL_tolstring(L, -1, nullptr);
		pop = true;
	}
	logprintf("unhandled Lua error %d: %s", error, msg);
	if(pop)
	{
		lua_pop(L, 1);
	}
}

std::unordered_map<AMX*, lua_State*> bind_map;
std::unordered_map<lua_State*, std::weak_ptr<AMX*>> init_map;

cell lua::init_bind(lua_State *L, AMX *amx)
{
	if(lua_gettop(L) == 0 || !lua_isfunction(L, -1)) return 0;
	auto it = init_map.find(L);
	if(it != init_map.end() && !it->second.expired()) return 0;
	bind_map[amx] = L;
	return 0xFFC52116;
}

int lua::bind(AMX *amx, cell *retval, int index)
{
	if(amx->pri != 0xFFC52116)
	{
		return AMX_ERR_SLEEP;
	}
	auto it = bind_map.find(amx);
	if(it == bind_map.end())
	{
		return AMX_ERR_SLEEP;
	}
	auto L = it->second;
	bind_map.erase(it);

	amx->pri = 0;
	amx->alt = 0;
	amx->reset_stk = amx->stk = amx->stp;
	amx->reset_hea = amx->hea = amx->hlw = 0;
	amx->cip = 0;

	auto hdr = (AMX_HEADER*)amx->base;
	hdr->hea = hdr->dat;

	auto ptr = std::make_shared<AMX*>(amx);
	init_map[L] = ptr;
	lua::pushuserdata(L, std::move(ptr));
	luaL_ref(L, LUA_REGISTRYINDEX);
	int error = lua_pcall(L, 0, -1, 0);
	if(error != LUA_OK)
	{
		lua::report_error(L, error);
	}
	lua_settop(L, 0);
	return AMX_ERR_SLEEP;
}

AMX *lua::bound_amx(lua_State *L)
{
	auto it = init_map.find(L);
	if(it != init_map.end())
	{
		if(auto lock = it->second.lock())
		{
			return *lock;
		}else{
			init_map.erase(it);
		}
	}
	return nullptr;
}

void lua::process_tick()
{
	decltype(exit_queue) queue;
	exit_queue.swap(queue);

	while(!queue.empty())
	{
		auto ptr = std::move(queue.front());
		queue.pop();
		if(auto lock = ptr.lock())
		{
			if(!lua::active(*lock))
			{
				lua_close(*lock);
			}else{
				exit_queue.push(lock);
			}
		}
	}
}
