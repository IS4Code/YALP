#include "lua_adapt.h"
#include "lua_api.h"
#include "main.h"

int lua::atpanic(lua_State *L)
{
	auto str = lua_tostring(L, -1);
	lua::report_error(L, lua_status(L));
	throw lua::panic_error(str, lua_status(L));
}

bool lua::check_params(AMX *amx, cell *params, cell needed)
{
	if(params[0] >= needed * static_cast<cell>(sizeof(cell))) return true;
	logprintf("[YALP] not enough arguments (%d expected, got %d)", needed, params[0] / static_cast<cell>(sizeof(cell)));
	amx_RaiseError(amx, AMX_ERR_PARAMS);
	return false;
}
