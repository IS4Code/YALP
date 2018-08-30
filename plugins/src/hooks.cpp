#include "hooks.h"
#include "main.h"
#include "lua_api.h"
#include "lua/interop.h"
#include "lua/interop/native.h"
#include "lua/interop/public.h"
#include "lua/interop/pubvar.h"
#include "lua/interop/tags.h"
#include "amx/loader.h"

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"
#include "subhook/subhook.h"

extern void *pAMXFunctions;

subhook_t amx_Init_h;
subhook_t amx_Register_h;
subhook_t amx_Exec_h;
subhook_t amx_FindPublic_h;
subhook_t amx_GetPublic_h;
subhook_t amx_NumPublics_h;
subhook_t amx_FindPubVar_h;
subhook_t amx_GetPubVar_h;
subhook_t amx_NumPubVars_h;
subhook_t amx_FindTagId_h;
subhook_t amx_GetTag_h;
subhook_t amx_NumTags_h;
subhook_t amx_GetAddr_h;

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

int AMXAPI amx_GetPublicOrig(AMX *amx, int index, char *funcname)
{
	if(subhook_is_installed(amx_GetPublic_h))
	{
		return reinterpret_cast<decltype(&amx_GetPublicOrig)>(subhook_get_trampoline(amx_GetPublic_h))(amx, index, funcname);
	}else{
		return amx_GetPublic(amx, index, funcname);
	}
}

int AMXAPI amx_NumPublicsOrig(AMX *amx, int *number)
{
	if(subhook_is_installed(amx_NumPublics_h))
	{
		return reinterpret_cast<decltype(&amx_NumPublicsOrig)>(subhook_get_trampoline(amx_NumPublics_h))(amx, number);
	}else{
		return amx_NumPublics(amx, number);
	}
}

int AMXAPI amx_FindPubVarOrig(AMX *amx, const char *varname, cell *amx_addr)
{
	if(subhook_is_installed(amx_FindPubVar_h))
	{
		return reinterpret_cast<decltype(&amx_FindPubVarOrig)>(subhook_get_trampoline(amx_FindPubVar_h))(amx, varname, amx_addr);
	} else{
		return amx_FindPubVar(amx, varname, amx_addr);
	}
}

int AMXAPI amx_GetPubVarOrig(AMX *amx, int index, char *varname, cell *amx_addr)
{
	if(subhook_is_installed(amx_GetPubVar_h))
	{
		return reinterpret_cast<decltype(&amx_GetPubVarOrig)>(subhook_get_trampoline(amx_GetPubVar_h))(amx, index, varname, amx_addr);
	} else{
		return amx_GetPubVar(amx, index, varname, amx_addr);
	}
}

int AMXAPI amx_NumPubVarsOrig(AMX *amx, int *number)
{
	if(subhook_is_installed(amx_NumPubVars_h))
	{
		return reinterpret_cast<decltype(&amx_NumPubVarsOrig)>(subhook_get_trampoline(amx_NumPubVars_h))(amx, number);
	} else{
		return amx_NumPubVars(amx, number);
	}
}

int AMXAPI amx_FindTagIdOrig(AMX *amx, cell tag_id, char *tagname)
{
	if(subhook_is_installed(amx_FindTagId_h))
	{
		return reinterpret_cast<decltype(&amx_FindTagIdOrig)>(subhook_get_trampoline(amx_FindTagId_h))(amx, tag_id, tagname);
	}else{
		return amx_FindTagId(amx, tag_id, tagname);
	}
}

int AMXAPI amx_GetTagOrig(AMX *amx, int index, char *tagname, cell *tag_id)
{
	if(subhook_is_installed(amx_GetTag_h))
	{
		return reinterpret_cast<decltype(&amx_GetTagOrig)>(subhook_get_trampoline(amx_GetTag_h))(amx, index, tagname, tag_id);
	}else{
		return amx_GetTag(amx, index, tagname, tag_id);
	}
}

int AMXAPI amx_NumTagsOrig(AMX *amx, int *number)
{
	if(subhook_is_installed(amx_NumTags_h))
	{
		return reinterpret_cast<decltype(&amx_NumTagsOrig)>(subhook_get_trampoline(amx_NumTags_h))(amx, number);
	}else{
		return amx_NumTags(amx, number);
	}
}

