#include "natives.h"
#include "lua_info.h"
#include <malloc.h>

namespace Natives
{
	static cell AMX_NATIVE_CALL lua_newstate(AMX *amx, cell *params)
	{
		auto state = lua::newstate(1024);
		if(state)
		{
			luaL_openlibs(state);
		}
		return reinterpret_cast<cell>(state);
	}

	static cell AMX_NATIVE_CALL lua_dostring(AMX *amx, cell *params)
	{
		auto state = reinterpret_cast<lua_State*>(params[1]);

		char *str;
		amx_StrParam(amx, params[2], str);

		return luaL_dostring(state, str);
	}

	static cell AMX_NATIVE_CALL lua_close(AMX *amx, cell *params)
	{
		auto state = reinterpret_cast<lua_State*>(params[1]);
		return lua::close(state);
	}
}

static AMX_NATIVE_INFO native_list[] =
{
	AMX_DECLARE_NATIVE(lua_newstate),
	AMX_DECLARE_NATIVE(lua_dostring),
	AMX_DECLARE_NATIVE(lua_close),
};

int RegisterNatives(AMX *amx)
{
	return amx_Register(amx, native_list, sizeof(native_list) / sizeof(*native_list));
}
