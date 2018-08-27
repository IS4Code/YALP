#ifndef PUBVAR_H_INCLUDED
#define PUBVAR_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_pubvar(lua_State *L, AMX *amx);
		bool amx_find_pubvar(AMX *amx, const char *varname, cell *amx_addr, int &error);
		bool amx_get_pubvar(AMX *amx, int index, char *varname, cell *amx_addr);
		bool amx_num_pubvars(AMX *amx, int *number);
		bool amx_get_pubvar_addr(AMX *amx, cell amx_addr, cell **phys_addr);
	}
}

#endif
