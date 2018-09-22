#include "public.h"
#include "lua_utils.h"
#include "lua_api.h"
#include "sleep.h"

#include <unordered_map>
#include <memory>
#include <cstring>
#include <limits>

static std::unordered_map<AMX*, std::weak_ptr<struct amx_public_info>> amx_map;

struct amx_public_info
{
	AMX *amx;

	lua_State *L;
	int self;
	int publictable;
	int publiclist;
	int contlist;

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

	auto info = std::make_shared<amx_public_info>(lua::mainthread(L), amx);
	amx_map[amx] = info;
	lua::pushuserdata(L, info);

	lua_getfield(L, table, "public");
	info->publictable = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_newtable(L);
	info->publiclist = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_newtable(L);
	info->contlist = luaL_ref(L, LUA_REGISTRYINDEX);

	info->self = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool getpublic(lua_State *L, const char *name, int index, int &error)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		error = lua::pgetfield(L, -1, name);
		if(error != LUA_OK || !lua_isfunction(L, -1))
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

bool getpubliclist(lua_State *L, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::interop::amx_find_public(AMX *amx, const char *funcname, int *index, int &error)
{
	if(index)
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
				if(getpubliclist(L, info->publiclist))
				{
					bool indexed = false;
					if(lua_getfield(L, -1, funcname) == LUA_TNUMBER)
					{
						*index = (int)lua_tointeger(L, -1);
						indexed = true;
					}
					lua_pop(L, 1);
					int lerror;
					if(getpublic(L, funcname, info->publictable, lerror))
					{
						if(indexed)
						{
							if(lua_rawgeti(L, -2, *index) == LUA_TTABLE)
							{
								lua_insert(L, -2);
								lua_rawseti(L, -2, 1);
								lua_pop(L, 2);
								error = AMX_ERR_NONE;
								(*index)--;
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
							error = AMX_ERR_NONE;
							(*index)--;
							return true;
						}
						lua_pop(L, 1);
					}else if(lerror != LUA_OK)
					{
						error = AMX_ERR_GENERAL;
						return true;
					}
					if(indexed)
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
				error = AMX_ERR_INDEX;
				return true;
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
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 4))
				{
					return false;
				}
				if(getpubliclist(L, info->publiclist))
				{
					if(lua_rawgeti(L, -1, index + 1) == LUA_TTABLE)
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
				lua::stackguard guard(L);
				if(!lua_checkstack(L, 1))
				{
					return false;
				}
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
			lua::stackguard guard(L);
			if(!lua_checkstack(L, amx->paramcount + 5))
			{
				result = amx->error = AMX_ERR_MEMORY;
				return true;
			}
			bool cont = index == AMX_EXEC_CONT;
			if(cont || getpubliclist(L, info->publiclist))
			{
				int tt = cont ? lua_rawgeti(L, LUA_REGISTRYINDEX, info->contlist) : lua_rawgeti(L, -1, index + 1);
				if(tt == LUA_TTABLE)
				{
					if(cont)
					{
						tt = lua_rawgeti(L, -1, amx->cip);
						if(tt == LUA_TTABLE)
						{
							tt = lua_rawgeti(L, -1, 1);
							if(tt == LUA_TFUNCTION)
							{
								lua_rawgeti(L, -2, 2);
								auto cookie = lua_tointeger(L, -1);
								lua_pop(L, 1);
								if(amx->alt != cookie)
								{
									tt = LUA_TNIL;
								}else{
									luaL_unref(L, -3, amx->cip);
								}
								lua_remove(L, -2);
							}
						}else{
							tt = LUA_TNIL;
						}
					}else{
						tt = lua_rawgeti(L, -1, 1);
					}
					if(tt == LUA_TFUNCTION)
					{
						auto hdr = (AMX_HEADER*)amx->base;
						auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
						auto stk = reinterpret_cast<cell*>(data + amx->stk);
						cell reset_stk = amx->stk;
						int paramcount;
						if(cont)
						{
							paramcount = 1;
							lua_pushlightuserdata(L, reinterpret_cast<void*>(amx->pri));
						}else{
							paramcount = amx->paramcount;
							amx->paramcount = 0;
							for(int i = 0; i < paramcount; i++)
							{
								cell value = stk[i];
								lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
							}
							reset_stk += paramcount * sizeof(cell);
							amx->stk -= 3 * sizeof(cell);
							*--stk = paramcount * sizeof(cell);
							*--stk = 0;
							*--stk = 0;
							amx->frm = amx->stk;
						}

						int error = lua_pcall(L, paramcount, 1, 0);
						if(error == LUA_OK)
						{
							amx->cip = 0;
							amx->error = AMX_ERR_NONE;
							if(lua_isinteger(L, -1))
							{
								auto num = lua_tointeger(L, -1);
								if(num < std::numeric_limits<cell>::min() || num > std::numeric_limits<ucell>::max())
								{
									logprintf("warning: cannot marshal return value (%lld cannot be stored in a single cell)", (long long)num);
								}
								amx->pri = (cell)num;
							}else if(lua::isnumber(L, -1))
							{
								float num = (float)lua_tonumber(L, -1);
								amx->pri = amx_ftoc(num);
							}else if(lua_isboolean(L, -1))
							{
								amx->pri = lua_toboolean(L, -1);
							}else if(lua_islightuserdata(L, -1))
							{
								amx->pri = reinterpret_cast<cell>(lua_touserdata(L, -1));
							}else{
								if(!lua_isnil(L, -1))
								{
									logprintf("warning: cannot marshal return type %s", luaL_typename(L, -1));
								}
								amx->pri = 0;
							}
							if(retval)
							{
								*retval = amx->pri;
							}
							lua_pop(L, 1);
						}else{
							switch(error)
							{
								case LUA_ERRMEM:
									amx->error = AMX_ERR_MEMORY;
									break;
								case LUA_ERRRUN:
									amx->error = AMX_ERR_GENERAL;
									if(lua_istable(L, -1))
									{
										if(lua_getfield(L, -1, "__amxerr") == LUA_TNUMBER)
										{
											amx->error = (int)lua_tointeger(L, -1);
										}
										lua_pop(L, 1);
										lua::interop::handle_sleep(L, amx, info->contlist);
									}
									break;
								default:
									amx->error = AMX_ERR_GENERAL;
									break;
							}
							if(amx->error != AMX_ERR_SLEEP)
							{
								lua::report_error(L, error);
							}
							if(retval)
							{
								*retval = amx->pri;
							}
							lua_pop(L, 1);
						}
						amx->stk = reset_stk;
						lua_pop(L, cont ? 1 : 2);
						result = amx->error;
						return true;
					}
					lua_pop(L, 1);
				}
				lua_pop(L, cont ? 1 : 2);
			}
			if(cont)
			{
				if(amx->cip == 0)
				{
					logprintf("warning: no continuation was saved");
				}else{
					logprintf("warning: continuation cannot be found (incorrectly saved or restored by the host)");
				}
			}
			result = amx->error = AMX_ERR_INDEX;
			return true;
		}
	}
	return false;
}
