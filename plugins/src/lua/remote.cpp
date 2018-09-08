#include "remote.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>

struct lua_registered_ref
{
	int count = 0;
	std::weak_ptr<struct lua_ref_info> info;
};

static std::unordered_map<const void*, lua_registered_ref> ref_map;

struct lua_ref_info
{
	lua_State *L;
	int self;
	int ptrtable;

	lua_ref_info(lua_State *L) : L(L)
	{

	}

	bool marshal(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller, bool noproxy = false);

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

static const char PROXYMTKEY = 0;

bool isproxy(lua_State *L, int idx)
{
	if(lua_getmetatable(L, idx))
	{
		lua_rawgetp(L, LUA_REGISTRYINDEX, &PROXYMTKEY);
		bool isproxy = lua_rawequal(L, -2, -1);
		lua_pop(L, 2);
		return isproxy;
	}
	return false;
}

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

	std::shared_ptr<lua_ref_info> connect(lua_State *from)
	{
		if(obj && source)
		{
			if(auto remote = remote_weak.lock())
			{
				auto L2 = remote->L;
				if(!lua_checkstack(L2, 4))
				{
					luaL_error(from, "stack overflow");
					return {};
				}
				if(remote->gettable())
				{
					lua_rawgeti(L2, -1, obj);
					lua_remove(L2, -2);
					return remote;
				}
			}
		}
		return {};
	}

	bool duplicate(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller, bool noproxy, lua_State *from)
	{
		if(auto remote = connect(from))
		{
			return remote->marshal(to, marshaller, noproxy);
		}
		return false;
	}

