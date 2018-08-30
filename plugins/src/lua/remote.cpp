#include "remote.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>

std::unordered_map<const void*, std::weak_ptr<struct lua_handle>> ptr_map;

struct lua_handle
{
	lua_State *L;
	int self;
	int ptrtable;

	lua_handle(lua_State *L) : L(L)
	{

	}
};

int reg(lua_State *L)
{
	auto ptr = lua_topointer(L, 1);
	if(!ptr && lua::isstring(L, 1))
	{
		ptr = lua_tostring(L, 1);
	}
	if(!ptr || lua_islightuserdata(L, 1))
	{
		return lua::argerrortype(L, 1, "reference type");
	}
	lua_pushvalue(L, 1);
	lua_rawsetp(L, lua_upvalueindex(2), ptr);

	auto &handle = lua::touserdata<std::shared_ptr<lua_handle>>(L, lua_upvalueindex(1));
	ptr_map[ptr] = handle;
	lua_pushlightuserdata(L, const_cast<void*>(ptr));
	return 1;
}

int getref(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	bool remove = luaL_opt(L, lua::checkboolean, 2, false);
	auto it = ptr_map.find(ptr);
	if(it != ptr_map.end())
	{
		auto obj = it->second.lock();
		if(remove)
		{
			ptr_map.erase(it);
		}
		if(obj)
		{
			auto L2 = obj->L;
			if(lua_rawgeti(L2, LUA_REGISTRYINDEX, obj->ptrtable) == LUA_TTABLE)
			{
				if(L == L2)
				{
					lua_rawgetp(L, -1, ptr);
					if(remove)
					{
						lua_pushnil(L);
						lua_rawsetp(L, -3, ptr);
					}
					return 1;
				}
				switch(lua_rawgetp(L2, -1, ptr))
				{
					case LUA_TSTRING:
					{
						size_t len;
						auto str = lua_tolstring(L2, -1, &len);
						lua_pushlstring(L, str, len);
						break;
					}
					default:
					{
						lua_pushnil(L);
						break;
					}
				}
				if(remove)
				{
					lua_pushnil(L2);
					lua_rawsetp(L2, -3, ptr);
				}
				lua_pop(L2, 2);
				return 1;
			}
			lua_pop(L2, 1);
		}
	}
	lua_pushnil(L);
	return 1;
}

int unreg(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	auto it = ptr_map.find(ptr);
	if(it != ptr_map.end())
	{
		auto obj = it->second.lock();
		ptr_map.erase(it);
		if(obj)
		{
			auto L2 = obj->L;
			if(lua_rawgeti(L2, LUA_REGISTRYINDEX, obj->ptrtable) == LUA_TTABLE)
			{
				lua_pushnil(L2);
				lua_rawsetp(L2, -2, ptr);
			}
			lua_pop(L2, 1);
			lua_pushboolean(L, true);
			return 1;
		}
	}
	lua_pushboolean(L, false);
	return 1;
}

int lua::remote::loader(lua_State *L)
{
	lua_createtable(L, 0, 2);
	int table = lua_absindex(L, -1);

	auto ptr = std::make_shared<lua_handle>(L);
	lua::pushuserdata(L, ptr);
	ptr->self = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_newtable(L);
	ptr->ptrtable = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->self);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->ptrtable);
	lua_pushcclosure(L, reg, 2);
	lua_setfield(L, table, "register");

	lua_pushcfunction(L, getref);
	lua_setfield(L, table, "get");

	lua_pushcfunction(L, unreg);
	lua_setfield(L, table, "unregister");

	return 1;
}

void lua::remote::close()
{
	ptr_map.clear();
}
