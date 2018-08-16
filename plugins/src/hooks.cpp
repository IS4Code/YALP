#include "hooks.h"
#include "main.h"
#include "amxinfo.h"
#include "lua_info.h"

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"
#include "subhook/subhook.h"

extern void *pAMXFunctions;

subhook_t amx_Init_h;
subhook_t amx_Register_h;
subhook_t amx_Exec_h;
subhook_t amx_FindPublic_h;
/*subhook_t amx_GetAddr_h;
subhook_t amx_StrLen_h;
*/

int AMXAPI amx_InitOrig(AMX *amx, void *program)
{
	if(subhook_is_installed(amx_Init_h))
	{
		return reinterpret_cast<decltype(&amx_Init)>(subhook_get_trampoline(amx_Init_h))(amx, program);
	}else{
		return amx_Init(amx, program);
	}
}

int AMXAPI amx_ExecOrig(AMX *amx, cell *retval, int index)
{
	if(subhook_is_installed(amx_Exec_h))
	{
		return reinterpret_cast<decltype(&amx_Exec)>(subhook_get_trampoline(amx_Exec_h))(amx, retval, index);
	}else{
		return amx_Exec(amx, retval, index);
	}
}

int AMXAPI amx_FindPublicOrig(AMX *amx, const char *funcname, int *index)
{
	if(subhook_is_installed(amx_FindPublic_h))
	{
		int ret = reinterpret_cast<decltype(&amx_FindPublicOrig)>(subhook_get_trampoline(amx_FindPublic_h))(amx, funcname, index);
		return ret;
	}else{
		return amx_FindPublic(amx, funcname, index);
	}
}

namespace hooks
{
	int AMXAPI amx_Init(AMX *amx, void *program)
	{
		int ret = amx_InitOrig(amx, program);
		if(ret == AMX_ERR_NONE)
		{
			lua::amx_init(amx, program);
		}
		return ret;
	}

	int AMXAPI amx_Exec(AMX *amx, cell *retval, int index)
	{
		int result;
		if(lua::amx_exec(amx, retval, index, result))
		{
			return result;
		}
		return amx_ExecOrig(amx, retval, index);
	}

	/*int AMXAPI amx_GetAddr(AMX *amx, cell amx_addr, cell **phys_addr)
	{
		int ret = reinterpret_cast<decltype(&amx_GetAddr)>(subhook_get_trampoline(amx_GetAddr_h))(amx, amx_addr, phys_addr);

		if(ret == AMX_ERR_MEMACCESS)
		{
			if(strings::pool.is_null_address(amx, amx_addr))
			{
				strings::null_value1[0] = 0;
				*phys_addr = strings::null_value1;
				return AMX_ERR_NONE;
			}
			auto ptr = strings::pool.get(amx, amx_addr);
			if(ptr != nullptr)
			{
				strings::pool.set_cache(ptr);
				*phys_addr = &(*ptr)[0];
				return AMX_ERR_NONE;
			}
		}else if(ret == 0 && hook_ref_args)
		{
			// Variadic functions pass all arguments by ref
			// so checking the actual cell value is necessary,
			// but there is a chance that it will interpret
			// a number as a string. Better have it disabled by default.
			if(strings::pool.is_null_address(amx, **phys_addr))
			{
				strings::null_value2[0] = 1;
				strings::null_value2[1] = 0;
				*phys_addr = strings::null_value2;
				return AMX_ERR_NONE;
			}
			auto ptr = strings::pool.get(amx, **phys_addr);
			if(ptr != nullptr)
			{
				strings::pool.set_cache(ptr);
				*phys_addr = &(*ptr)[0];
				return AMX_ERR_NONE;
			}
		}
		return ret;
	}

	int AMXAPI amx_StrLen(const cell *cstring, int *length)
	{
		auto str = strings::pool.find_cache(cstring);
		if(str != nullptr)
		{
			*length = str->size();
			return AMX_ERR_NONE;
		}

		return reinterpret_cast<decltype(&amx_StrLen)>(subhook_get_trampoline(amx_StrLen_h))(cstring, length);
	}*/

	int AMXAPI amx_FindPublic(AMX *amx, const char *funcname, int *index)
	{
		if(lua::amx_find_public(amx, funcname, index))
		{
			return AMX_ERR_NONE;
		}

		return amx_FindPublicOrig(amx, funcname, index);
	}

	int AMXAPI amx_Register(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
	{
		int ret = reinterpret_cast<decltype(&amx_Register)>(subhook_get_trampoline(amx_Register_h))(amx, nativelist, number);
		lua::register_natives(amx, nativelist, number);
		return ret;
	}
}

template <class Func>
void register_amx_hook(subhook_t &hook, int index, Func *fnptr)
{
	hook = subhook_new(reinterpret_cast<void*>(((Func**)pAMXFunctions)[index]), reinterpret_cast<void*>(fnptr), {});
	subhook_install(hook);
}

void hooks::load()
{
	register_amx_hook(amx_Init_h, PLUGIN_AMX_EXPORT_Init, &hooks::amx_Init);
	register_amx_hook(amx_Register_h, PLUGIN_AMX_EXPORT_Register, &hooks::amx_Register);
	register_amx_hook(amx_Exec_h, PLUGIN_AMX_EXPORT_Exec, &hooks::amx_Exec);
	register_amx_hook(amx_FindPublic_h, PLUGIN_AMX_EXPORT_FindPublic, &hooks::amx_FindPublic);
	/*RegisterAmxHook(amx_GetAddr_h, PLUGIN_AMX_EXPORT_GetAddr, &Hooks::amx_GetAddr);
	RegisterAmxHook(amx_StrLen_h, PLUGIN_AMX_EXPORT_StrLen, &Hooks::amx_StrLen);*/
}

void unregister_hook(subhook_t hook)
{
	subhook_remove(hook);
	subhook_free(hook);
}

void hooks::unload()
{
	unregister_hook(amx_Init_h);
	unregister_hook(amx_Register_h);
	unregister_hook(amx_Exec_h);
	unregister_hook(amx_FindPublic_h);
	/*UnregisterHook(amx_GetAddr_h);
	UnregisterHook(amx_StrLen_h);
	*/
}