	int index(lua_State *L)
	{
		if(auto remote = connect(L))
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
				return lua::error(L);
			}
			return 1;
		}
		return 0;
	}

	int newindex(lua_State *L)
	{
		if(auto remote = connect(L))
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
				return lua::error(L);
			}
			return 0;
		}
		return 0;
	}

	int call(lua_State *L)
	{
		int args = lua_gettop(L);
		int numresults = lua::numresults(L);
		if(auto remote = connect(L))
		{
			auto L2 = remote->L;
			int callable = lua_absindex(L2, -1);
			int top = lua_gettop(L2) - 1;

			if(!lua_checkstack(L2, args + 4))
			{
				lua_pop(L2, 1);
				return luaL_error(L, "stack overflow");
			}
			for(int i = 2; i <= args; i++)
			{
				lua_pushvalue(L, i);
				source->marshal(L2, remote);
			}

			if(lua_pcall(L2, args - 1, numresults, 0) != LUA_OK)
			{
				remote->marshal(L, source);
				return lua::error(L);
			}

			numresults = lua_gettop(L2) - top;
			
			if(!lua_checkstack(L, args + 4))
			{
				lua_settop(L2, top);
				return luaL_error(L, "stack overflow");
			}
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
		return luaL_error(L, "the proxy is dead");
	}

	int len(lua_State *L)
	{
		if(auto remote = connect(L))
		{
			auto L2 = remote->L;
			int obj = lua_absindex(L2, -1);

			int err = lua::plen(L2, obj);
			remote->marshal(L, source);

			lua_remove(L2, obj);

			if(err != LUA_OK)
			{
				return lua::error(L);
			}
			return 1;
		}
		return luaL_error(L, "the proxy is dead");
	}

	template <int Op>
	int cmp(lua_State *L, int idx)
	{
		if(auto remote = connect(L))
		{
			auto L2 = remote->L;
			int top = lua_gettop(L);
			if(!lua_checkstack(L2, top + 4))
			{
				return luaL_error(L, "stack overflow");
			}
			int err;
			if(idx == 2)
			{
				lua_pop(L, 1);
				if(!source->marshal(L2, remote, true))
				{
					lua_pop(L2, 1);
					if(Op == LUA_OPEQ)
					{
						return 0;
					}else{
						return luaL_argerror(L, 1, "requires creating a proxy");
					}
				}
				err = lua::pcompare(L2, -1, -2, Op);
			}else{
				if(!source->marshal(L2, remote, true))
				{
					lua_pop(L2, 1);
					if(Op == LUA_OPEQ)
					{
						return 0;
					}else{
						return luaL_argerror(L, 2, "requires creating a proxy");
					}
				}
				err = lua::pcompare(L2, -2, -1, Op);
			}
			remote->marshal(L, source);

			if(L2 != L)
			{
				lua_pop(L2, 2);
			}

			if(err != LUA_OK)
			{
				return lua::error(L);
			}
			return 1;
		}
		if(Op == LUA_OPEQ)
		{
			lua_pushboolean(L, false);
			return 1;
		}
		return luaL_error(L, "the proxy is dead");
	}

	template <int Op>
	int arith(lua_State *L, int idx)
	{
		if(auto remote = connect(L))
		{
			auto L2 = remote->L;
			int top = lua_gettop(L);
			if(!lua_checkstack(L2, top + 4))
			{
				return luaL_error(L, "stack overflow");
			}
			int obj = lua_absindex(L2, -1);
			for(int i = 1; i < idx; i++)
			{
				lua_pushvalue(L, i);
				if(!source->marshal(L2, remote, true))
				{
					lua_settop(L2, obj - 1);
					return luaL_argerror(L, i, "requires creating a proxy");
				}
			}
			lua_rotate(L2, obj, idx - 1);
			for(int i = idx + 1; i <= top; i++)
			{
				lua_pushvalue(L, i);
				if(!source->marshal(L2, remote, true))
				{
					lua_settop(L2, obj - 1);
					return luaL_argerror(L, i, "requires creating a proxy");
				}
			}

			int err = lua::parith(L2, Op);
			remote->marshal(L, source);

			if(err != LUA_OK)
			{
				return lua::error(L);
			}
			return 1;
		}
		return luaL_error(L, "the proxy is dead");
	}

	int concat(lua_State *L, int idx)
	{
		if(auto remote = connect(L))
		{
			auto L2 = remote->L;
			int top = lua_gettop(L);
			if(!lua_checkstack(L2, top + 4))
			{
				return luaL_error(L, "stack overflow");
			}
			int obj = lua_absindex(L2, -1);
			for(int i = 1; i < idx; i++)
			{
				lua_pushvalue(L, i);
				if(!source->marshal(L2, remote, true))
				{
					lua_settop(L2, obj - 1);
					return luaL_argerror(L, i, "requires creating a proxy");
				}
			}
			lua_rotate(L2, obj, idx - 1);
			for(int i = idx + 1; i <= top; i++)
			{
				lua_pushvalue(L, i);
				if(!source->marshal(L2, remote, true))
				{
					lua_settop(L2, obj - 1);
					return luaL_argerror(L, i, "requires creating a proxy");
				}
			}
			
			int err = lua::pconcat(L2, top);
			remote->marshal(L, source);

			if(err != LUA_OK)
			{
				return lua::error(L);
			}
			return 1;
		}
		return luaL_error(L, "the proxy is dead");
	}

	template <int (lua_foreign_reference::*Method)(lua_State *L)>
	static int op(lua_State *L)
	{
		return (lua::touserdata<lua_foreign_reference>(L, 1).*Method)(L);
	}

	template <int (lua_foreign_reference::*Method)(lua_State *L, int idx)>
	static int opcheck(lua_State *L)
	{
		int idx = 0;
		for(int i = 1; i <= lua_gettop(L); i++)
		{
			if(isproxy(L, i))
			{
				idx = i;
				break;
			}
		}
		return (lua::touserdata<lua_foreign_reference>(L, idx).*Method)(L, idx);
	}
};

