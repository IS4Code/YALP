#ifndef FILEUTILS_H_INCLUDED
#define FILEUTILS_H_INCLUDED

#include "sdk/amx/amx.h"
#include "lua/lualibs.h"
#include <stdio.h>

namespace lua
{
	FILE *marshal_file(lua_State *L, cell value, AMX *amx, AMX_NATIVE fseek);
	cell marshal_file(lua_State *L, FILE *file, AMX *amx, AMX_NATIVE ftemp);
}

#endif
