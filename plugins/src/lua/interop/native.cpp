#include "native.h"
#include "lua_utils.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>

constexpr cell STKMARGIN = 16 * sizeof(cell);

static std::unordered_map<AMX*, std::weak_ptr<struct amx_native_info>> amx_map;
static std::unordered_set<cell> addr_set;

struct amx_native_info
{
	AMX *amx;
	std::unordered_map<std::string, AMX_NATIVE> natives;

	amx_native_info(AMX *amx) : amx(amx)
	{

	}

	~amx_native_info()
	{
		if(amx)
		{
			amx_map.erase(amx);
			amx = nullptr;
		}
	}
};

int __call(lua_State *L)
{
	auto &info = lua::touserdata<std::shared_ptr<amx_native_info>>(L, lua_upvalueindex(1));
	auto native = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	if(native)
	{
		auto amx = info->amx;
		cell hea = amx->hea;
		cell stk = amx->stk;

		auto hdr = (AMX_HEADER*)amx->base;
		auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;

		std::vector<cell> reset_addr;

		bool castresult = false;
		int paramcount = 0;
		size_t len;
		bool isconst;
		for(int i = lua_gettop(L); i >= 1; i--)
		{
			if(amx->hea + STKMARGIN > amx->stk)
			{
				return lua::amx_error(L, AMX_ERR_STACKERR);
			}

			cell value = 0;
			
			if(lua_isinteger(L, i))
			{
				value = (cell)lua_tointeger(L, i);
			}else if(lua_isnumber(L, i))
			{
				float num = (float)lua_tonumber(L, i);
				value = amx_ftoc(num);
			}else if(lua_isboolean(L, i))
			{
				value = lua_toboolean(L, i);
			}else if(lua_isstring(L, i))
			{
				size_t len;
				auto str = lua_tolstring(L, i, &len);
				len++;

				if(amx->stk - amx->hea - len * sizeof(cell) < STKMARGIN)
				{
					return lua::amx_error(L, AMX_ERR_MEMORY);
				}

				value = amx->hea;
				auto addr = reinterpret_cast<cell*>(data + amx->hea);
				for(size_t j = 0; j < len; j++)
				{
					addr[j] = str[j];
				}
				amx->hea += len * sizeof(cell);
			}else if(i == 1 && lua_isfunction(L, i))
			{
				castresult = true;
				continue;
			}else if(auto buf = lua::tobuffer(L, i, len, isconst))
			{
				if(isconst)
				{
					if(amx->stk - amx->hea - len < STKMARGIN)
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
							reset_addr.push_back(value);
						}
					}
				}
			}else if(lua_islightuserdata(L, i))
			{
				value = reinterpret_cast<cell>(lua_touserdata(L, i));
			}else{
				if(lua_isnil(L, i) && paramcount == 0)
				{
					continue;
				}
				return luaL_argerror(L, i, "type not expected");
			}
				
			amx->stk -= sizeof(cell);
			paramcount++;
			*(cell*)(data + amx->stk) = value;
		}

		amx->stk -= sizeof(cell);
		auto params = (cell*)(data + amx->stk);
		*params = paramcount * sizeof(cell);

		amx->error = 0;
		cell result = native(amx, params);

		for(const cell &value : reset_addr)
		{
			addr_set.erase(value);
		}

		amx->stk = stk;
		amx->hea = hea;

		if(amx->error)
		{
			int code = amx->error;
			amx->error = 0;
			bool mainstate = false;
			if(code == AMX_ERR_SLEEP)
			{
				mainstate = lua_pushthread(L);
				lua_pop(L, 1);
			}
			if(code != AMX_ERR_SLEEP || mainstate)
			{
				return lua::amx_error(L, code);
			}
			lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
			if(castresult)
			{
				lua_pushvalue(L, 1);
				return lua_yield(L, 2);
			}else{
				return lua_yield(L, 1);
			}
		}

		if(lua::numresults(L) != 0)
		{
			if(castresult)
			{
				lua_pushvalue(L, 1);
			}
			lua_pushlightuserdata(L, reinterpret_cast<void*>(result));
			if(castresult)
			{
				lua_call(L, 1, 1);
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
	auto it = info->natives.find(name);
	if(it != info->natives.end())
	{
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_pushlightuserdata(L, it->second);
		lua_pushcclosure(L, __call, 2);
		return 1;
	}
	lua_pushnil(L);
	return 1;
}

int native_index(lua_State *L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 2);
	lua_call(L, 1, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -2);
	lua_settable(L, 1);
	return 1;
}

void lua::interop::init_native(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	auto info = std::make_shared<amx_native_info>(amx);
	amx_map[amx] = info;

	lua_newtable(L);
	lua_createtable(L, 0, 1);

	lua::pushuserdata(L, info);
	lua_pushcclosure(L, getnative, 1);
	lua_pushvalue(L, -1);
	lua_setfield(L, table, "getnative");
	lua_pushcclosure(L, native_index, 1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	lua_setfield(L, table, "native");
}

void lua::interop::amx_register_natives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
{
	auto it = amx_map.find(amx);
	if(it != amx_map.end())
	{
		if(auto info = it->second.lock())
		{
			for(int i = 0; nativelist[i].name != nullptr && (i < number || number == -1); i++)
			{
				info->natives.insert(std::make_pair(nativelist[i].name, nativelist[i].func));
			}
		}
	}
}

bool lua::interop::amx_get_addr(AMX *amx, cell amx_addr, cell **phys_addr)
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
