#include "hooks.h"
#include "main.h"
#include "lua_api.h"
#include "lua/interop.h"
#include "lua/interop/native.h"
#include "lua/interop/public.h"
#include "lua/interop/pubvar.h"
#include "lua/interop/tags.h"
#include "amx/loader.h"
#include "amx/fileutils.h"

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"
#include "subhook/subhook.h"

extern void *pAMXFunctions;

template <class FType>
class amx_hook_func;

template <class Ret, class... Args>
class amx_hook_func<Ret(*)(Args...)>
{
public:
	typedef Ret hook_ftype(Ret(*)(Args...), Args...);

	typedef Ret AMXAPI handler_ftype(Args...);

	template <subhook_t &Hook, hook_ftype *Handler>
	static Ret AMXAPI handler(Args... args)
	{
		return Handler(reinterpret_cast<Ret(*)(Args...)>(subhook_get_trampoline(Hook)), args...);
	}
};

template <int Index>
class amx_hook;

template <int Index>
class amx_hook_var
{
	friend class amx_hook<Index>;

	static subhook_t hook;
};

template <int Index>
subhook_t amx_hook_var<Index>::hook;

template <int Index>
class amx_hook
{
	typedef amx_hook_var<Index> var;

public:
	template <class FType, typename amx_hook_func<FType>::hook_ftype *Func>
	struct ctl
	{
		static void load()
		{
			typename amx_hook_func<FType>::handler_ftype *hookfn = &amx_hook_func<FType>::template handler<var::hook, Func>;

			var::hook = subhook_new(reinterpret_cast<void*>(((FType*)pAMXFunctions)[Index]), reinterpret_cast<void*>(hookfn), {});
			subhook_install(var::hook);
		}

		static void unload()
		{
			subhook_remove(var::hook);
			subhook_free(var::hook);
		}

		static void install()
		{
			subhook_install(var::hook);
		}

		static void uninstall()
		{
			subhook_remove(var::hook);
		}

		static FType orig()
		{
			if(subhook_is_installed(var::hook))
			{
				return reinterpret_cast<FType>(subhook_get_trampoline(var::hook));
			}else{
				return ((FType*)pAMXFunctions)[Index];
			}
		}
	};
};

#define AMX_HOOK_FUNC(Func, ...) Func(decltype(&::Func) _base_func, __VA_ARGS__) noexcept
#define base_func _base_func
#define amx_Hook(Func) amx_hook<PLUGIN_AMX_EXPORT_##Func>::ctl<decltype(&::amx_##Func), &hooks::amx_##Func>

namespace hooks
{
	int AMX_HOOK_FUNC(amx_Init, AMX *amx, void *program)
	{
		int ret = base_func(amx, program);
		if(ret == AMX_ERR_NONE)
		{
			amx::loader::Init(amx, program);
		}
		return ret;
	}

	int AMX_HOOK_FUNC(amx_Exec, AMX *amx, cell *retval, int index)
	{
		int result;
		if(lua::interop::amx_exec(amx, retval, index, result))
		{
			return result;
		}
		int error = base_func(amx, retval, index);
		if(error == AMX_ERR_SLEEP)
		{
			return lua::bind(amx, retval, index);
		}
		return error;
	}

	int AMX_HOOK_FUNC(amx_Register, AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
	{
		int ret = base_func(amx, nativelist, number);
		lua::interop::amx_register_natives(amx, nativelist, number);
		amx::RegisterNatives(amx, nativelist, number);
		return ret;
	}

	int AMX_HOOK_FUNC(amx_FindPublic, AMX *amx, const char *funcname, int *index)
	{
		int error;
		if(lua::interop::amx_find_public(amx, funcname, index, error))
		{
			return error;
		}

		return base_func(amx, funcname, index);
	}

	int AMX_HOOK_FUNC(amx_GetPublic, AMX *amx, int index, char *funcname)
	{
		if(lua::interop::amx_get_public(amx, index, funcname))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, index, funcname);
	}

	int AMX_HOOK_FUNC(amx_NumPublics, AMX *amx, int *number)
	{
		if(lua::interop::amx_num_publics(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, number);
	}

	int AMX_HOOK_FUNC(amx_FindPubVar, AMX *amx, const char *varname, cell *amx_addr)
	{
		int error;
		if(lua::interop::amx_find_pubvar(amx, varname, amx_addr, error))
		{
			return error;
		}

		return base_func(amx, varname, amx_addr);
	}

	int AMX_HOOK_FUNC(amx_GetPubVar, AMX *amx, int index, char *varname, cell *amx_addr)
	{
		if(lua::interop::amx_get_pubvar(amx, index, varname, amx_addr))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, index, varname, amx_addr);
	}

	int AMX_HOOK_FUNC(amx_NumPubVars, AMX *amx, int *number)
	{
		if(lua::interop::amx_num_pubvars(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, number);
	}

	int AMX_HOOK_FUNC(amx_FindTagId, AMX *amx, cell tag_id, char *tagname)
	{
		if(lua::interop::amx_find_tag_id(amx, tag_id, tagname))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, tag_id, tagname);
	}

	int AMX_HOOK_FUNC(amx_GetTag, AMX *amx, int index, char *tagname, cell *tag_id)
	{
		if(lua::interop::amx_get_tag(amx, index, tagname, tag_id))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, index, tagname, tag_id);
	}

	int AMX_HOOK_FUNC(amx_NumTags, AMX *amx, int *number)
	{
		if(lua::interop::amx_num_tags(amx, number))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, number);
	}

	int AMX_HOOK_FUNC(amx_GetAddr, AMX *amx, cell amx_addr, cell **phys_addr)
	{
		if(lua::interop::amx_get_addr(amx, amx_addr, phys_addr))
		{
			return AMX_ERR_NONE;
		}

		return base_func(amx, amx_addr, phys_addr);
	}
}

void hooks::load()
{
	amx_Hook(Init)::load();
	amx_Hook(Register)::load();
	amx_Hook(Exec)::load();
	amx_Hook(FindPublic)::load();
	amx_Hook(GetPublic)::load();
	amx_Hook(NumPublics)::load();
	amx_Hook(FindPubVar)::load();
	amx_Hook(GetPubVar)::load();
	amx_Hook(NumPubVars)::load();
	amx_Hook(FindTagId)::load();
	amx_Hook(GetTag)::load();
	amx_Hook(NumTags)::load();
	amx_Hook(GetAddr)::load();
}

void hooks::unload()
{
	amx_Hook(Init)::unload();
	amx_Hook(Register)::unload();
	amx_Hook(Exec)::unload();
	amx_Hook(FindPublic)::unload();
	amx_Hook(GetPublic)::unload();
	amx_Hook(NumPublics)::unload();
	amx_Hook(FindPubVar)::unload();
	amx_Hook(GetPubVar)::unload();
	amx_Hook(NumPubVars)::unload();
	amx_Hook(FindTagId)::unload();
	amx_Hook(GetTag)::unload();
	amx_Hook(NumTags)::unload();
	amx_Hook(GetAddr)::unload();
}
