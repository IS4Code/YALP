#include "lua_info.h"
#include "lua_api.h"
#include "sdk/plugincommon.h"

void **ppData;
lua_State *lua_loading = nullptr;
AMX *last_lua_amx;

std::unordered_map<lua_State*, std::shared_ptr<lua_info>> lua_map;
std::unordered_map<AMX*, lua_State*> amx_map;

bool LoadFilterScriptFromMemory(char* pFileName, char* pFileData)
{
	return reinterpret_cast<bool(*)(char*, char*)>(ppData[PLUGIN_DATA_LOADFSCRIPT])(pFileName, pFileData);
}

bool UnloadFilterScript(char* pFileName)
{
	return reinterpret_cast<bool(*)(char*)>(ppData[PLUGIN_DATA_UNLOADFSCRIPT])(pFileName);
}

lua_info::~lua_info()
{
	if(state)
	{
		lua_close(state);
		state = nullptr;
	}
	if(amx)
	{
		UnloadFilterScript(&fs_name[0]);
		amx = nullptr;
	}
}

namespace lua
{
	void init_plugin(void **ppData)
	{
		::ppData = ppData;
	}

	void amx_load(AMX *amx)
	{
		
	}

	void amx_unload(AMX *amx)
	{
		
	}

	void amx_init(AMX *amx, void *program)
	{
		if(lua_loading && !last_lua_amx)
		{
			last_lua_amx = amx;
			amx_map[amx] = lua_loading;
			amx->flags |= AMX_FLAG_RELOC;

			auto info = std::shared_ptr<lua_info>(new lua_info());
			info->amx = amx;
			info->state = lua_loading;
			lua_map[lua_loading] = std::move(info);
		}
	}

	bool amx_find_public(AMX *amx, const char *funcname, int *index)
	{
		if(index)
		{
			auto it = amx_map.find(amx);
			if(it != amx_map.end())
			{
				auto L = it->second;
				if(lua::getpublictable(L))
				{
					if(lua::getpublic(L, funcname))
					{
						*index = (int)lua_rawlen(L, -2) + 1;
						lua_rawseti(L, -2, *index);
						lua_pop(L, 1);
						return true;
					}
					lua_pop(L, 1);
				}
			}
		}
		return false;
	}

	bool amx_exec(AMX *amx, cell *retval, int index, int &result)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			auto L = it->second;
			if(index > 0 && lua::getpublictable(L))
			{
				if(lua_rawgeti(L, -1, index) == LUA_TFUNCTION)
				{
					auto hdr = (AMX_HEADER*)amx->base;
					auto data = (amx->data != NULL) ? amx->data : amx->base + (int)hdr->dat;
					auto stk = reinterpret_cast<cell*>(data + amx->stk);
					int paramcount = amx->paramcount;
					amx->paramcount = 0;
					for(int i = 0; i < paramcount; i++)
					{
						lua_pushlightuserdata(L, reinterpret_cast<void*>(stk[i]));
					}
					amx->stk += paramcount * sizeof(cell);
					switch(lua_pcall(L, paramcount, 1, 0))
					{
						case 0:
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
							amx->error = AMX_ERR_MEMORY;
							break;
						default:
							amx->error = AMX_ERR_GENERAL;
							break;
					}
					lua_pop(L, 2);
					result = amx->error;
					return true;
				}
				lua_pop(L, 2);
			}
			result = amx->error = AMX_ERR_INDEX;
			return true;
		}
		return false;
	}

	void register_natives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
	{
		if(lua_loading && last_lua_amx)
		{
			auto &natives = lua_map.at(lua_loading)->natives;

			for(int i = 0; nativelist[i].name != nullptr && (i < number || number == -1); i++)
			{
				natives.insert(std::make_pair(nativelist[i].name, nativelist[i].func));
			}
		}
	}

	lua_State *newstate(int32_t heapspace)
	{
		auto L = luaL_newstate();
		if(!L) return nullptr;

		AMX_HEADER hdr{};
		hdr.magic = AMX_MAGIC;
		hdr.file_version = MIN_FILE_VERSION;
		hdr.amx_version = MIN_AMX_VERSION;
		hdr.dat = hdr.hea = hdr.size = sizeof(AMX_HEADER);
		hdr.stp = hdr.hea + heapspace;
		hdr.defsize = sizeof(AMX_FUNCSTUB);

		std::string name("?lua_");
		name.append(std::to_string(reinterpret_cast<intptr_t>(L)));

		lua_loading = L;
		last_lua_amx = nullptr;
		bool ok = LoadFilterScriptFromMemory(&name[0], reinterpret_cast<char*>(&hdr));
		lua_loading = nullptr;

		if(ok && last_lua_amx)
		{
			lua_map[L]->fs_name = std::move(name);
			lua::init(L);
			return L;
		}
		lua_close(L);
		return nullptr;
	}

	bool close(lua_State *L)
	{
		auto it = lua_map.find(L);
		if(it != lua_map.end())
		{
			amx_map.erase(it->second->amx);
			lua_map.erase(it);
			return true;
		}
		return false;
	}

	std::shared_ptr<lua_info> get(lua_State *L)
	{
		auto it = lua_map.find(L);
		if(it != lua_map.end())
		{
			return it->second;
		}
		return nullptr;
	}
}
