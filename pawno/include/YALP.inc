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

native Lua:lua_newstate(lua_lib:load=lua_baselibs, lua_lib:preload=lua_newlibs, memlimit=-1);
native lua_dostring(Lua:L, str[]);
native bool:lua_close(Lua:L);
native lua_load(Lua:L, const reader[], data, bufsize=-1, chunkname[]="");
native lua_pcall(Lua:L, nargs, nresults, errfunc=0);
native lua_call(Lua:L, nargs, nresults);
native lua_stackdump(Lua:L, depth=-1);
native lua_tostring(Lua:L, idx, buffer[], size=sizeof(buffer));
native lua_bind(Lua:L);
native lua_pushpfunction(Lua:L, const f[]);

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

native lua_tointeger(Lua:L, idx);
native Float:lua_tonumber(Lua:L, idx);
native lua_pop(Lua:L, n);
native lua_gettable(Lua:L, idx);
native lua_getfield(Lua:L, idx, const k[]);
native lua_getglobal(Lua:L, const name[]);
native lua_settable(Lua:L, idx);
native lua_setfield(Lua:L, idx, const k[]);
native lua_setglobal(Lua:L, const name[]);
native lua_len(Lua:L, idx);
native lua_pushstring(Lua:L, const s[]);
native lua_pushfstring(Lua:L, const fmt[], ...);

//Directly ported
native Lua:lua_newthread(Lua:L);

native lua_version(Lua:L);

native lua_absindex(Lua:L, idx);
native lua_gettop(Lua:L);
native lua_settop(Lua:L, idx);
native lua_pushvalue(Lua:L, idx);
native lua_rotate(Lua:L, idx, n);
native lua_copy(Lua:L, fromidx, toidx);
native bool:lua_checkstack(Lua:L, n);

native lua_xmove(Lua:from, Lua:to, n);

native bool:lua_isnumber(Lua:L, idx);
native bool:lua_isstring(Lua:L, idx);
native bool:lua_iscfunction(Lua:L, idx);
native bool:lua_isinteger(Lua:L, idx);
native bool:lua_isuserdata(Lua:L, idx);
native lua_type(Lua:L, idx);

native bool:lua_toboolean(Lua:L, idx);
native lua_rawlen(Lua:L, idx);
native Pointer:lua_touserdata(Lua:L, idx);
native L:lua_tothread(Lua:L, idx);
native Pointer:lua_topointer(Lua:L, idx);

native bool:lua_rawequal(Lua:L, idx1, idx2);

native lua_pushnil(Lua:L);
native lua_pushnumber(Lua:L, Float:n);
native lua_pushinteger(Lua:L, n);
native lua_pushboolean(Lua:L, bool:b);
native lua_pushlightuserdata(Lua:L, Pointer:p);
native lua_pushthread(Lua:L);

native lua_rawget(Lua:L, idx);
native lua_rawgeti(Lua:L, idx, n);
native lua_rawgetp(Lua:L, idx, Pointer:p);

native lua_createtable(Lua:L, narr, nrec);
native Pointer:lua_newuserdata(Lua:L, sz);
native bool:lua_getmetatable(Lua:L, objindex);
native lua_getuservalue(Lua:L, idx);

native lua_rawset(Lua:L, idx);
native lua_rawseti(Lua:L, idx, n);
native lua_rawsetp(Lua:L, idx, Pointer:p);
native lua_setmetatable(Lua:L, objindex);
native lua_setuservalue(Lua:L, idx);

native lua_resume(Lua:L, Lua:from, narg);
native lua_status(Lua:L);

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCSETPAUSE		6
#define LUA_GCSETSTEPMUL	7
#define LUA_GCISRUNNING		9

native lua_gc(Lua:L, what, data);

native lua_next(Lua:L, idx);
