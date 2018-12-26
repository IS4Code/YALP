#include "loader.h"
#include "sdk/plugincommon.h"

extern void **ppData;

AMX *last_amx;
std::function<void(AMX*, void*)> init_func;

bool LoadFilterScriptFromMemory(const char* pFileName, const char* pFileData)
{
	return reinterpret_cast<bool(*)(char*, char*)>(ppData[PLUGIN_DATA_LOADFSCRIPT])(const_cast<char*>(pFileName), const_cast<char*>(pFileData));
}

bool UnloadFilterScript(const char* pFileName)
{
	return reinterpret_cast<bool(*)(char*)>(ppData[PLUGIN_DATA_UNLOADFSCRIPT])(const_cast<char*>(pFileName));
}

bool amx::loader::Init(AMX *amx, void *program)
{
	if(init_func && last_amx == nullptr)
	{
		last_amx = amx;
		amx->flags |= AMX_FLAG_RELOC;
		init_func(amx, program);
		return true;
	}
	return false;
}

AMX *amx::LoadProgram(const char *name, const char *program, std::function<void(AMX*, void*)> &&init)
{
	last_amx = nullptr;
	init_func = std::move(init);
	bool ok = LoadFilterScriptFromMemory(name, program);
	AMX *amx = last_amx;
	last_amx = nullptr;
	init_func = nullptr;
	if(ok)
	{
		return amx;
	}
	return nullptr;
}

struct AMX_FAKE_HEADER : public AMX_HEADER
{
	uint16_t namelength;
};

AMX *amx::LoadNew(const char *name, int32_t heapspace, uint16_t namelength, std::function<void(AMX*, void*)> &&init)
{
	AMX_FAKE_HEADER hdr{};
	hdr.magic = AMX_MAGIC;
	hdr.file_version = 7;
	hdr.amx_version = MIN_AMX_VERSION;
	hdr.cod = hdr.dat = hdr.hea = hdr.size = sizeof(hdr);
	hdr.stp = hdr.hea + heapspace;
	hdr.defsize = sizeof(AMX_FUNCSTUBNT);
	hdr.nametable = reinterpret_cast<unsigned char*>(&hdr.namelength) - reinterpret_cast<unsigned char*>(&hdr);
	hdr.namelength = namelength;
	hdr.publics = hdr.natives = hdr.libraries = hdr.pubvars = hdr.tags = hdr.nametable;
	return LoadProgram(name, reinterpret_cast<char*>(&hdr), std::move(init));
}

bool amx::Unload(const char *name)
{
	return UnloadFilterScript(name);
}
