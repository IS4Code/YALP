#ifndef PUBLIC_H_INCLUDED
#define PUBLIC_H_INCLUDED

#include "lua/lua.hpp"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_public(lua_State *L, AMX *amx);
		bool amx_find_public(AMX *amx, const char *funcname, int *index);
		bool amx_exec(AMX *amx, cell *retval, int index, int &result);
		//bool amx_push_string(AMX *amx, cell *amx_addr, cell **phys_addr, const char *string, int pack, int use_wchar);
	}
}

#endif
