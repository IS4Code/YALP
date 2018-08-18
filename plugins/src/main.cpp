#include "main.h"
#include "hooks.h"
#include "natives.h"
#include "lua/timer.h"

#include "sdk/amx/amx.h"
#include "sdk/plugincommon.h"

logprintf_t logprintf;
extern void *pAMXFunctions;
void **ppData;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() 
{
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	::ppData = ppData;
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];

	//lua::init_plugin(ppData);

	hooks::load();

	logprintf(" YALP v0.1 loaded");
	logprintf(" Created by IllidanS4");
	return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	lua::timer::close();
	hooks::unload();

	logprintf(" YALP v0.1 unloaded");
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx) 
{
	RegisterNatives(amx);
	return AMX_ERR_NONE;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx) 
{
	return AMX_ERR_NONE;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick()
{
	lua::timer::tick();
}
