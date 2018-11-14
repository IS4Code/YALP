#include "result.h"
#include "lua_utils.h"

#include <stdio.h>
#ifdef _WIN32
#include "subhook/subhook.h"
#include <io.h>
#include <fcntl.h>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#else
#include <unistd.h>
#endif

int asnone(lua_State *L)
{
	lua::checklightudata(L, 1);
	return 0;
}

int asnil(lua_State *L)
{
	lua::checklightudata(L, 1);
	lua_pushnil(L);
	return 1;
}

int asinteger(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	lua_pushinteger(L, reinterpret_cast<cell>(ptr));
	return 1;
}

int asuinteger(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	lua_pushinteger(L, reinterpret_cast<ucell>(ptr));
	return 1;
}

int asboolean(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	lua_pushboolean(L, !!ptr);
	return 1;
}

int asfloat(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	lua_pushnumber(L, amx_ctof(reinterpret_cast<cell&>(ptr)));
	return 1;
}

int asoffset(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	size_t ofs = reinterpret_cast<size_t>(ptr);
	if(ofs % sizeof(cell) != 0)
	{
		return luaL_argerror(L, 1, "not a valid offset");
	}
	lua_pushinteger(L, ofs / sizeof(cell) + 1);
	return 1;
}

int ashandle(lua_State *L)
{
	auto ptr = lua::checklightudata(L, 1);
	if(!ptr)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushlightuserdata(L, ptr);
	return 1;
}

#ifdef _WIN32
static decltype(&SetFilePointer) sfk_trampoline;
static HANDLE sfp_handle;
static std::thread::id sfp_thread;

DWORD WINAPI HookSetFilePointer(_In_ HANDLE hFile, _In_ LONG lDistanceToMove, _Inout_opt_ PLONG lpDistanceToMoveHigh, _In_ DWORD dwMoveMethod)
{
	if(sfp_thread == std::this_thread::get_id())
	{
		sfp_handle = hFile;
	}
	return sfk_trampoline(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

template <class Func>
class subhook_guard
{
	subhook_t hook;

public:
	subhook_guard(Func *src, Func *dst, Func *&trampoline) : hook(subhook_new(reinterpret_cast<void*>(src), reinterpret_cast<void*>(dst), {}))
	{
		trampoline = reinterpret_cast<Func*>(subhook_get_trampoline(hook));
		subhook_install(hook);
	}

	~subhook_guard()
	{
		subhook_remove(hook);
		subhook_free(hook);
	}
};
#endif

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

	FILE *f = nullptr;

#ifdef _WIN32
	sfp_handle = nullptr;
	sfp_thread = std::this_thread::get_id();

	{
		subhook_guard<decltype(SetFilePointer)> guard(&SetFilePointer, &HookSetFilePointer, sfk_trampoline);

		lua_pushvalue(L, lua_upvalueindex(1)); //fseek
		lua_pushvalue(L, 1);
		lua_pushlightuserdata(L, reinterpret_cast<void*>(0));
		lua_pushlightuserdata(L, reinterpret_cast<void*>(1));
		lua_call(L, 3, 0);
	}

	HANDLE hfile;
	if(sfp_handle == nullptr || !DuplicateHandle(GetCurrentProcess(), sfp_handle, GetCurrentProcess(), &hfile, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		return lua::argerror(L, 1, "invalid file handle");
	}

	if(DestroyOriginal)
	{
		lua_pushvalue(L, lua_upvalueindex(2)); //fclose
		lua_pushvalue(L, 1);
		lua_call(L, 1, 0);
	}

	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hfile), _O_RDWR);
	if(fd != -1)
	{
		f = _fdopen(fd, "r+");
		if(!f)
		{
			_close(fd);
		}
	}else{
		CloseHandle(hfile);
	}
#else
	int fd = fileno(reinterpret_cast<FILE*>(ptr));
	if(fd == -1)
	{
		return lua::argerror(L, 1, "invalid file handle");
	}
	fd = dup(fd);

	if(DestroyOriginal)
	{
		lua_pushvalue(L, lua_upvalueindex(1)); //fclose
		lua_pushvalue(L, 1);
		lua_call(L, 1, 0);
	}

	if(fd != -1)
	{
		f = fdopen(fd, "r+");
		if(!f)
		{
			close(fd);
		}
	}
#endif

	if(!f)
	{
		return luaL_error(L, "file descriptor cannot be created");
	}

	auto &file = lua::newuserdata<luaL_Stream>(L);
	lua_pushvalue(L, mtindex);
	lua_setmetatable(L, -2);
	file.f = f;
	file.closef = [](lua_State *L)
	{
		auto &file = lua::touserdata<luaL_Stream>(L, 1);
		int res = fclose(file.f);
		return luaL_fileresult(L, (res == 0), nullptr);
	};
	return 1;
}

void lua::interop::init_result(lua_State *L, AMX *amx)
{
	int table = lua_absindex(L, -1);

	lua_pushcfunction(L, asnone);
	lua_setfield(L, table, "asnone");
	lua_pushcfunction(L, asnil);
	lua_setfield(L, table, "asnil");
	lua_pushcfunction(L, asinteger);
	lua_setfield(L, table, "asinteger");
	lua_pushcfunction(L, asuinteger);
	lua_setfield(L, table, "asuinteger");
	lua_pushcfunction(L, asboolean);
	lua_setfield(L, table, "asboolean");
	lua_pushcfunction(L, asfloat);
	lua_setfield(L, table, "asfloat");
	lua_pushcfunction(L, asoffset);
	lua_setfield(L, table, "asoffset");
	lua_pushcfunction(L, ashandle);
	lua_setfield(L, table, "ashandle");

#ifdef _WIN32
	lua_getfield(L, table, "getnative");
	lua::pushstring(L, "fseek");
	lua_call(L, 1, 1);
	lua_getfield(L, table, "getnative");
	lua::pushstring(L, "fclose");
	lua_call(L, 1, 1);
	lua_pushcclosure(L, asfile<true>, 2);
	lua_setfield(L, table, "asfile");

	lua_getfield(L, table, "getnative");
	lua::pushstring(L, "fseek");
	lua_call(L, 1, 1);
	lua_pushcclosure(L, asfile<false>, 1);
	lua_setfield(L, table, "asnewfile");
#else
	lua_getfield(L, table, "getnative");
	lua::pushstring(L, "fclose");
	lua_call(L, 1, 1);
	lua_pushcclosure(L, asfile<true>, 1);
	lua_setfield(L, table, "asfile");

	lua_pushcfunction(L, asfile<false>);
	lua_setfield(L, table, "asnewfile");
#endif
}
