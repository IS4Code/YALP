#ifndef LUA_INFO_H_INCLUDED
#define LUA_INFO_H_INCLUDED

#include "lua/lua.hpp"
#include "sdk/amx/amx.h"
#include <string>
#include <unordered_map>
#include <memory>

struct lua_info
{
	std::string fs_name;
	lua_State *state = nullptr;
	AMX *amx = nullptr;
	std::unordered_map<std::string, AMX_NATIVE> natives;

	lua_info() = default;
	lua_info(const lua_info&) = delete;
	lua_info &operator=(const lua_info&) = delete;

	~lua_info();
};

namespace lua
{
	void init_plugin(void **ppData);
	void amx_load(AMX *amx);
	void amx_unload(AMX *amx);
	void amx_init(AMX *amx, void *program);
	bool amx_exec(AMX *amx, cell *retval, int index, int &result);
	bool amx_find_public(AMX *amx, const char *funcname, int *index);
	void register_natives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);
	lua_State *newstate(int32_t heapspace);
	bool close(lua_State *L);
	std::shared_ptr<lua_info> get(lua_State *L);
}

#endif
