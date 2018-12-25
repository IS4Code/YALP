#include "interop.h"
#include "lua_utils.h"
#include "lua_api.h"
#include "amxutils.h"
#include "amx/loader.h"
#include "interop/native.h"
#include "interop/public.h"
#include "interop/pubvar.h"
#include "interop/memory.h"
#include "interop/string.h"
#include "interop/result.h"
#include "interop/file.h"
#include "interop/tags.h"
#include "interop/sleep.h"

#include <unordered_map>
#include <memory>
#include <string>
#include <limits>

std::unordered_map<AMX*, std::weak_ptr<class amx_info>> amx_map;

class amx_info
{
public:
	lua_State *L;
	std::string fs_name;
	int self = 0;
	AMX *amx = nullptr;

	amx_info(lua_State *L) : L(L)
	{

	}

	~amx_info()
	{
		amx_map.erase(amx);
		if(amx && !fs_name.empty() && amx::Unload(fs_name.c_str()))
		{
			amx = nullptr;
		}
	}
};

int forward(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_getfield(L, lua_upvalueindex(1), "#lua");
	int numresults = lua::numresults(L);
	lua_pushinteger(L, numresults);
	lua_rotate(L, 1, 3);
	int nups = lua::packupvals(L, 4, lua_gettop(L) - 3);
	lua_pushcclosure(L, [](lua_State *L)
	{
		int res = (int)lua_tointeger(L, lua_upvalueindex(3));
		lua_pushnil(L);
		lua_replace(L, lua_upvalueindex(3));

		lua_pushvalue(L, lua_upvalueindex(2));
		lua_setfield(L, lua_upvalueindex(1), "#lua");
		int num = lua::unpackupvals(L, 4);
		lua_call(L, num - 1, res);
		
		if(res == LUA_MULTRET || res > 0)
		{
			if(res != LUA_MULTRET)
			{
				lua_settop(L, res);
			}else{
				res = lua_gettop(L);
			}
			lua_createtable(L, res + 1, 0);
			lua_insert(L, 1);
			lua_pushinteger(L, res);
			lua_rawseti(L, 1, 1);
			while(res > 0)
			{
				lua_rawseti(L, 1, 1 + res--);
			}
			lua_replace(L, lua_upvalueindex(4));
		}
		return 0;
	}, nups + 3);
	if(numresults != 0)
	{
		lua_pushvalue(L, -1);
	}
	lua_setfield(L, lua_upvalueindex(1), "#lua");

	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(2)));
	int index, error;
	error = amx_FindPublic(amx, "#lua", &index);
	if(error != AMX_ERR_NONE)
	{
		return lua::amx_error(L, error);
	}
	cell retval;
	error = amx_Exec(amx, &retval, index);
	if(error != AMX_ERR_NONE)
	{
		return lua::amx_error(L, error, retval);
	}
	if(numresults == 0)
	{
		return 0;
	}
	lua_getupvalue(L, -1, 4);
	if(lua_istable(L, -1))
	{
		int table = lua_absindex(L, -1);
		lua_rawgeti(L, table, 1);
		int num = (int)lua_tointeger(L, -1);
		if(numresults != LUA_MULTRET && numresults < num)
		{
			num = numresults;
		}
		luaL_checkstack(L, num, nullptr);
		for(int i = 1; i <= num; i++)
		{
			lua_rawgeti(L, table,  1 + i);
		}
		return num;
	}else{
		lua_pushlightuserdata(L, reinterpret_cast<void*>(retval));
		return 1;
	}
}

int lua::interop::loader(lua_State *L)
{
	auto ptr = std::make_shared<amx_info>(lua::mainthread(L));
	amx_info &info = *ptr;
	lua::pushuserdata(L, ptr);
	auto loader = [&](AMX *amx, void *program)
	{
		std::unordered_map<cell, std::string> tagcache;

		int nlen;
		amx_NameLength(amx, &nlen);

		auto tagname = reinterpret_cast<char*>(alloca(nlen + 1));
		int numtags;
		amx_NumTags(amx, &numtags);
		for(int i = 0; i < numtags; i++)
		{
			cell tag_id;
			amx_GetTag(amx, i, tagname, &tag_id);

			tagcache[tag_id] = tagname;
		}

		info.amx = amx;
		info.self = luaL_ref(L, LUA_REGISTRYINDEX);
		amx_map[amx] = ptr;

		lua_newtable(L);

		lua_pushinteger(L, std::numeric_limits<cell>::min());
		lua_setfield(L, -2, "cellmin");
		lua_pushinteger(L, std::numeric_limits<cell>::max());
		lua_setfield(L, -2, "cellmax");

		lua_newtable(L);
		lua_setfield(L, -2, "public");

		init_sleep(L, amx);
		init_native(L, amx);
		init_public(L, amx);
		init_pubvar(L, amx);
		init_memory(L, amx);
		init_string(L, amx);
		init_result(L, amx);
		init_file(L, amx);
		init_tags(L, amx, tagcache);

		lua_getfield(L, -1, "public");
		lua_pushlightuserdata(L, amx);
		lua_pushcclosure(L, forward, 2);
		lua_setfield(L, -2, "forward");

		lua::pushstring(L, "#lua");
		lua_setfield(L, -2, "loopback");
	};

	AMX *amx = lua::bound_amx(L);
	if(amx)
	{
		loader(amx, nullptr);
	}else{
		info.fs_name = "?luafs_";
		info.fs_name.append(std::to_string(reinterpret_cast<intptr_t>(&info)));
		amx = amx::LoadNew(info.fs_name.c_str(), 8192, sNAMEMAX, std::move(loader));
		if(!amx)
		{
			lua_pop(L, 1);
			return 0;
		}
	}

	return 1;
}

bool lua::interop::amx_get_addr(AMX *amx, cell amx_addr, cell **phys_addr)
{
	return amx_get_param_addr(amx, amx_addr, phys_addr) || amx_get_pubvar_addr(amx, amx_addr, phys_addr);
}

void lua::interop::amx_unload(AMX *amx)
{
	auto it = amx_map.find(amx);
	if(it != amx_map.end())
	{
		if(auto lock = it->second.lock())
		{
			lock->amx = nullptr;
			lua_close(lock->L);
			lua::cleanup(lock->L);
		}else{
			amx_map.erase(it);
		}
	}
	amx_unregister_natives(amx);
}
