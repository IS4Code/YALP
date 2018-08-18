#include "hooks.h"
#include "main.h"
#include "lua/interop.h"
#include "lua/interop/native.h"
#include "lua/interop/public.h"
#include "amx/loader.h"

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"
#include "subhook/subhook.h"

extern void *pAMXFunctions;

subhook_t amx_Init_h;
subhook_t amx_Register_h;
subhook_t amx_Exec_h;
subhook_t amx_FindPublic_h;
//subhook_t amx_PushString_h;

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

int AMXAPI amx_RegisterOrig(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
{
	if(subhook_is_installed(amx_Register_h))
	{
		return reinterpret_cast<decltype(&amx_Register)>(subhook_get_trampoline(amx_Register_h))(amx, nativelist, number);
	} else {
		return amx_Register(amx, nativelist, number);
	}
}

int AMXAPI amx_FindPublicOrig(AMX *amx, const char *funcname, int *index)
{
	if(subhook_is_installed(amx_FindPublic_h))
	{
		return reinterpret_cast<decltype(&amx_FindPublicOrig)>(subhook_get_trampoline(amx_FindPublic_h))(amx, funcname, index);
	}else{
		return amx_FindPublic(amx, funcname, index);
	}
}

/*int AMXAPI amx_PushStringOrig(AMX *amx, cell *amx_addr, cell **phys_addr, const char *string, int pack, int use_wchar)
{
	if(subhook_is_installed(amx_PushString_h))
	{
		return reinterpret_cast<decltype(&amx_PushStringOrig)>(subhook_get_trampoline(amx_PushString_h))(amx, amx_addr, phys_addr, string, pack, use_wchar);
	} else {
		return amx_PushString(amx, amx_addr, phys_addr, string, pack, use_wchar);
	}
}*/

namespace hooks
{
	int AMXAPI amx_Init(AMX *amx, void *program)
	{
		int ret = amx_InitOrig(amx, program);
		if(ret == AMX_ERR_NONE)
		{
			amx::loader::Init(amx, program);
		}
		return ret;
	}

	int AMXAPI amx_Exec(AMX *amx, cell *retval, int index)
	{
		int result;
		if(lua::interop::amx_exec(amx, retval, index, result))
		{
			return result;
		}
		return amx_ExecOrig(amx, retval, index);
	}

	int AMXAPI amx_FindPublic(AMX *amx, const char *funcname, int *index)
	{
		if(lua::interop::amx_find_public(amx, funcname, index))
		{
			return AMX_ERR_NONE;
		}

		return amx_FindPublicOrig(amx, funcname, index);
	}

	int AMXAPI amx_Register(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
	{
		int ret = amx_RegisterOrig(amx, nativelist, number);
		lua::interop::amx_register_natives(amx, nativelist, number);
		return ret;
	}

	/*int AMXAPI amx_PushString(AMX *amx, cell *amx_addr, cell **phys_addr, const char *string, int pack, int use_wchar)
	{
		if(lua::interop::amx_push_string(amx, amx_addr, phys_addr, string, pack, use_wchar))
		{
			return AMX_ERR_NONE;
		}
		return amx_PushStringOrig(amx, amx_addr, phys_addr, string, pack, use_wchar);
	}*/
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
	//register_amx_hook(amx_PushString_h, PLUGIN_AMX_EXPORT_PushString, &hooks::amx_PushString);
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
	//unregister_hook(amx_PushString_h);
}
