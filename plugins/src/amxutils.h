#ifndef AMXUTILS_H_INCLUDED
#define AMXUTILS_H_INCLUDED

#include "sdk/amx/amx.h"
#include <string>

namespace amx
{
	const char *StrError(int error);
	std::string GetString(const cell *source, size_t size, bool cstring);
	void SetString(cell *dest, const char *source, size_t len, bool pack);
	bool MemCheck(AMX *amx, size_t size);
}

#endif
