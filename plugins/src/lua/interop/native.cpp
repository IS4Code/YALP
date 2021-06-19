#include "native.h"
#include "lua_utils.h"
#include "amx/amxutils.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <limits>
#include <vector>
#include <cstring>

static std::unordered_map<AMX*, std::shared_ptr<struct amx_native_info>> amx_map;
static std::unordered_set<cell> addr_set;

struct amx_native_info
{
	AMX *amx;
	std::unordered_map<std::string, AMX_NATIVE> natives;

	amx_native_info(AMX *amx) : amx(amx)
	{

	}
};

class amx_stackguard
{
	AMX *amx;
	cell hea, stk;

public:
	std::vector<cell> reset_addr;

	amx_stackguard(AMX *amx) : amx(amx), hea(amx->hea), stk(amx->stk)
	{

	}

	~amx_stackguard()
	{
		amx->hea = hea;
		amx->stk = stk;

		for(const cell &value : reset_addr)
		{
			addr_set.erase(value);
		}
	}
};

int __call(lua_State *L)
{
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto native = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	if(native)
	{
		int errorcode;
		cell result;
		bool castresult = false;

		{
			amx_stackguard amx_guard(amx);
			auto hdr = (AMX_HEADER*)amx->base;
			auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;

			std::unique_ptr<std::unordered_map<int, std::pair<cell*, std::vector<void(*)(lua_State *L, cell value)>>>> restorers;
			
			int paramcount = 0;
			size_t len;
			bool isconst;
			for(int i = lua_gettop(L); i >= 1; i--)
			{
				if(!amx::MemCheck(amx, 0))
				{
					return lua::amx_error(L, AMX_ERR_STACKERR);
				}

				cell value = 0;
			
				if(lua_isinteger(L, i))
				{
					auto num = lua_tointeger(L, i);
					if(num < std::numeric_limits<cell>::min() || num > std::numeric_limits<ucell>::max())
					{
						return lua::argerror(L, i, "%I cannot be stored in a single cell", num);
					}
					value = (cell)num;
				}else if(lua::isnumber(L, i))
				{
					float num = (float)lua_tonumber(L, i);
					value = amx_ftoc(num);
				}else if(lua_isboolean(L, i))
				{
					value = lua_toboolean(L, i);
				}else if(lua::isstring(L, i))
				{
					size_t len;
					auto str = lua_tolstring(L, i, &len);
					auto dlen = ((len + sizeof(cell)) / sizeof(cell)) * sizeof(cell);

					if(!amx::MemCheck(amx, len * sizeof(cell)))
					{
						return lua::amx_error(L, AMX_ERR_MEMORY);
					}

					value = amx->hea;
					auto addr = reinterpret_cast<cell*>(data + amx->hea);
					amx::SetString(addr, str, len, true);
					amx->hea += dlen;
				}else if(i == 1 && lua_isfunction(L, i))
				{
					castresult = true;
					continue;
				}else if(auto buf = lua::tobuffer(L, i, len, isconst))
				{
					if(isconst)
					{
						if(!amx::MemCheck(amx, len))
						{
							return lua::amx_error(L, AMX_ERR_MEMORY);
						}
						value = amx->hea;
						auto addr = data + amx->hea;
						std::memcpy(addr, buf, len);
						amx->hea += len;
					}else{
						value = reinterpret_cast<unsigned char*>(buf) - data;
						if(value < 0 || value >= amx->stp)
						{
							if(addr_set.insert(value).second)
							{
								amx_guard.reset_addr.push_back(value);
							}
						}
					}
				}else if(lua_islightuserdata(L, i))
				{
					value = reinterpret_cast<cell>(lua_touserdata(L, i));
				}else if(lua_istable(L, i))
				{
					lua_len(L, i);
					int isnum;
					auto len = lua_tointegerx(L, -1, &isnum);
					lua_pop(L, 1);
					if(!isnum || len < 0)
					{
						return lua::argerror(L, i, "invalid table length");
					}
					
					size_t clen = (size_t)(len + 1) * sizeof(cell);
					if(!amx::MemCheck(amx, clen))
					{
						return lua::amx_error(L, AMX_ERR_MEMORY);
					}
					auto addr = reinterpret_cast<cell*>(data + amx->hea);
					if(!restorers)
					{
						restorers = std::unique_ptr<std::unordered_map<int, std::pair<cell*, std::vector<void(*)(lua_State *L, cell value)>>>>(new std::unordered_map<int, std::pair<cell*, std::vector<void(*)(lua_State *L, cell value)>>>());
					}
					auto &pair = (*restorers)[i];
					pair.first = addr;
					auto &rvector = pair.second;
					for(int j = 1; j <= len; j++)
					{
						lua_pushinteger(L, j);
						switch(lua_gettable(L, i))
						{
							case LUA_TNUMBER:
								if(lua_isinteger(L, -1))
								{
									auto num = lua_tointeger(L, -1);
									if(num < std::numeric_limits<cell>::min() || num > std::numeric_limits<ucell>::max())
									{
										return lua::argerror(L, i, "table index %d: %I cannot be stored in a single cell", j, num);
									}
									value = (cell)num;
									rvector.push_back([](lua_State *L, cell value){ lua_pushinteger(L, value); });
								}else{
									float num = (float)lua_tonumber(L, i);
									value = amx_ftoc(num);
									rvector.push_back([](lua_State *L, cell value) { lua_pushnumber(L, amx_ctof(value)); });
								}
								break;
							case LUA_TBOOLEAN:
								value = lua_toboolean(L, -1);
								rvector.push_back([](lua_State *L, cell value) { lua_pushboolean(L, value); });
								break;
							case LUA_TLIGHTUSERDATA:
								value = reinterpret_cast<cell>(lua_touserdata(L, -1));
								rvector.push_back([](lua_State *L, cell value) { lua_pushlightuserdata(L, reinterpret_cast<void*>(value)); });
								break;
							default:
								return lua::argerror(L, i, "table index %d: cannot marshal %s", j, luaL_typename(L, -1));
						}
						lua_pop(L, 1);
						*(addr++) = value;
					}
					*(addr++) = 0;
					value = amx->hea;
					amx->hea += clen;
				}else{
					if(lua_isnil(L, i) && paramcount == 0)
					{
						continue;
					}
					return lua::argerrortype(L, i, i == 1 ? "simple type or function" : "simple type");
				}
				
				amx->stk -= sizeof(cell);
				paramcount++;
				*(cell*)(data + amx->stk) = value;
			}

			amx->stk -= sizeof(cell);
			auto params = (cell*)(data + amx->stk);
			*params = paramcount * sizeof(cell);

			amx->error = 0;

			{
				lua::jumpguard guard(L);
				result = native(amx, params);
			}

			if(restorers)
			{
				for(auto &pair : *restorers)
				{
					int i = pair.first;
					cell *addr = pair.second.first;
					for(size_t j = 0; j < pair.second.second.size(); j++)
					{
						lua_pushinteger(L, j + 1);
						pair.second.second[j](L, *(addr++));
						lua_settable(L, i);
					}
				}
			}

			errorcode = amx->error;
		}

		if(errorcode)
		{
			bool mainstate = false;
			if(errorcode == AMX_ERR_SLEEP)
			{
				mainstate = !lua_isyieldable(L);
			}
			if(errorcode != AMX_ERR_SLEEP || mainstate)
			{
				return lua::amx_error(L, errorcode, result);
			}
			lua_pushvalue(L, lua_upvalueindex(3));
			lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
			if(castresult && lua::numresults(L) != 0)
			{
				return lua_yieldk(L, 2, lua_gettop(L) - 1, [](lua_State *L, int status, lua_KContext top)
				{
					lua_pushvalue(L, 1);
					lua_insert(L, top);
					return lua::tailcall(L, lua_gettop(L) - top);
				});
			}else{
				return lua_yield(L, 2);
			}
		}

		if(lua::numresults(L) != 0)
		{
			if(castresult)
			{
				lua_settop(L, 1);
			}
			lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
			if(castresult)
			{
				return lua::tailcall(L, 1);
			}
			return 1;
		}
	}
	return 0;
}

