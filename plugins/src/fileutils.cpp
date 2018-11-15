#include "fileutils.h"
#include "lua_utils.h"

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

FILE *lua::marshal_file(lua_State *L, cell value, int fseek)
{
	FILE *f;

#ifdef _WIN32
	sfp_handle = nullptr;
	sfp_thread = std::this_thread::get_id();
	lua_pushvalue(L, fseek);
	lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
	lua_pushlightuserdata(L, reinterpret_cast<void*>(0));
	lua_pushlightuserdata(L, reinterpret_cast<void*>(1));
	{
		subhook_guard<decltype(SetFilePointer)> guard(&SetFilePointer, &HookSetFilePointer, sfk_trampoline);
		lua_call(L, 3, 0);
	}

	HANDLE hfile;
	if(sfp_handle == nullptr || !DuplicateHandle(GetCurrentProcess(), sfp_handle, GetCurrentProcess(), &hfile, 0, TRUE, DUPLICATE_SAME_ACCESS))
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
		f = fdopen(fd, "r+");
		if(!f)
		{
			close(fd);
		}
	}
#endif
	return f;
}

#ifdef _WIN32
static decltype(&CreateFileA) cf_trampoline;
static HANDLE cf_handle;
static std::thread::id cf_thread;

HANDLE WINAPI HookCreateFileA(_In_ LPCSTR lpFileName, _In_ DWORD dwDesiredAccess, _In_ DWORD dwShareMode, _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes, _In_ DWORD dwCreationDisposition, _In_ DWORD dwFlagsAndAttributes, _In_opt_ HANDLE hTemplateFile)
{
	if(cf_thread == std::this_thread::get_id())
	{
		return cf_handle;
	}
	return cf_trampoline(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
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

	cf_handle = hfile;
	cf_thread = std::this_thread::get_id();
	lua_pushvalue(L, ftemp);
	{
		subhook_guard<decltype(CreateFileA)> guard(&CreateFileA, &HookCreateFileA, cf_trampoline);
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
	value = reinterpret_cast<cell>(fdopen(fd, "r+"));
	if(!value)
	{
		close(fd);
	}
#endif
	return value;
}
