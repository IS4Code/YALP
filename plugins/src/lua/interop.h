#ifndef INTEROP_H_INCLUDED
#define INTEROP_H_INCLUDED

#include "lua/lua.hpp"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		int loader(lua_State *L);
	}
}

#endif
