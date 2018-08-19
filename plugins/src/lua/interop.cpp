#include "interop.h"
#include "lua_utils.h"
#include "amxutils.h"
#include "amx/loader.h"
#include "interop/native.h"
#include "interop/public.h"
#include "interop/memory.h"
#include "interop/string.h"
#include "interop/result.h"
#include "interop/tags.h"

#include <unordered_map>
#include <string>

class amx_info
{
public:
	std::string fs_name;
	AMX *amx = nullptr;

	~amx_info()
	{
		if(amx && amx::Unload(fs_name.c_str()))
		{
			amx = nullptr;
		}
	}
};

int lua::interop::loader(lua_State *L)
{
	amx_info &info = lua::newuserdata<amx_info>(L);
	info.fs_name = "?luafs_";
	info.fs_name.append(std::to_string(reinterpret_cast<intptr_t>(&info)));
	AMX *amx = amx::LoadNew(info.fs_name.c_str(), 1024, sNAMEMAX, [&](AMX *amx, void *program)
	{
		info.amx = amx;
		luaL_ref(L, LUA_REGISTRYINDEX);

		lua_newtable(L);

		init_native(L, amx);
		init_public(L, amx);
		init_memory(L, amx);
		init_string(L, amx);
		init_result(L, amx);
		init_tags(L, amx);
	});
	if(!amx)
	{
		lua_pop(L, 1);
		return 0;
	}

	return 1;
}
