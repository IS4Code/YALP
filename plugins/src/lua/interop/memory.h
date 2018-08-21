#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_memory(lua_State *L, AMX *amx);
	}
}

#endif
