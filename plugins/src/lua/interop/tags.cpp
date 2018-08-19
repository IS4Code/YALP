#include "tags.h"
#include "lua_utils.h"

#include <unordered_map>
#include <memory>
#include <cstring>

static std::unordered_map<AMX*, std::weak_ptr<struct amx_tag_info>> amx_map;

struct amx_tag_info
{
	AMX *amx;

	lua_State *L;
	int self;
	int taglist;

	amx_tag_info(lua_State *L, AMX *amx) : L(L), amx(amx)
	{

	}

	~amx_tag_info()
	{
		if(amx)
		{
			amx_map.erase(amx);
			amx = nullptr;
		}
	}
};

bool getudatatag(lua_State *L, int idx, const char *&tagname)
{
	if(lua_getmetatable(L, idx))
	{
		if(lua_getfield(L, -1, "__tag") == LUA_TSTRING)
		{
			tagname = lua_tostring(L, -1);
			lua_pop(L, 2);
			return true;
		}
		lua_pop(L, 2);
	}
	return false;
}

int tagof(lua_State *L)
{
	const char *tagname;
	if(lua_type(L, 1) == LUA_TSTRING)
	{
		tagname = lua_tostring(L, 1);
	}else if(lua_isinteger(L, 1))
	{
		tagname = "";
	}else if(lua_isboolean(L, 1))
	{
		tagname = "bool";
	}else if(lua_isnumber(L, 1))
	{
		tagname = "Float";
	}else if(!getudatatag(L, 1, tagname))
	{
		return luaL_argerror(L, 1, "cannot obtain tag");
	}

	if(!tagname[0])
	{
		lua_pushlightuserdata(L, reinterpret_cast<void*>(0x80000000));
		return 1;
	}
	size_t maxlen = (size_t)lua_tointeger(L, lua_upvalueindex(2));
	if(std::strlen(tagname) > maxlen)
	{
		auto msg = lua_pushfstring(L, "tag name exceeds %d characters", maxlen);
		return luaL_argerror(L, 1, msg);
	}
	lua_pushstring(L, tagname);
	int index = luaL_ref(L, lua_upvalueindex(1));
	index |= 0x80000000;
	if(tagname[0] >= 'A' && tagname[0] <= 'Z')
	{
		index |= 0x40000000;
	}
	lua_pushlightuserdata(L, reinterpret_cast<void*>(index));
	return 1;
}

void lua::interop::init_tags(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	auto info = std::make_shared<amx_tag_info>(L, amx);
	amx_map[amx] = info;
	lua::pushuserdata(L, info);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	int maxlen;
	amx_NameLength(amx, &maxlen);
	lua_pushinteger(L, maxlen);
	lua_pushcclosure(L, tagof, 2);
	lua_setfield(L, table, "tagof");
	info->taglist = luaL_ref(L, LUA_REGISTRYINDEX);

	info->self = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool gettaglist(lua_State *L, int index)
{
	if(lua_rawgeti(L, LUA_REGISTRYINDEX, index) == LUA_TTABLE)
	{
		return true;
	}
	lua_pop(L, 1);
	return false;
}

bool lua::interop::amx_find_tag_id(AMX *amx, cell tag_id, char *tagname)
{
	if(tagname && (tag_id & 0x80000000) == 0 && tag_id != 0)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(gettaglist(L, info->taglist))
				{
					int index = tag_id & 0x3FFFFFFF;
					if(lua_rawgeti(L, -1, index) == LUA_TSTRING)
					{
						auto str = lua_tostring(L, -1);
						if((str[0] >= 'A' && str[0] <= 'Z') == ((tag_id & 0x40000000) != 0))
						{
							std::strcpy(tagname, str);
							lua_pop(L, 2);
							return true;
						}
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_get_tag(AMX *amx, int index, char *tagname, cell *tag_id)
{
	if(tagname || tag_id)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(gettaglist(L, info->taglist))
				{
					index += 1;
					if(lua_rawgeti(L, -1, index) == LUA_TSTRING)
					{
						auto str = lua_tostring(L, -1);
						if(tagname)
						{
							std::strcpy(tagname, str);
						}
						if(tag_id)
						{
							*tag_id = index | (str[0] >= 'A' && str[0] <= 'Z' ? 0x40000000 : 0);
						}
						lua_pop(L, 2);
						return true;
					}
					lua_pop(L, 2);
				}
			}
		}
	}
	return false;
}

bool lua::interop::amx_num_tags(AMX *amx, int *number)
{
	if(number)
	{
		auto it = amx_map.find(amx);
		if(it != amx_map.end())
		{
			if(auto info = it->second.lock())
			{
				auto L = info->L;
				if(gettaglist(L, info->taglist))
				{
					*number = lua_rawlen(L, -1);
					lua_pop(L, 1);
					return true;
				}
			}
		}
	}
	return false;
}
