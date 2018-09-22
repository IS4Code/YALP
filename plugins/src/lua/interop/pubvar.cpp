#include "pubvar.h"
#include "lua_utils.h"
#include "lua_api.h"

#include <unordered_map>
#include <memory>
#include <cstring>

static std::unordered_map<AMX*, std::weak_ptr<struct amx_pubvar_info>> amx_map;
static std::unordered_map<cell, std::weak_ptr<cell>> addr_map;

struct amx_pubvar_info
{
	AMX *amx;

	lua_State *L;
	int self;
	int pubvartable;
	int pubvarlist;

	amx_pubvar_info(lua_State *L, AMX *amx) : L(L), amx(amx)
	{

	}

	~amx_pubvar_info()
	{
		if(amx)
		{
			amx_map.erase(amx);
			amx = nullptr;
		}
	}
};

void lua::interop::init_pubvar(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	auto info = std::make_shared<amx_pubvar_info>(lua::mainthread(L), amx);
	amx_map[amx] = info;
	lua::pushuserdata(L, info);

	lua_getfield(L, table, "public");
	info->pubvartable = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_newtable(L);
	info->pubvarlist = luaL_ref(L, LUA_REGISTRYINDEX);

	info->self = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool getpubvar(lua_State *L, const char *name, int index, int &error, void *&buf)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		error = lua::pgetfield(L, -1, name);
		size_t length;
		bool isconst;
		if(error != LUA_OK || !(buf = lua::tobuffer(L, -1, length, isconst)))
		{
			if(error != LUA_OK)
			{
				lua::report_error(L, error);
			}
			lua_pop(L, 2);
			return false;
		}
		lua_remove(L, -2);
		return true;
	}
	error = LUA_OK;
	lua_pop(L, 1);
	return false;
}

bool getpubvarlist(lua_State *L, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::interop::amx_find_pubvar(AMX *amx, const char *varname, cell *amx_addr, int &error)
{
	if(amx_addr)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					error = AMX_ERR_MEMORY;
					return true;
				}
				if(getpubvarlist(L, info->pubvarlist))
				{
					int index = 0;
					if(lua_getfield(L, -1, varname) == LUA_TNUMBER)
					{
						index = (int)lua_tointeger(L, -1);
					}
					lua_pop(L, 1);
					int lerror;
					void *buf;
					if(getpubvar(L, varname, info->pubvartable, lerror, buf))
					{
						auto hdr = (AMX_HEADER*)amx->base;
						auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
						cell addr = *amx_addr = reinterpret_cast<unsigned char*>(buf) - data;
						auto lock = addr_map[addr].lock();
						if(!lock)
						{
							lock = std::make_shared<cell>(addr);
							addr_map[addr] = lock;
						}
						if(index)
						{
							if(lua_rawgeti(L, -2, index) == LUA_TTABLE)
							{
								lua_insert(L, -2);
								lua_rawseti(L, -2, 1);
								lua::pushuserdata(L, std::move(lock));
								lua_rawseti(L, -2, 3);
								lua_pop(L, 2);
								error = AMX_ERR_NONE;
								return true;
							}
							lua_pop(L, 1);
						}else{
							lua_createtable(L, 3, 0);
							lua_insert(L, -2);
							lua_rawseti(L, -2, 1);
							lua_pushstring(L, varname);
							lua_rawseti(L, -2, 2);
							lua::pushuserdata(L, std::move(lock));
							lua_rawseti(L, -2, 3);
							int index = luaL_ref(L, -2);
							lua_pushinteger(L, index);
							lua_setfield(L, -2, varname);
							lua_pop(L, 1);
							error = AMX_ERR_NONE;
							return true;
						}
						lua_pop(L, 1);
					}else if(lerror != LUA_OK)
					{
						error = AMX_ERR_GENERAL;
						return true;
					}
					if(index)
					{
						if(lua_rawgeti(L, -1, index) == LUA_TTABLE)
						{
							lua_pushnil(L);
							lua_rawseti(L, -2, 1);
							lua_pushnil(L);
							lua_rawseti(L, -2, 3);
						}
						lua_pop(L, 1);
					}
					lua_pop(L, 1);
				}
				error = AMX_ERR_INDEX;
				return true;
			}
		}
	}
	return false;
}

bool lua::interop::amx_get_pubvar(AMX *amx, int index, char *varname, cell *amx_addr)
{
	if(varname || amx_addr)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(getpubvarlist(L, info->pubvarlist))
				{
					if(lua_rawgeti(L, -1, index + 1) == LUA_TTABLE)
					{
						if(amx_addr)
						{
							if(lua_rawgeti(L, -1, 3) == LUA_TUSERDATA)
							{
								*amx_addr = *lua::touserdata<std::shared_ptr<cell>>(L, -1);
							}
							lua_pop(L, 1);
						}
						if(varname)
						{
							if(lua_rawgeti(L, -1, 2) == LUA_TSTRING)
							{
								auto str = lua_tostring(L, -1);
								std::strcpy(varname, str);
							}
							lua_pop(L, 1);
							lua_pop(L, 2);
						}
						return true;
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_num_pubvars(AMX *amx, int *number)
{
	if(number)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(getpubvarlist(L, info->pubvarlist))
				{
					*number = (int)lua_rawlen(L, -1);
					lua_pop(L, 1);
					return true;
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_get_pubvar_addr(AMX *amx, cell amx_addr, cell **phys_addr)
{
	if(phys_addr)
	{
		auto it = addr_map.find(amx_addr);
		if(it != addr_map.end())
		{
			auto lock = it->second.lock();
			if(!lock)
			{
				addr_map.erase(it);
				return false;
			}

			auto hdr = (AMX_HEADER*)amx->base;
			auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
			*phys_addr = reinterpret_cast<cell*>(data + amx_addr);
			return true;
		}
	}
	return false;
}