int __call_fast(lua_State *L)
{
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto native = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	if(native)
	{
		int errorcode;
		cell result;
		bool castresult = false;

		{
			cell *params = reinterpret_cast<cell*>(alloca(sizeof(cell) * (1 + lua_gettop(L))));
			cell *end = params + (1 + lua_gettop(L));

			int paramcount = 0;
			for(int i = lua_gettop(L); i >= 1; i--)
			{
				cell value = 0;
			
				if(lua_isinteger(L, i))
				{
					auto num = lua_tointeger(L, i);
					if(num < std::numeric_limits<cell>::min() || num > std::numeric_limits<ucell>::max())
					{
						return lua::argerror(L, i, "%I cannot be stored in a single cell", num);
					}
					value = (cell)num;
				}else if(lua::isnumber(L, i))
				{
					float num = (float)lua_tonumber(L, i);
					value = amx_ftoc(num);
				}else if(lua_isboolean(L, i))
				{
					value = lua_toboolean(L, i);
				}else if(i == 1 && lua_isfunction(L, i))
				{
					castresult = true;
					continue;
				}else if(lua_islightuserdata(L, i))
				{
					value = reinterpret_cast<cell>(lua_touserdata(L, i));
				}else{
					if(lua_isnil(L, i) && paramcount == 0)
					{
						continue;
					}
					return lua::argerrortype(L, i, i == 1 ? "simple type or function" : "simple type");
				}

				*(--end) = value;
				paramcount++;
			}

			*(--end) = paramcount * sizeof(cell);

			amx->error = 0;

			{
				lua::jumpguard guard(L);
				result = native(amx, end);
			}

			errorcode = amx->error;
		}

		if(errorcode)
		{
			return lua::amx_error(L, errorcode, result);
		}

		if(lua::numresults(L) != 0)
		{
			if(castresult)
			{
				lua_settop(L, 1);
			}
			lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
			if(castresult)
			{
				return lua::tailcall(L, 1);
			}
			return 1;
		}
	}
	return 0;
}

