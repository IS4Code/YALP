#ifndef SLEEP_H_INCLUDED
#define SLEEP_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_sleep(lua_State *L, AMX *amx);
		void handle_sleep(lua_State *L, AMX *amx, int contlist);
	}
}

#endif
