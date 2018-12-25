#include "file.h"
#include "lua_utils.h"
#include "fileutils.h"
#include "native.h"

template <bool DestroyOriginal>
int asfile(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	if(!ptr)
	{
		lua_pushnil(L);
		return 1;
	}

	if(luaL_getmetatable(L, LUA_FILEHANDLE) != LUA_TTABLE)
	{
		return luaL_error(L, "the io package is not loaded");
	}
	int mtindex = lua_absindex(L, -1);

#ifdef _WIN32
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto fseek = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	FILE *f = lua::marshal_file(L, reinterpret_cast<cell>(ptr), amx, fseek);
#else
	FILE *f = lua::marshal_file(L, reinterpret_cast<cell>(ptr), nullptr, nullptr);
#endif

	if(DestroyOriginal)
	{
#ifdef _WIN32
		auto fclose = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(3)));
		cell params[] = {sizeof(cell), reinterpret_cast<cell>(ptr)};

		fclose(amx, params);
#else
		fclose(reinterpret_cast<FILE*>(ptr));
#endif
	}

	if(!f)
	{
		lua_pushnil(L);
		return 1;
	}

	auto &file = lua::newuserdata<luaL_Stream>(L);
	lua_pushvalue(L, mtindex);
	lua_setmetatable(L, -2);
	file.f = f;
	file.closef = [](lua_State *L)
	{
		auto &file = lua::touserdata<luaL_Stream>(L, 1);
		int res = fclose(file.f);
		return luaL_fileresult(L, res == 0, nullptr);
	};
	return 1;
}

int tofile(lua_State *L)
{
	luaL_checkudata(L, 1, LUA_FILEHANDLE);
	auto &file = lua::touserdata<luaL_Stream>(L, 1);
#ifdef _WIN32
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto ftemp = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	cell value = lua::marshal_file(L, file.f, amx, ftemp);
#else
	cell value = lua::marshal_file(L, file.f, nullptr, nullptr);
#endif
	if(value == 0)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
	return 1;
}

int closefile(lua_State *L)
{
	auto file = lua::checklightudata(L, 1);
#ifdef _WIN32
	auto amx = reinterpret_cast<AMX*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto fclose = reinterpret_cast<AMX_NATIVE>(lua_touserdata(L, lua_upvalueindex(2)));
	cell params[] = {sizeof(cell), reinterpret_cast<cell>(file)};

	fclose(amx, params);
#else
	fclose(reinterpret_cast<FILE*>(file));
#endif
	return 0;
}

void lua::interop::init_file(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

#ifdef _WIN32
	auto fclose = reinterpret_cast<void*>(find_native(amx, "fclose"));
	auto fseek = reinterpret_cast<void*>(find_native(amx, "fseek"));
	auto ftemp = reinterpret_cast<void*>(find_native(amx, "ftemp"));

	lua_pushlightuserdata(L, amx);
	lua_pushlightuserdata(L, fseek);
	lua_pushlightuserdata(L, fclose);
	lua_pushcclosure(L, asfile<true>, 3);
	lua_setfield(L, table, "asfile");

	lua_pushlightuserdata(L, amx);
	lua_pushlightuserdata(L, fseek);
	lua_pushcclosure(L, asfile<false>, 2);
	lua_setfield(L, table, "asnewfile");

	lua_pushlightuserdata(L, amx);
	lua_pushlightuserdata(L, fclose);
	lua_pushcclosure(L, closefile, 2);
	lua_setfield(L, table, "closefile");

	lua_pushlightuserdata(L, amx);
	lua_pushlightuserdata(L, ftemp);
	lua_pushcclosure(L, tofile, 2);
	lua_setfield(L, table, "tofile");
#else
	lua_pushcfunction(L, asfile<true>);
	lua_setfield(L, table, "asfile");

	lua_pushcfunction(L, asfile<false>);
	lua_setfield(L, table, "asnewfile");

	lua_pushcfunction(L, tofile);
	lua_setfield(L, table, "tofile");

	lua_pushcfunction(L, closefile);
	lua_setfield(L, table, "closefile");
#endif
}
