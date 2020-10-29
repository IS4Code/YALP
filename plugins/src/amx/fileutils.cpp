#include "fileutils.h"
#include "lua_utils.h"

#ifdef _WIN32
#include "amx/amxutils.h"
#include "subhook/subhook.h"
#include "subhook/subhook_private.h"
#include <io.h>
#include <thread>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#ifdef _WIN32
struct file_extra : amx::Extra
{
	AMX_NATIVE fseek = nullptr;
	AMX_NATIVE ftemp = nullptr;

	file_extra(AMX *amx) : amx::Extra(amx)
	{

	}
};
#endif

#ifdef _WIN32
template <class Func>
class subhook_guard;

template <class Ret, class... Args>
class subhook_guard<Ret(__stdcall)(Args...)>
{
	typedef Ret(__stdcall func_type)(Args...);

	subhook_t hook;

public:
	subhook_guard(func_type *src, func_type *dst) : hook(subhook_new(reinterpret_cast<void*>(src), reinterpret_cast<void*>(dst), {}))
	{
		subhook_install(hook);
	}

	Ret trampoline(Args...args)
	{
		auto ptr = subhook_get_trampoline(hook);
		if(ptr)
		{
			return reinterpret_cast<func_type*>(ptr)(std::forward<Args>(args)...);
		}
		
		auto src = subhook_get_src(hook);
		auto dst = subhook_read_dst(src);
		auto olddst = subhook_get_dst(hook);
		if(dst != olddst)
		{
			hook->dst = dst;
			subhook_remove(hook);
			auto ret = reinterpret_cast<func_type*>(src)(std::forward<Args>(args)...);
			subhook_install(hook);
			hook->dst = olddst;
			return ret;
		}else if(!dst)
		{
			return reinterpret_cast<func_type*>(src)(std::forward<Args>(args)...);
		}else{
			subhook_remove(hook);
			auto ret = reinterpret_cast<func_type*>(src)(std::forward<Args>(args)...);
			subhook_install(hook);
			return ret;
		}
	}

	~subhook_guard()
	{
		subhook_remove(hook);
		subhook_free(hook);
	}
};

static thread_local struct {
	subhook_guard<decltype(SetFilePointer)> *hook;
	HANDLE handle;
} sfp_info;

DWORD WINAPI HookSetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	auto &info = sfp_info;
	info.handle = hFile;
	return info.hook->trampoline(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}
#endif

bool amx::FileLoad(cell value, AMX *amx, FILE *&f)
{
#ifdef _WIN32
	auto &info = sfp_info;
	info.handle = nullptr;
	{
		subhook_guard<decltype(SetFilePointer)> guard(&SetFilePointer, &HookSetFilePointer);
		info.hook = &guard;
		cell params[] = {3 * sizeof(cell), value, 0, 1};
		amx::GetHandle(amx)->get_extra<file_extra>().fseek(amx, params);
	}

	HANDLE hfile;
	if(info.handle == nullptr || !DuplicateHandle(GetCurrentProcess(), info.handle, GetCurrentProcess(), &hfile, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		return false;
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
		return false;
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
	return true;
}

#ifdef _WIN32
static thread_local struct {
	subhook_guard<decltype(CreateFileA)> *hook;
	HANDLE handle;
} cf_info;

HANDLE WINAPI HookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	auto &info = cf_info;
	if(info.handle)
	{
		return info.handle;
	}
	return info.hook->trampoline(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}
#endif

cell amx::FileStore(FILE *file, AMX *amx)
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
	{
		subhook_guard<decltype(CreateFileA)> guard(&CreateFileA, &HookCreateFileA);
		info.hook = &guard;
		cell params[] = {0};
		value = amx::GetHandle(amx)->get_extra<file_extra>().ftemp(amx, params);
	}
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

void amx::RegisterNatives(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number)
{
#ifdef _WIN32
	for(int i = 0; nativelist[i].name != nullptr && (i < number || number == -1); i++)
	{
		if(!std::strcmp(nativelist[i].name, "fseek"))
		{
			amx::GetHandle(amx)->get_extra<file_extra>().fseek = nativelist[i].func;
		}else if(!std::strcmp(nativelist[i].name, "ftemp"))
		{
			amx::GetHandle(amx)->get_extra<file_extra>().ftemp = nativelist[i].func;
		}
	}
#endif
}
