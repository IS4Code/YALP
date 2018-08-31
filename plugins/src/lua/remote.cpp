#include "remote.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>

static std::unordered_map<const void*, std::weak_ptr<struct lua_ref_info>> ref_map;

static char PROXYMT;

struct lua_ref_info
{
	lua_State *L;
	int self;
	int ptrtable;

	lua_ref_info(lua_State *L) : L(L)
	{

	}

	bool marshal(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller);

	bool gettable()
	{
		if(lua_rawgeti(L, LUA_REGISTRYINDEX, ptrtable) == LUA_TTABLE)
		{
			return true;
		}
		lua_pop(L, 1);
		return false;
	}

	std::shared_ptr<lua_ref_info> getself()
	{
		if(lua_rawgeti(L, LUA_REGISTRYINDEX, self) == LUA_TUSERDATA)
		{
			auto ptr = lua::touserdata< std::shared_ptr<lua_ref_info>>(L, -1);
			lua_pop(L, 1);
			return ptr;
		}
		lua_pop(L, 1);
		return {};
	}
};

struct lua_foreign_reference
{
	int obj = 0;
	std::shared_ptr<lua_ref_info> source;
	std::weak_ptr<lua_ref_info> remote_weak;

	lua_foreign_reference()
	{

	}

	~lua_foreign_reference()
	{
		if(obj)
		{
			if(auto remote = remote_weak.lock())
			{
				if(remote->gettable())
				{
					luaL_unref(remote->L, -1, obj);
					lua_pop(remote->L, 1);
				}
			}
			obj = 0;
		}
	}

	std::shared_ptr<lua_ref_info> connect()
	{
		if(obj && source)
		{
			if(auto remote = remote_weak.lock())
			{
				if(remote->gettable())
				{
					auto L2 = remote->L;
					lua_rawgeti(L2, -1, obj);
					lua_remove(L2, -2);
					return remote;
				}
			}
		}
		return {};
	}

	bool duplicate(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller)
	{
		if(auto remote = connect())
		{
			remote->marshal(to, marshaller);
			return true;
		}
		return false;
	}

	int index(lua_State *L)
	{
		if(auto remote = connect())
		{
			auto L2 = remote->L;
			int indexable = lua_absindex(L2, -1);

			lua_pushvalue(L, 2);
			source->marshal(L2, remote);
			int err = lua::pgettable(L2, indexable);
			remote->marshal(L, source);

			lua_remove(L2, indexable);

			if(err != LUA_OK)
			{
				return lua_error(L);
			}
			return 1;
		}
		return 0;
	}

	int newindex(lua_State *L)
	{
		if(auto remote = connect())
		{
			auto L2 = remote->L;
			int indexable = lua_absindex(L2, -1);

			lua_pushvalue(L, 2);
			source->marshal(L2, remote);
			lua_pushvalue(L, 3);
			source->marshal(L2, remote);
			int err = lua::psettable(L2, indexable);

			lua_remove(L2, indexable);

			if(err != LUA_OK)
			{
				remote->marshal(L, source);
				return lua_error(L);
			}
			return 0;
		}
		return 0;
	}

	int call(lua_State *L)
	{
		int args = lua_gettop(L);
		int numresults = lua::numresults(L);
		if(auto remote = connect())
		{
			auto L2 = remote->L;
			int callable = lua_absindex(L2, -1);
			int top = lua_gettop(L2) - 1;

			for(int i = 2; i <= args; i++)
			{
				lua_pushvalue(L, i);
				source->marshal(L2, remote);
			}

			if(lua_pcall(L2, args - 1, numresults, 0) != LUA_OK)
			{
				remote->marshal(L, source);
				return lua_error(L);
			}

			numresults = lua_gettop(L2) - top;

			for(int i = 1; i <= numresults; i++)
			{
				lua_pushvalue(L2, top + i);
				remote->marshal(L, source);
			}

			if(L != L2)
			{
				lua_settop(L2, top);
			}

			return numresults;
		}
		return 0;
	}

	int eq(lua_State *L)
	{
		if(auto remote = connect())
		{
			if(lua_getmetatable(L, 2))
			{
				bool isproxy = lua_rawgetp(L, -1, &PROXYMT) != LUA_TNIL;
				lua_pop(L, 2);

				if(isproxy)
				{
					auto &ref = lua::touserdata<lua_foreign_reference>(L, 2);
					
					if(auto remote2 = ref.connect())
					{
						if(remote != remote2 || remote->L != remote2->L)
						{
							lua_pop(remote2->L, 1);
						}else{
							auto L2 = remote->L;
							int err = lua::pcompare(L2, -2, -1, LUA_OPEQ);
							remote->marshal(L, source);

							if(L2 != L)
							{
								lua_pop(L2, 2);
							}

							if(err != LUA_OK)
							{
								return lua_error(L);
							}
							return 1;
						}
					}
				}
			}
			lua_pop(remote->L, 1);
		}
		lua_pushboolean(L, false);
		return 1;
	}
};

