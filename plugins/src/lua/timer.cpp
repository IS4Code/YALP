#include "timer.h"
#include "lua_utils.h"
#include "lua_api.h"

#include <utility>
#include <chrono>
#include <list>
#include <functional>
#include <memory>

typedef std::function<void()> handler_t;

static int tick_count = 0;
std::list<std::pair<int, handler_t>> tick_handlers;
std::list<std::pair<std::chrono::system_clock::time_point, handler_t>> timer_handlers;

template <class Ord, class Obj>
typename std::list<std::pair<Ord, Obj>>::iterator insert_sorted(std::list<std::pair<Ord, Obj>> &list, const Ord &ord, Obj &&obj)
{
	for(auto it = list.begin();; it++)
	{
		if(it == list.end() || it->first > ord)
		{
			return list.insert(it, std::make_pair(ord, std::forward<Obj>(obj)));
		}
	}
}

void register_tick(int ticks, handler_t &&handler)
{
	int time = tick_count + ticks;
	insert_sorted(tick_handlers, time, std::move(handler));
}

void register_timer(int interval, handler_t &&handler)
{
	auto time = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds(interval));
	insert_sorted(timer_handlers, time, std::move(handler));
}

void lua::timer::tick()
{
	tick_count++;
	{
		auto it = tick_handlers.begin();
		while(it != tick_handlers.end())
		{
			auto &pair = *it;

			if(pair.first <= tick_count)
			{
				auto handler = std::move(pair.second);
				it = tick_handlers.erase(it);
				handler();
			}else{
				break;
			}
		}
	}
	if(tick_handlers.empty())
	{
		tick_count = 0;
	}

	auto now = std::chrono::system_clock::now();
	{
		auto it = timer_handlers.begin();
		while(it != timer_handlers.end())
		{
			auto &pair = *it;

			if(pair.first <= now)
			{
				auto handler = std::move(pair.second);
				it = timer_handlers.erase(it);
				handler();
			}else{
				break;
			}
		}
	}
}

void lua::timer::close()
{
	tick_handlers.clear();
	timer_handlers.clear();
}

template <void (*Register)(int interval, handler_t &&handler)>
int settimer(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	auto interval = luaL_checkinteger(L, 2);
	if(lua_gettop(L) > 2)
	{
		lua_pushinteger(L, lua_gettop(L) - 2);
		lua_replace(L, 2);
		lua_pushcclosure(L, [](lua_State *L)
		{
			lua_pushvalue(L, lua_upvalueindex(1));
			int num = (int)lua_tointeger(L, lua_upvalueindex(2));
			luaL_checkstack(L, num, nullptr);
			for(int i = 1; i <= num; i++)
			{
				lua_pushvalue(L, lua_upvalueindex(2 + i));
			}
			return lua::tailcall(L, num);
		}, lua_gettop(L));
	}else{
		lua_remove(L, 2);
	}
	
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	std::weak_ptr<char> handle = lua::touserdata<std::shared_ptr<char>>(L, lua_upvalueindex(1));
	L = lua::mainthread(L);
	Register((int)interval, [=]()
	{
		if(auto lock = handle.lock())
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
			luaL_unref(L, LUA_REGISTRYINDEX, ref);
			int err = lua_pcall(L, 0, 0, 0);
			if(err != LUA_OK)
			{
				lua::report_error(L, err);
				lua_pop(L, 1);
			}
		}
	});

	return 0;
}

static const char HOOKKEY = 0;

bool lua::timer::pushyielded(lua_State *L, lua_State *from)
{
	luaL_checkstack(L, 4, nullptr);
	if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) == LUA_TTABLE)
	{
		if(!lua_checkstack(from, 1))
		{
			luaL_error(L, "stack overflow");
		}
		lua_pushthread(from);
		lua_xmove(from, L, 1);
		if(lua_rawget(L, -2) != LUA_TNIL)
		{
			lua_remove(L, -2);
			return true;
		}
		lua_pop(L, 2);
	}
	return false;
}

int parallelreg(lua_State *L)
{
	if(!lua_isyieldable(L)) return luaL_error(L, "must be executed inside 'async'");
	int count = 100000;
	if(lua_isinteger(L, 1))
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);
		luaL_checktype(L, 3, LUA_TFUNCTION);
		count = (int)lua_tointeger(L, 1);
		if(count <= 0)
		{
			return luaL_argerror(L, 1, "out of range");
		}
		lua_remove(L, 1);
	}else if(!lua_isfunction(L, 1))
	{
		return lua::argerrortype(L, 1, "function or integer");
	}else{
		luaL_checktype(L, 2, LUA_TFUNCTION);
	}

	if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 0, 2);
		lua_pushvalue(L, -1);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &HOOKKEY);
		lua_pushstring(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_pushvalue(L, -1);
		lua_setmetatable(L, -2);
	}
	lua_pushthread(L);
	lua_pushvalue(L, 2);
	lua_rawset(L, -3);
	lua_pop(L, 1);
	lua_remove(L, 2);

	lua_Hook hook = [](lua_State *L, lua_Debug *ar)
	{
		lua_yield(L, 0);
	};

	lua_sethook(L, hook, LUA_MASKCOUNT, count);
	lua_KFunction cont = [](lua_State *L, int status, lua_KContext ctx)
	{
		lua_sethook(L, nullptr, 0, 0);
		lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY);
		lua_pushthread(L);
		lua_pushnil(L);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		switch(status)
		{
			case LUA_OK:
			case LUA_YIELD:
				return lua_gettop(L);
			default:
				return lua_error(L);
		}
	};
	
	return cont(L, lua_pcallk(L, lua_gettop(L) - 1, lua::numresults(L), 0, 0, cont), 0);
}

int parallel(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	lua_pushvalue(L, lua_upvalueindex(2));
	if(lua_isinteger(L, 2))
	{
		lua_insert(L, 4);
	}else{
		lua_insert(L, 3);
	}
	return lua::tailcall(L, lua_gettop(L) - 1);
}

int lua::timer::loader(lua_State *L)
{
	lua_createtable(L, 0, 2);
	int table = lua_absindex(L, -1);

	lua::pushuserdata(L, std::make_shared<char>());
	lua_pushvalue(L, -1);
	lua_pushcclosure(L, settimer<register_timer>, 1);
	lua_setfield(L, table, "ms");
	lua_pushvalue(L, -1);
	lua_pushcclosure(L, settimer<register_tick>, 1);
	lua_setfield(L, table, "tick");
	luaL_ref(L, LUA_REGISTRYINDEX);

	lua_pushcfunction(L, parallelreg);
	lua_setfield(L, table, "parallelreg");

	lua_getfield(L, table, "parallelreg");
	lua_getfield(L, table, "tick");
	lua_pushcclosure(L, [](lua_State *L)
	{
		lua_settop(L, 1);
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_insert(L, 1);
		lua_pushinteger(L, 1);
		return lua::tailcall(L, lua_gettop(L) - 1);
	}, 1);
	lua_pushcclosure(L, parallel, 2);
	lua_setfield(L, table, "parallel");

	return 1;
}
