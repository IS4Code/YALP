#ifndef INTEROP_H_INCLUDED
#define INTEROP_H_INCLUDED

#include "lua/lua.hpp"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		int loader(lua_State *L);
		void amx_init(AMX *amx, void *program);
		bool amx_exec(AMX *amx, cell *retval, int index, int &result);
		bool amx_find_public(AMX *amx, const char *funcname, int *index);
	}
}

#endif
