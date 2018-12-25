#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_file(lua_State *L, AMX *amx);
	}
}

#endif