bool lua_ref_info::marshal(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller)
{
	if(L == to)
	{
		return true;
	}
	if(L == lua::mainthread(to))
	{
		lua_xmove(L, to, 1);
		return true;
	}
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, ptrtable) != LUA_TTABLE)
	{
		return false;
	}
	lua_insert(L, -2);
	int t = lua_absindex(L, -2);
	int top = lua_absindex(L, -1);
	switch(lua_type(L, top))
	{
		case LUA_TSTRING:
		{
			size_t len;
			auto str = lua_tolstring(L, top, &len);
			lua_pushlstring(to, str, len);
			lua_remove(L, top);
			break;
		}
		case LUA_TNUMBER:
		{
			if(lua_isinteger(L, top))
			{
				lua_pushinteger(to, lua_tointeger(L, top));
			}else{
				lua_pushnumber(to, lua_tonumber(L, top));
			}
			lua_remove(L, top);
			break;
		}
		case LUA_TNIL:
		{
			lua_pushnil(to);
			lua_remove(L, top);
			break;
		}
		case LUA_TLIGHTUSERDATA:
		{
			lua_pushlightuserdata(to, lua_touserdata(L, top));
			lua_remove(L, top);
			break;
		}
		case LUA_TBOOLEAN:
		{
			lua_pushboolean(to, lua_toboolean(L, top));
			lua_remove(L, top);
			break;
		}
		case LUA_TUSERDATA:
		{
			if(lua_getmetatable(L, top))
			{
				bool isproxy = lua_rawgetp(L, -1, &PROXYMT) != LUA_TNIL;
				lua_pop(L, 2);

				if(isproxy)
				{
					auto &ref = lua::touserdata<lua_foreign_reference>(L, top);
					if(ref.duplicate(to, marshaller))
					{
						lua_remove(L, top);
						break;
					}
				}
			}
			//goto case default;
		}
		default:
		{
			int obj = luaL_ref(L, t);
			auto &ref = lua::newuserdata<lua_foreign_reference>(to);
			ref.obj = obj;
			ref.source = marshaller;
			ref.remote_weak = getself();

			if(lua_getmetatable(to, -1))
			{
				lua_pushboolean(to, true);
				lua_rawsetp(to, -2, &PROXYMT);
				lua_pushstring(to, "proxy");
				lua_setfield(to, -2, "__name");
				lua_pushcfunction(to, [](lua_State *L)
				{
					return lua::touserdata<lua_foreign_reference>(L, 1).index(L);
				});
				lua_setfield(to, -2, "__index");
				lua_pushcfunction(to, [](lua_State *L)
				{
					return lua::touserdata<lua_foreign_reference>(L, 1).newindex(L);
				});
				lua_setfield(to, -2, "__newindex");
				lua_pushcfunction(to, [](lua_State *L)
				{
					return lua::touserdata<lua_foreign_reference>(L, 1).call(L);
				});
				lua_setfield(to, -2, "__call");
				lua_pushcfunction(to, [](lua_State *L)
				{
					return lua::touserdata<lua_foreign_reference>(L, 1).eq(L);
				});
				lua_setfield(to, -2, "__eq");
				lua_pop(to, 1);
			}
			break;
		}
	}
	lua_remove(L, t);
	return true;
}

int _register(lua_State *L)
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

	auto &handle = lua::touserdata<std::shared_ptr<lua_ref_info>>(L, lua_upvalueindex(1));
	ref_map[ptr] = handle;
	lua_pushlightuserdata(L, const_cast<void*>(ptr));
	return 1;
}

int get(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	bool remove = luaL_opt(L, lua::checkboolean, 2, false);
	auto it = ref_map.find(ptr);
	if(it != ref_map.end())
	{
		auto obj = it->second.lock();
		if(remove)
		{
			ref_map.erase(it);
		}
		if(obj)
		{
			auto L2 = obj->L;
			if(lua_rawgeti(L2, LUA_REGISTRYINDEX, obj->ptrtable) == LUA_TTABLE)
			{
				int table = lua_absindex(L2, -1);

				lua_rawgetp(L2, -1, ptr);
				obj->marshal(L, lua::touserdata<std::shared_ptr<lua_ref_info>>(L, lua_upvalueindex(1)));

				if(remove)
				{
					lua_pushnil(L2);
					lua_rawsetp(L2, table, ptr);
				}
				if(L != L2)
				{
					lua_pop(L2, 1);
				}
				return 1;
			}
			lua_pop(L2, 1);
		}
	}
	lua_pushnil(L);
	return 1;
}

int unregister(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	auto it = ref_map.find(ptr);
	if(it != ref_map.end())
	{
		auto obj = it->second.lock();
		ref_map.erase(it);
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

	auto ptr = std::make_shared<lua_ref_info>(lua::mainthread(L));
	lua::pushuserdata(L, ptr);
	ptr->self = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_newtable(L);
	ptr->ptrtable = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->self);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->ptrtable);
	lua_pushcclosure(L, _register, 2);
	lua_setfield(L, table, "register");

	lua_rawgeti(L, LUA_REGISTRYINDEX, ptr->self);
	lua_pushcclosure(L, get, 1);
	lua_setfield(L, table, "get");

	lua_pushcfunction(L, unregister);
	lua_setfield(L, table, "unregister");

	return 1;
}

void lua::remote::close()
{
	ref_map.clear();
}
