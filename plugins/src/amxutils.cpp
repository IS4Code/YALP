#include "amxutils.h"

#include <cstring>
#include <stdlib.h>

const char *amx::StrError(int errnum)
{
	static const char *messages[] = {
		/* AMX_ERR_NONE */ "(none)",
		/* AMX_ERR_EXIT */ "Forced exit",
		/* AMX_ERR_ASSERT */ "Assertion failed",
		/* AMX_ERR_STACKERR */ "Stack/heap collision (insufficient stack size)",
		/* AMX_ERR_BOUNDS */ "Array index out of bounds",
		/* AMX_ERR_MEMACCESS */ "Invalid memory access",
		/* AMX_ERR_INVINSTR */ "Invalid instruction",
		/* AMX_ERR_STACKLOW */ "Stack underflow",
		/* AMX_ERR_HEAPLOW */ "Heap underflow",
		/* AMX_ERR_CALLBACK */ "No (valid) native function callback",
		/* AMX_ERR_NATIVE */ "Native function failed",
		/* AMX_ERR_DIVIDE */ "Divide by zero",
		/* AMX_ERR_SLEEP */ "(sleep mode)",
		/* 13 */ "(reserved)",
		/* 14 */ "(reserved)",
		/* 15 */ "(reserved)",
		/* AMX_ERR_MEMORY */ "Out of memory",
		/* AMX_ERR_FORMAT */ "Invalid/unsupported P-code file format",
		/* AMX_ERR_VERSION */ "File is for a newer version of the AMX",
		/* AMX_ERR_NOTFOUND */ "File or function is not found",
		/* AMX_ERR_INDEX */ "Invalid index parameter (bad entry point)",
		/* AMX_ERR_DEBUG */ "Debugger cannot run",
		/* AMX_ERR_INIT */ "AMX not initialized (or doubly initialized)",
		/* AMX_ERR_USERDATA */ "Unable to set user data field (table full)",
		/* AMX_ERR_INIT_JIT */ "Cannot initialize the JIT",
		/* AMX_ERR_PARAMS */ "Parameter error",
		/* AMX_ERR_DOMAIN */ "Domain error, expression result does not fit in range",
		/* AMX_ERR_GENERAL */ "General error (unknown or unspecific error)",
	};
	if(errnum < 0 || (size_t)errnum >= sizeof(messages) / sizeof(*messages))
		return "(unknown)";
	return messages[errnum];
}

std::string amx::GetString(const cell *source, size_t size, bool cstring)
{
	if(source == nullptr) return {};

	std::string str;
	if(static_cast<ucell>(*source) > UNPACKEDMAX)
	{
		cell c = 0;
		int i = sizeof(cell) - 1;
		while(true)
		{
			if(i == sizeof(cell) - 1)
			{
				c = *source++;
				if(size-- == 0)
				{
					break;
				}
			}
			char ch = c >> (i * 8);
			if(cstring && ch == '\0') break;
			str.push_back(ch);
			i = (i + sizeof(cell) - 1) % sizeof(cell);
		}
	}else{
		cell c;
		while(true)
		{
			c = *source++;
			if(size-- == 0)
			{
				break;
			}
			if(cstring && c == 0) break;
			str.push_back(c);
		}
	}
	return str;
}

void amx::SetString(cell *dest, const char *source, size_t len, bool pack)
{
	if(!pack)
	{
		while(len-- > 0)
		{
			*dest++ = *source++;
		}
		*dest = 0;
	}else{
		dest[len / sizeof(cell)] = 0;
		std::memcpy(dest, source, len);
		len = (len + sizeof(cell) - 1) / sizeof(cell);

		while(len-- > 0)
		{
			*dest = _byteswap_ulong(*dest);
			dest++;
		}
	}
}

constexpr cell STKMARGIN = 16 * sizeof(cell);

bool amx::MemCheck(AMX *amx, size_t size)
{
	return (cell)size >= 0 && amx->hea + (cell)size + STKMARGIN <= amx->stk;
}
