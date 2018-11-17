#include "fileutils.h"
#include "lua_utils.h"

#ifdef _WIN32
#include "subhook/subhook.h"
#include <io.h>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#ifdef _WIN32
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

#ifdef _WIN32
static thread_local struct {
	decltype(&SetFilePointer) trampoline;
	HANDLE handle;
} sfp_info;

DWORD WINAPI HookSetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	auto &info = sfp_info;
	info.handle = hFile;
	return info.trampoline(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}
#endif

FILE *lua::marshal_file(lua_State *L, cell value, int fseek)
{
	FILE *f;

#ifdef _WIN32
	auto &info = sfp_info;
	info.handle = nullptr;
	lua_pushvalue(L, fseek);
	lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
	lua_pushlightuserdata(L, reinterpret_cast<void*>(0));
	lua_pushlightuserdata(L, reinterpret_cast<void*>(1));
	{
		subhook_guard<decltype(SetFilePointer)> guard(&SetFilePointer, &HookSetFilePointer, info.trampoline);
		lua_call(L, 3, 0);
	}

	HANDLE hfile;
	if(info.handle == nullptr || !DuplicateHandle(GetCurrentProcess(), info.handle, GetCurrentProcess(), &hfile, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		return reinterpret_cast<FILE*>(lua::argerror(L, 1, "invalid file handle"));
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
	int fd = fileno(reinterpret_cast<FILE*>(value));
	if(fd == -1)
	{
		return reinterpret_cast<FILE*>(lua::argerror(L, 1, "invalid file handle"));
	}
	fd = dup(fd);

	if(fd != -1)
	{
		int flags = fcntl(fd, F_GETFL);
		const char *access = (flags & O_RDWR) ? "r+" : ((flags & O_WRONLY) ? "w" : "r");
		f = fdopen(fd, access);
		if(!f)
		{
			close(fd);
		}
	}
#endif
	return f;
}

#ifdef _WIN32
static thread_local struct {
	decltype(&CreateFileA) trampoline;
	HANDLE handle;
} cf_info;

HANDLE WINAPI HookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	auto &info = cf_info;
	if(info.handle)
	{
		return info.handle;
	}
	return info.trampoline(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}
#endif

cell lua::marshal_file(lua_State *L, FILE *file, int ftemp)
{
	cell value;
#ifdef _WIN32
	int fd = _fileno(file);
	if(fd == -1)
	{
		return 0;
	}
	HANDLE hfile = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
	if(hfile == nullptr || !DuplicateHandle(GetCurrentProcess(), hfile, GetCurrentProcess(), &hfile, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		return 0;
	}

	auto &info = cf_info;
	info.handle = hfile;
	lua_pushvalue(L, ftemp);
	{
		subhook_guard<decltype(CreateFileA)> guard(&CreateFileA, &HookCreateFileA, info.trampoline);
		lua_call(L, 0, 1);
	}
	value = reinterpret_cast<cell>(lua_touserdata(L, -1));
	lua_pop(L, 1);
#else
	int fd = fileno(file);
	if(fd == -1)
	{
		return 0;
	}
	fd = dup(fd);

	if(fd == -1)
	{
		return 0;
	}
	int flags = fcntl(fd, F_GETFL);
	const char *access = (flags & O_RDWR) ? "r+" : ((flags & O_WRONLY) ? "w" : "r");
	value = reinterpret_cast<cell>(fdopen(fd, access));
	if(!value)
	{
		close(fd);
	}
#endif
	return value;
}
