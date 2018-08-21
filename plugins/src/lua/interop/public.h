#ifndef PUBLIC_H_INCLUDED
#define PUBLIC_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_public(lua_State *L, AMX *amx);
		bool amx_find_public(AMX *amx, const char *funcname, int *index, int &error);
		bool amx_get_public(AMX *amx, int index, char *funcname);
		bool amx_num_publics(AMX *amx, int *number);
		bool amx_exec(AMX *amx, cell *retval, int index, int &result);
	}
}

#endif
