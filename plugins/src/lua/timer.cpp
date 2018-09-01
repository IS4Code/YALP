#include "timer.h"
#include "lua_utils.h"
#include "lua_api.h"

#include <utility>
#include <chrono>
#include <list>
#include <functional>
#include <memory>

typedef std::function<void()> handler_t;

unsigned int tick_count = 0;
std::list<std::pair<unsigned int, handler_t>> tick_handlers;
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

void register_tick(unsigned int ticks, handler_t &&handler)
{
	unsigned int time = tick_count + ticks;
	insert_sorted(tick_handlers, time, std::move(handler));
}

void register_timer(unsigned int interval, handler_t &&handler)
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

template <void (*Register)(unsigned int interval, handler_t &&handler)>
int settimer(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	auto interval = luaL_checkinteger(L, 2);

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	std::weak_ptr<char> handle = lua::touserdata<std::shared_ptr<char>>(L, lua_upvalueindex(1));
	L = lua::mainthread(L);
	Register((unsigned int)interval, [=]()
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

static const char HOOKKEY;

bool lua::timer::pushyielded(lua_State *L, lua_State *from)
{
	if(lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) == LUA_TTABLE)
	{
		lua_pushthread(from);
		lua_xmove(from, L, 1);
		if(lua_rawget(L, -2) != LUA_TNIL)
		{
			lua_remove(L, -2);
			lua_pushinteger(L, 1);
			return true;
		}
		lua_pop(L, 2);
	}
	return false;
}

int parallel(lua_State *L)
{
	if(!lua_isyieldable(L)) return luaL_error(L, "must be executed in a coroutine");
	int count = 100000;
	if(lua_isinteger(L, 1))
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);
		count = (int)lua_tointeger(L, 1);
		lua_remove(L, 1);
	}else if(!lua_isfunction(L, 1))
	{
		return lua::argerrortype(L, 1, "function or integer");
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
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_rawset(L, -3);
	lua_pop(L, 1);

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

	lua_getfield(L, table, "tick");
	lua_pushcclosure(L, parallel, 1);
	lua_setfield(L, table, "parallel");

	return 1;
}
