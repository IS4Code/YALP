#ifndef FILEUTILS_H_INCLUDED
#define FILEUTILS_H_INCLUDED

#include "sdk/amx/amx.h"
#include "lua/lualibs.h"
#include <stdio.h>

namespace lua
{
	FILE *marshal_file(lua_State *L, cell value, int fseek);
	cell marshal_file(lua_State *L, FILE *file, int ftemp);
}

#endif