int getnative(lua_State *L)
{
	auto &info = lua::touserdata<std::shared_ptr<amx_native_info>>(L, lua_upvalueindex(1));
	auto name = luaL_checkstring(L, 1);
	bool fast = luaL_opt(L, lua::checkboolean, 2, false);
	auto it = info->natives.find(name);
	if(it != info->natives.end())
	{
		lua_pushlightuserdata(L, info->amx);
		lua_pushlightuserdata(L, reinterpret_cast<void*>(it->second));
		if(fast)
		{
			lua_pushcclosure(L, __call_fast, 2);
		}else{
			lua_pushvalue(L, lua_upvalueindex(2));
			lua_pushcclosure(L, __call, 3);
		}
		return 1;
	}
	lua_pushnil(L);
	return 1;
}

int native_index(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 2);
	lua_pushvalue(L, lua_upvalueindex(2));
	lua_call(L, 2, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -2);
	lua_settable(L, 1);
	return 1;
}

int nativeopts(lua_State *L)
{
	if(!lua_isnoneornil(L, 1))
	{
		bool fast = lua::checkboolean(L, 1);
		lua_pushboolean(L, fast);
		lua_setupvalue(L, lua_upvalueindex(1), 2);
	}
	return 0;
}

void lua::interop::init_native(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);
	
	auto it = amx_map.find(amx);
	if(it == amx_map.end())
	{
		it = amx_map.insert(std::make_pair(amx, std::make_shared<amx_native_info>(amx))).first;
	}

	lua_newtable(L);
	lua_createtable(L, 0, 1);

	lua::pushuserdata(L, it->second);
	lua_getfield(L, table, "sleep");
	lua_pushnil(L);
	lua_pushcclosure(L, getnative, 3);
	lua_pushvalue(L, -1);
	lua_setupvalue(L, -2, 3);
	lua_pushvalue(L, -1);
	lua_setfield(L, table, "getnative");
	lua_pushboolean(L, false);
	lua_pushcclosure(L, native_index, 2);
	lua_pushvalue(L, -1);
	lua_pushcclosure(L, nativeopts, 1);
	lua_setfield(L, table, "nativeopts");
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	lua_setfield(L, table, "native");
}

void lua::interop::amx_register_natives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
{
	auto it = amx_map.find(amx);
	if(it == amx_map.end())
	{
		it = amx_map.insert(std::make_pair(amx, std::make_shared<amx_native_info>(amx))).first;
	}
	auto &info = *it->second;
	for(int i = 0; nativelist[i].name != nullptr && (i < number || number == -1); i++)
	{
		info.natives.insert(std::make_pair(nativelist[i].name, nativelist[i].func));
	}
}

bool lua::interop::amx_get_param_addr(AMX *amx, cell amx_addr, cell **phys_addr)
{
	if(phys_addr && addr_set.find(amx_addr) != addr_set.end())
	{
		auto hdr = (AMX_HEADER*)amx->base;
		auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
		*phys_addr = reinterpret_cast<cell*>(data + amx_addr);
		return true;
	}
	return false;
}

void lua::interop::amx_unregister_natives(AMX *amx)
{
	auto it = amx_map.find(amx);
	if(it != amx_map.end())
	{
		amx_map.erase(it);
	}
}

AMX_NATIVE lua::interop::find_native(AMX *amx, const char *native)
{
	auto it = amx_map.find(amx);
	if(it == amx_map.end())
	{
		return nullptr;
	}
	auto &natives = it->second->natives;
	auto it2 = natives.find(native);
	if(it2 == natives.end())
	{
		return nullptr;
	}
	return it2->second;
}
