#include "interop.h"
#include "lua_utils.h"
#include "amxutils.h"
#include "amx/loader.h"
#include "interop/native.h"
#include "interop/public.h"
#include "interop/memory.h"
#include "interop/string.h"
#include "interop/result.h"
#include "interop/tags.h"
#include "interop/sleep.h"

#include <unordered_map>
#include <string>

class amx_info
{
public:
	std::string fs_name;
	AMX *amx = nullptr;

	~amx_info()
	{
		if(amx && amx::Unload(fs_name.c_str()))
		{
			amx = nullptr;
		}
	}
};

int forward(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_pushinteger(L, lua_gettop(L));
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_getfield(L, lua_upvalueindex(1), "#lua");
	int numresults = lua::numresults(L);
	lua_pushinteger(L, numresults);
	lua_rotate(L, 1, 4);
	lua_pushcclosure(L, [](lua_State *L)
	{
		int res = (int)lua_tointeger(L, lua_upvalueindex(4));
		lua_pushnil(L);
		lua_replace(L, lua_upvalueindex(4));

		lua_pushvalue(L, lua_upvalueindex(3));
		lua_setfield(L, lua_upvalueindex(2), "#lua");
		int num = (int)lua_tointeger(L, lua_upvalueindex(1));
		for(int i = 1; i <= num; i++)
		{
			lua_pushvalue(L, lua_upvalueindex(4 + i));
		}
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
	}, lua_gettop(L));
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
	amx_info &info = lua::newuserdata<amx_info>(L);
	info.fs_name = "?luafs_";
	info.fs_name.append(std::to_string(reinterpret_cast<intptr_t>(&info)));
	AMX *amx = amx::LoadNew(info.fs_name.c_str(), 1024, sNAMEMAX, [&](AMX *amx, void *program)
	{
		info.amx = amx;
		luaL_ref(L, LUA_REGISTRYINDEX);

		lua_newtable(L);

		init_sleep(L, amx);
		init_native(L, amx);
		init_public(L, amx);
		init_memory(L, amx);
		init_string(L, amx);
		init_result(L, amx);
		init_tags(L, amx);

		lua_getfield(L, -1, "public");
		lua_pushlightuserdata(L, amx);
		lua_pushcclosure(L, forward, 2);
		lua_setfield(L, -2, "forward");

		lua::pushstring(L, "#lua");
		lua_setfield(L, -2, "loopback");
	});
	if(!amx)
	{
		lua_pop(L, 1);
		return 0;
	}

	return 1;
}
