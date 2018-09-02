#include "lua_adapt.h"
#include "lua_api.h"
#include "main.h"

int lua::atpanic(lua_State *L)
{
	auto str = lua_tostring(L, -1);
	lua::report_error(L, lua_status(L));
	throw lua::panic_error(str);
}