namespace lua
{
	template <>
	struct mt_ctor<lua_foreign_reference>
	{
		bool operator()(lua_State *L)
		{
			if(lua_rawgetp(L, LUA_REGISTRYINDEX, &PROXYMTKEY) == LUA_TTABLE)
			{
				return true;
			}
			lua_pop(L, 1);

			lua_createtable(L, 0, 24 + 2);
			lua_pushvalue(L, -1);
			lua_rawsetp(L, LUA_REGISTRYINDEX, &PROXYMTKEY);

			lua_pushstring(L, "proxy");
			lua_setfield(L, -2, "__name");
			lua_pushcfunction(L, lua_foreign_reference::op<&lua_foreign_reference::index>);
			lua_setfield(L, -2, "__index");
			lua_pushcfunction(L, lua_foreign_reference::op<&lua_foreign_reference::newindex>);
			lua_setfield(L, -2, "__newindex");
			lua_pushcfunction(L, lua_foreign_reference::op<&lua_foreign_reference::call>);
			lua_setfield(L, -2, "__call");
			lua_pushcfunction(L, lua_foreign_reference::op<&lua_foreign_reference::len>);
			lua_setfield(L, -2, "__len");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::concat>);
			lua_setfield(L, -2, "__concat");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::cmp<LUA_OPEQ>>);
			lua_setfield(L, -2, "__eq");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::cmp<LUA_OPLT>>);
			lua_setfield(L, -2, "__lt");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::cmp<LUA_OPLE>>);
			lua_setfield(L, -2, "__le");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPADD>>);
			lua_setfield(L, -2, "__add");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPSUB>>);
			lua_setfield(L, -2, "__sub");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPMUL>>);
			lua_setfield(L, -2, "__mul");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPDIV>>);
			lua_setfield(L, -2, "__div");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPMOD>>);
			lua_setfield(L, -2, "__mod");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPPOW>>);
			lua_setfield(L, -2, "__pow");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPUNM>>);
			lua_setfield(L, -2, "__unm");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPIDIV>>);
			lua_setfield(L, -2, "__idiv");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPBAND>>);
			lua_setfield(L, -2, "__band");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPBOR>>);
			lua_setfield(L, -2, "__bor");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPBXOR>>);
			lua_setfield(L, -2, "__bxor");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPBNOT>>);
			lua_setfield(L, -2, "__bnot");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPSHL>>);
			lua_setfield(L, -2, "__shl");
			lua_pushcfunction(L, lua_foreign_reference::opcheck<&lua_foreign_reference::arith<LUA_OPSHR>>);
			lua_setfield(L, -2, "__shr");

			return true;
		}
	};
}

bool lua_ref_info::marshal(lua_State *to, const std::shared_ptr<lua_ref_info> &marshaller, bool noproxy)
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
		lua_pop(L, 1);
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
			if(isproxy(L, top))
			{
				auto &ref = lua::touserdata<lua_foreign_reference>(L, top);
				if(ref.duplicate(to, marshaller, noproxy, L))
				{
					lua_remove(L, top);
					break;
				}
			}
			//goto case default;
		}
		default:
		{
			if(noproxy)
			{
				lua_remove(L, t);
				return false;
			}
			int obj = luaL_ref(L, t);
			auto &ref = lua::newuserdata<lua_foreign_reference>(to);
			ref.obj = obj;
			ref.source = marshaller;
			ref.remote_weak = getself();
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
	auto &ref = ref_map[ptr];
	if(ref.info.expired())
	{
		ref.count = 0;
	}
	ref.info = handle;
	ref.count++;
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
		auto &reg = it->second;

		auto obj = reg.info.lock();
		if(remove)
		{
			if(--reg.count <= 0)
			{
				ref_map.erase(it);
			}else{
				remove = false;
			}
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
		auto &reg = it->second;

		auto obj = reg.info.lock();
		if(--reg.count <= 0)
		{
			ref_map.erase(it);
		}else{
			lua_pushboolean(L, true);
			return 1;
		}
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

	lua_pushcfunction(L, [](lua_State *L)
	{
		lua_pushboolean(L, isproxy(L, 1));
		return 1;
	});
	lua_setfield(L, table, "isproxy");

	lua_pushcfunction(L, [](lua_State *L)
	{
		if(!isproxy(L, 1)) return lua::argerrortype(L, 1, "proxy");
		auto &proxy = lua::touserdata<lua_foreign_reference>(L, 1);
		if(auto remote = proxy.connect(L))
		{
			lua_pop(remote->L, 1);
			lua_pushboolean(L, true);
		}else{
			lua_pushboolean(L, false);
		}
		return 1;
	});
	lua_setfield(L, table, "isalive");

	return 1;
}

void lua::remote::close()
{
	ref_map.clear();
}
