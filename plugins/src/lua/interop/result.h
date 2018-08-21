#ifndef RESULT_H_INCLUDED
#define RESULT_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_result(lua_State *L, AMX *amx);
	}
}

#endif
