#include "public.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>
#include <cstring>

static std::unordered_map<AMX*, std::weak_ptr<struct amx_public_info>> amx_map;

struct amx_public_info
{
	AMX *amx;

	lua_State *L;
	int self;
	int publictable;
	int publiclist;

	amx_public_info(lua_State *L, AMX *amx) : L(L), amx(amx)
	{

	}

	~amx_public_info()
	{
		if(amx)
		{
			amx_map.erase(amx);
			amx = nullptr;
		}
	}
};

void lua::interop::init_public(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	auto info = std::make_shared<amx_public_info>(L, amx);
	amx_map[amx] = info;
	lua::pushuserdata(L, info);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, table, "public");
	info->publictable = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_newtable(L);
	info->publiclist = luaL_ref(L, LUA_REGISTRYINDEX);

	info->self = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool getpublic(lua_State *L, const char *name, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		if(lua_getfield(L, -1, name) == LUA_TFUNCTION)
		{
			lua_remove(L, -2);
			return true;
		}
		lua_pop(L, 2);
		return false;
	}
	lua_pop(L, 1);
	return false;
}

bool getpubliclist(lua_State *L, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::interop::amx_find_public(AMX *amx, const char *funcname, int *index)
{
	if(index)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(getpubliclist(L, info->publiclist))
				{
					bool indexed = false;
					if(lua_getfield(L, -1, funcname) == LUA_TNUMBER)
					{
						*index = (int)lua_tointeger(L, -1);
						indexed = true;
					}
					lua_pop(L, 1);
					if(getpublic(L, funcname, info->publictable))
					{
						if(indexed)
						{
							if(lua_rawgeti(L, -2, *index) == LUA_TTABLE)
							{
								lua_insert(L, -2);
								lua_rawseti(L, -2, 1);
								lua_pop(L, 2);
								return true;
							}
							lua_pop(L, 1);
						}else{
							lua_createtable(L, 2, 0);
							lua_insert(L, -2);
							lua_rawseti(L, -2, 1);
							lua_pushstring(L, funcname);
							lua_rawseti(L, -2, 2);
							*index = luaL_ref(L, -2);
							lua_pushinteger(L, *index);
							lua_setfield(L, -2, funcname);
							lua_pop(L, 1);
							return true;
						}
						lua_pop(L, 1);
					}else if(indexed)
					{
						if(lua_rawgeti(L, -1, *index) == LUA_TTABLE)
						{
							lua_pushnil(L);
							lua_rawseti(L, -2, 1);
						}
						lua_pop(L, 1);
					}
					lua_pop(L, 1);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_get_public(AMX *amx, int index, char *funcname)
{
	if(funcname)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(getpubliclist(L, info->publiclist))
				{
					if(lua_rawgeti(L, -1, index) == LUA_TTABLE)
					{
						if(lua_rawgeti(L, -1, 2) == LUA_TSTRING)
						{
							auto str = lua_tostring(L, -1);
							std::strcpy(funcname, str);
							lua_pop(L, 3);
							return true;
						}
						lua_pop(L, 1);
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_num_publics(AMX *amx, int *number)
{
	if(number)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(getpubliclist(L, info->publiclist))
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

bool lua::interop::amx_exec(AMX *amx, cell *retval, int index, int &result)
{
	auto it = amx_map.find(amx);
	if(it != amx_map.end())
	{
		if(auto info = it->second.lock())
		{
			auto L = info->L;
			if(getpubliclist(L, info->publiclist))
			{
				if(lua_rawgeti(L, -1, index) == LUA_TTABLE)
				{
					if(lua_rawgeti(L, -1, 1) == LUA_TFUNCTION)
					{
						auto hdr = (AMX_HEADER*)amx->base;
						auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
						auto stk = reinterpret_cast<cell*>(data + amx->stk);
						int paramcount = amx->paramcount;
						amx->paramcount = 0;
						for(int i = 0; i < paramcount; i++)
						{
							cell value = stk[i];
							lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
						}
						cell reset_stk = amx->stk + paramcount * sizeof(cell);
						amx->stk -= 3 * sizeof(cell);
						*--stk = paramcount * sizeof(cell);
						*--stk = 0;
						*--stk = 0;
						amx->frm = amx->stk;

						switch(lua_pcall(L, paramcount, 1, 0))
						{
							case LUA_OK:
								amx->error = AMX_ERR_NONE;
								if(retval)
								{
									if(lua_isinteger(L, -1))
									{
										*retval = (cell)lua_tointeger(L, -1);
									}else if(lua_isnumber(L, -1))
									{
										float num = (float)lua_tonumber(L, -1);
										*retval = amx_ftoc(num);
									}else if(lua_isboolean(L, -1))
									{
										*retval = lua_toboolean(L, -1);
									}else if(lua_islightuserdata(L, -1))
									{
										*retval = reinterpret_cast<cell>(lua_touserdata(L, -1));
									}else{
										*retval = 0;
									}
								}
								break;
							case LUA_ERRMEM:
								lua_pop(L, 1);
								amx->error = AMX_ERR_MEMORY;
								break;
							default:
								lua_pop(L, 1);
								amx->error = AMX_ERR_GENERAL;
								break;
						}
						amx->stk = reset_stk;
						lua_pop(L, 3);
						result = amx->error;
						return true;
					}
					lua_pop(L, 1);
				}
				lua_pop(L, 2);
			}
			result = amx->error = AMX_ERR_INDEX;
			return true;
		}
	}
	return false;
}
