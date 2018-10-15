#include "timer.h"
#include "lua_utils.h"
#include "lua_api.h"

#include <utility>
#include <chrono>
#include <list>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>

typedef std::function<void()> handler_t;

static int tick_count = 0;
static std::list<std::pair<int, handler_t>> tick_handlers;
static std::list<std::pair<std::chrono::steady_clock::time_point, handler_t>> timer_handlers;

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

static void register_tick(int ticks, handler_t &&handler)
{
	int time = tick_count + ticks;
	insert_sorted(tick_handlers, time, std::move(handler));
}

static void register_timer(int interval, handler_t &&handler)
{
	auto time = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(interval));
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

	auto now = std::chrono::steady_clock::now();
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
static int settimer(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	auto interval = luaL_checkinteger(L, 2);
	lua_remove(L, 2);
	if(lua_gettop(L) > 1)
	{
		int nups = lua::packupvals(L, 1, lua_gettop(L));
		lua_pushcclosure(L, [](lua_State *L)
		{
			int num = lua::unpackupvals(L, 1);
			return lua::tailcall(L, num - 1);
		}, nups);
	}
	
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	std::weak_ptr<char> handle = lua::touserdata<std::shared_ptr<char>>(L, lua_upvalueindex(1));
	L = lua::mainthread(L);
	Register((int)interval, [=]()
	{
		if(auto lock = handle.lock())
		{
			lua::stackguard guard(L);
			luaL_checkstack(L, 2, nullptr);
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

static int parallelex(lua_State *L)
{
	if(!lua_isyieldable(L)) return luaL_error(L, "must be executed inside 'async'");
	if(lua_gethook(L)) return luaL_error(L, "the thread must not have any hooks");

	int count = static_cast<int>(luaL_checkinteger(L, 1));
	if(count <= 0)
	{
		return luaL_argerror(L, 1, "out of range");
	}
	luaL_checktype(L, 2, LUA_TFUNCTION);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	lua_remove(L, 1);

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
	lua_pushvalue(L, 1);
	lua_rawset(L, -3);
	lua_pop(L, 1);
	lua_remove(L, 1);

	lua_Hook hook = [](lua_State *L, lua_Debug *ar)
	{
		if(ar->event == LUA_HOOKCOUNT)
		{
			lua_yield(L, 0);
		}
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

static int parallel(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	if(!lua_isinteger(L, 2))
	{
		lua_pushinteger(L, 100000);
		lua_insert(L, 2);
	}
	lua_pushvalue(L, lua_upvalueindex(2));
	lua_insert(L, 3);
	return lua::tailcall(L, lua_gettop(L) - 1);
}

static int sleep(lua_State *L)
{
	auto interval = luaL_checkinteger(L, 1);
	auto duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(interval));
	
	auto end = std::chrono::steady_clock::now() + duration;
	lua_pushvalue(L, lua_upvalueindex(1));
	lua::pushcfunction(L, [=](lua_State *L)
	{
		if(std::chrono::steady_clock::now() >= end)
		{
			lua_pushboolean(L, true);
			return 1;
		}
		std::this_thread::yield();
		return 0;
	});
	return lua::tailcall(L, 1);
}

static int wait(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	return lua::tailyield(L, lua_gettop(L));
}

static void timeout_hook(lua_State *L, lua_Debug *ar)
{
	if(ar->event == LUA_HOOKCOUNT)
	{
		lua_getinfo(L, "Sl", ar);
		if(ar->currentline > 0)
		{
			lua_pushfstring(L, "%s:%d: ", ar->short_src, ar->currentline);
		} else {
			lua_pushfstring(L, "");
		}
		lua_pushstring(L, "function was terminated after timeout");
		lua_concat(L, 2);
		lua_error(L);
	}
}

static int timeout(lua_State *L)
{
	auto duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(luaL_checkinteger(L, 1)));
	lua_remove(L, 1);

	if(!lua_isfunction(L, 1) && !lua_isthread(L, 1))
	{
		return lua::argerrortype(L, 1, "function or thread");
	}

	struct signal
	{
		bool ended = false;
		std::mutex hook_mutex;
	};

	auto info = std::make_shared<signal>();

	lua_State *lthread;
	if(lua_isthread(L, 1))
	{
		lthread = lua_tothread(L, 1);
	}else{
		lthread = L;
	}

	std::thread([=]()
	{
		std::this_thread::sleep_for(duration);
		
		std::lock_guard<std::mutex> lock(info->hook_mutex);
		if(!info->ended)
		{
			lua_sethook(lthread, timeout_hook, LUA_MASKCOUNT, 1);
		}
	}).detach();

	if(lua_isthread(L, 1))
	{
		int nargs = lua_gettop(L) - 1;
		if(!lua_checkstack(lthread, nargs))
		{
			return luaL_error(L, "stack overflow");
		}
		lua_xmove(L, lthread, nargs);
		int status = lua_resume(lthread, L, nargs);
		std::lock_guard<std::mutex> lock(info->hook_mutex);
		info->ended = true;
		if(lua_gethook(lthread) == timeout_hook)
		{
			lua_sethook(lthread, nullptr, 0, 0);
		}
		switch(status)
		{
			case LUA_OK:
			case LUA_YIELD:
				nargs = lua_gettop(lthread);
				luaL_checkstack(L, nargs, nullptr);
				lua_xmove(lthread, L, nargs);
				return lua_gettop(L) - 1;
			default:
				lua_xmove(lthread, L, 1);
				return lua_error(L);
		}
	}else{
		return lua::pcallk(L, lua_gettop(L) - 1, lua::numresults(L), 0, [=](lua_State *L, int status)
		{
			std::lock_guard<std::mutex> lock(info->hook_mutex);
			info->ended = true;
			if(lua_gethook(L) == timeout_hook)
			{
				lua_sethook(L, nullptr, 0, 0);
			}
			switch(status)
			{
				case LUA_OK:
				case LUA_YIELD:
					return lua_gettop(L);
				default:
					return lua_error(L);
			}
		});
	}
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

	lua_pushcfunction(L, parallelex);
	lua_setfield(L, table, "parallelex");

	lua_getfield(L, table, "parallelex");
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

	lua::loadbufferx(L, "local f=...;while not f() do end", "='sleep'", nullptr);
	lua_pushcclosure(L, sleep, 1);
	lua_setfield(L, table, "sleep");

	lua_getfield(L, table, "ms");
	lua_pushcclosure(L, wait, 1);
	lua_setfield(L, table, "wait");

	lua_getfield(L, table, "tick");
	lua_pushcclosure(L, wait, 1);
	lua_setfield(L, table, "waitticks");

	lua_pushcfunction(L, timeout);
	lua_setfield(L, table, "timeout");

	return 1;
}
