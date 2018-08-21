#ifndef INTEROP_H_INCLUDED
#define INTEROP_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		int loader(lua_State *L);
	}
}

#endif
