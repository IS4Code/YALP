#ifndef LOADER_H_INCLUDED
#define LOADER_H_INCLUDED

#include "sdk/amx/amx.h"
#include <functional>

namespace amx
{
	namespace loader
	{
		bool Init(AMX *amx, void *program);
	}
	AMX *LoadProgram(const char *name, const char *program, std::function<void(AMX*, void*)> &&init);
	AMX *LoadNew(const char *name, int32_t heapspace, uint16_t namelength, std::function<void(AMX*, void*)> &&init);
	bool Unload(const char *name);
}

#endif