int AMXAPI amx_GetAddrOrig(AMX *amx, cell amx_addr, cell **phys_addr)
{
	if(subhook_is_installed(amx_GetAddr_h))
	{
		return reinterpret_cast<decltype(&amx_GetAddrOrig)>(subhook_get_trampoline(amx_GetAddr_h))(amx, amx_addr, phys_addr);
	}else{
		return amx_GetAddr(amx, amx_addr, phys_addr);
	}
}

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
		int error = amx_ExecOrig(amx, retval, index);
		if(error == AMX_ERR_SLEEP)
		{
			lua::bind(amx);
		}
		return error;
	}

	int AMXAPI amx_Register(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
	{
		int ret = amx_RegisterOrig(amx, nativelist, number);
		lua::interop::amx_register_natives(amx, nativelist, number);
		return ret;
	}

	int AMXAPI amx_FindPublic(AMX *amx, const char *funcname, int *index)
	{
		int error;
		if(lua::interop::amx_find_public(amx, funcname, index, error))
		{
			return error;
		}

		return amx_FindPublicOrig(amx, funcname, index);
	}

	int AMXAPI amx_GetPublic(AMX *amx, int index, char *funcname)
	{
		if(lua::interop::amx_get_public(amx, index, funcname))
		{
			return AMX_ERR_NONE;
		}

		return amx_GetPublicOrig(amx, index, funcname);
	}

	int AMXAPI amx_NumPublics(AMX *amx, int *number)
	{
		if(lua::interop::amx_num_publics(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return amx_NumPublicsOrig(amx, number);
	}

	int AMXAPI amx_FindPubVar(AMX *amx, const char *varname, cell *amx_addr)
	{
		int error;
		if(lua::interop::amx_find_pubvar(amx, varname, amx_addr, error))
		{
			return error;
		}

		return amx_FindPubVarOrig(amx, varname, amx_addr);
	}

	int AMXAPI amx_GetPubVar(AMX *amx, int index, char *varname, cell *amx_addr)
	{
		if(lua::interop::amx_get_pubvar(amx, index, varname, amx_addr))
		{
			return AMX_ERR_NONE;
		}

		return amx_GetPubVarOrig(amx, index, varname, amx_addr);
	}

	int AMXAPI amx_NumPubVars(AMX *amx, int *number)
	{
		if(lua::interop::amx_num_pubvars(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return amx_NumPubVarsOrig(amx, number);
	}

	int AMXAPI amx_FindTagId(AMX *amx, cell tag_id, char *tagname)
	{
		if(lua::interop::amx_find_tag_id(amx, tag_id, tagname))
		{
			return AMX_ERR_NONE;
		}

		return amx_FindTagIdOrig(amx, tag_id, tagname);
	}

	int AMXAPI amx_GetTag(AMX *amx, int index, char *tagname, cell *tag_id)
	{
		if(lua::interop::amx_get_tag(amx, index, tagname, tag_id))
		{
			return AMX_ERR_NONE;
		}

		return amx_GetTagOrig(amx, index, tagname, tag_id);
	}

	int AMXAPI amx_NumTags(AMX *amx, int *number)
	{
		if(lua::interop::amx_num_tags(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return amx_NumTagsOrig(amx, number);
	}

	int AMXAPI amx_GetAddr(AMX *amx, cell amx_addr, cell **phys_addr)
	{
		if(lua::interop::amx_get_addr(amx, amx_addr, phys_addr))
		{
			return AMX_ERR_NONE;
		}

		return amx_GetAddrOrig(amx, amx_addr, phys_addr);
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
	register_amx_hook(amx_GetPublic_h, PLUGIN_AMX_EXPORT_GetPublic, &hooks::amx_GetPublic);
	register_amx_hook(amx_NumPublics_h, PLUGIN_AMX_EXPORT_NumPublics, &hooks::amx_NumPublics);
	register_amx_hook(amx_FindPubVar_h, PLUGIN_AMX_EXPORT_FindPubVar, &hooks::amx_FindPubVar);
	register_amx_hook(amx_GetPubVar_h, PLUGIN_AMX_EXPORT_GetPubVar, &hooks::amx_GetPubVar);
	register_amx_hook(amx_NumPubVars_h, PLUGIN_AMX_EXPORT_NumPubVars, &hooks::amx_NumPubVars);
	register_amx_hook(amx_FindTagId_h, PLUGIN_AMX_EXPORT_FindTagId, &hooks::amx_FindTagId);
	register_amx_hook(amx_GetTag_h, PLUGIN_AMX_EXPORT_GetTag, &hooks::amx_GetTag);
	register_amx_hook(amx_NumTags_h, PLUGIN_AMX_EXPORT_NumTags, &hooks::amx_NumTags);
	register_amx_hook(amx_GetAddr_h, PLUGIN_AMX_EXPORT_GetAddr, &hooks::amx_GetAddr);
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
	unregister_hook(amx_GetPublic_h);
	unregister_hook(amx_NumPublics_h);
	unregister_hook(amx_FindPubVar_h);
	unregister_hook(amx_GetPubVar_h);
	unregister_hook(amx_NumPubVars_h);
	unregister_hook(amx_FindTagId_h);
	unregister_hook(amx_GetTag_h);
	unregister_hook(amx_NumTags_h);
	unregister_hook(amx_GetAddr_h);
}
