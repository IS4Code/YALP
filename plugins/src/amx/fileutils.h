#ifndef FILEUTILS_H_INCLUDED
#define FILEUTILS_H_INCLUDED

#include "sdk/amx/amx.h"
#include "lua/lualibs.h"
#include <stdio.h>

namespace amx
{
	bool FileLoad(cell value, AMX *amx, FILE *&f);
	cell FileStore(FILE *file, AMX *amx);

	void RegisterNatives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);
}

#endif
