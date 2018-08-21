#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_string(lua_State *L, AMX *amx);
	}
}

#endif
