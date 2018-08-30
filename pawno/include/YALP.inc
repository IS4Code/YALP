/** YALP v0.1 by IllidanS4 **/
//github.com/IllidanS4/YALP

#if defined _inc_YALP
#undef _inc_YALP
#endif
#if defined _YALP_included
#endinput
#endif
#define _YALP_included

#include <file>

enum lua_lib (<<= 1)
{
    lua_lib_base = 1,
    lua_lib_package,
    lua_lib_coroutine,
    lua_lib_table,
    lua_lib_io,
    lua_lib_os,
    lua_lib_string,
    lua_lib_math,
    lua_lib_utf8,
    lua_lib_debug,
    lua_lib_interop,
    lua_lib_timer,
    lua_lib_remote,
}

const lua_lib:lua_baselibs = lua_lib_base | lua_lib_package | lua_lib_coroutine | lua_lib_table | lua_lib_string | lua_lib_math;
const lua_lib:lua_newlibs = lua_lib_interop | lua_lib_timer | lua_lib_remote;

native Lua:lua_newstate(lua_lib:load=lua_baselibs, lua_lib:preload=lua_newlibs);
native lua_dostring(Lua:L, str[]);
native bool:lua_close(Lua:L);
native lua_load(Lua:L, const reader[], data, bufsize=-1, chunkname[]="");
native lua_pcall(Lua:L, nargs, nresults, errfunc=0);
native lua_call(Lua:L, nargs, nresults);
native lua_stackdump(Lua:L, depth=-1);
native lua_gettop(Lua:L);
native lua_settop(Lua:L, top);
native lua_tostring(Lua:L, idx, buffer[], size=sizeof(buffer));

stock lua_dofile(Lua:L, const name[], bufsize=-1)
{
    new err = lua_loadfile(L, name, bufsize);
    if(err) return err;
    return lua_pcall(L, 0, -1, 0);
}

stock lua_loadfile(Lua:L, const name[], bufsize=-1)
{
    new File:file = fopen(name, io_read);
    if(!file) return 7;
    new chunkname[64] = "@";
    strcat(chunkname, name);
    new err = lua_load(L, #_lua_freader, _:file, bufsize, chunkname);
    fclose(file);
    return err;
}

forward _lua_freader(Lua:L, buffer[], File:file, size);
public _lua_freader(Lua:L, buffer[], File:file, size)
{
    const cellsize = cellbits / 8;
    size = fblockread(file, buffer, size) * cellsize;
    if(size == 0)
    {
        size = flength(file) % cellsize;
        if(size != 0)
        {
            fseek(file, -size, seek_current);
            for(new i = 1; i <= size; i++)
            {
                buffer{cellsize - i} = fgetchar(file, 0, false);
            }
            return -size;
        }
    }
    return size;
}
