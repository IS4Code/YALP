/** YALP v1.1.1 by IllidanS4 **/
//github.com/IllidanS4/YALP

#if defined _inc_YALP
#undef _inc_YALP
#endif
#if defined _YALP_included
#endinput
#endif
#define _YALP_included

#include <file>
#include <string>

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

const lua_lib:lua_baselibs = lua_lib_base | lua_lib_coroutine | lua_lib_table | lua_lib_string | lua_lib_math;
const lua_lib:lua_newlibs = lua_lib_interop | lua_lib_timer | lua_lib_remote;

enum lua_load_mode (<<= 1)
{
    lua_load_text = 1,
    lua_load_binary,
}

native Lua:lua_newstate(lua_lib:load=lua_baselibs, lua_lib:preload=lua_newlibs, memlimit=-1);
native bool:lua_dostring(Lua:L, const str[]);
native bool:lua_close(Lua:L);
native lua_status:lua_load(Lua:L, const reader[], data, bufsize=-1, chunkname[]="");

const LUA_MULTRET = -1;

const lua_status:LUA_OK = lua_status:0;
const lua_status:LUA_YIELD = lua_status:1;
const lua_status:LUA_ERRRUN = lua_status:2;
const lua_status:LUA_ERRSYNTAX = lua_status:3;
const lua_status:LUA_ERRMEM = lua_status:4;
const lua_status:LUA_ERRGCMM = lua_status:5;
const lua_status:LUA_ERRERR = lua_status:6;
const lua_status:LUA_ERRFILE = lua_status:7;

native lua_status:lua_pcall(Lua:L, nargs, nresults, errfunc=0);
native lua_call(Lua:L, nargs, nresults);
native lua_stackdump(Lua:L, depth=-1);
native lua_tostring(Lua:L, idx, buffer[], size=sizeof(buffer), bool:pack=false);
native lua_bind(Lua:L);
native lua_pushpfunction(Lua:L, const f[]);
native lua_status:lua_loadstream(Lua:L, File:file, const chunkname[], lua_load_mode:mode=lua_load_text);
native LuaLoader:lua_loader(Lua:L, const chunkname[], lua_load_mode:mode=lua_load_text);
native lua_status:lua_write(LuaLoader:stream, const data[], size=-1);

stock lua_status:lua_loadfile(Lua:L, const name[], lua_load_mode:mode=lua_load_text)
{
    new File:file = fopen(name, io_read);
    if(!file) return LUA_ERRFILE;
    new chunkname[64] = "@";
    strcat(chunkname, name);
    new lua_status:err = lua_loadstream(L, file, chunkname, mode);
    fclose(file);
    return err;
}

stock lua_status:lua_dofile(Lua:L, const name[], lua_load_mode:mode=lua_load_text)
{
    new lua_status:err = lua_loadfile(L, name, mode);
    if(err != LUA_OK) return err;
    return lua_pcall(L, 0, -1, 0);
}

const lua_type:LUA_TNONE = lua_type:-1;
const lua_type:LUA_TNIL = lua_type:0;
const lua_type:LUA_TBOOLEAN = lua_type:1;
const lua_type:LUA_TLIGHTUSERDATA = lua_type:2;
const lua_type:LUA_TNUMBER = lua_type:3;
const lua_type:LUA_TSTRING = lua_type:4;
const lua_type:LUA_TTABLE = lua_type:5;
const lua_type:LUA_TFUNCTION = lua_type:6;
const lua_type:LUA_TUSERDATA = lua_type:7;
const lua_type:LUA_TTHREAD = lua_type:8;

native lua_tointeger(Lua:L, idx);
native Float:lua_tonumber(Lua:L, idx);
native lua_pop(Lua:L, n);
native lua_type:lua_gettable(Lua:L, idx);
native lua_type:lua_getfield(Lua:L, idx, const k[]);
native lua_type:lua_getglobal(Lua:L, const name[]);
native lua_settable(Lua:L, idx);
native lua_setfield(Lua:L, idx, const k[]);
native lua_setglobal(Lua:L, const name[]);
native lua_len(Lua:L, idx);
native lua_pushstring(Lua:L, const s[]);
native lua_pushfstring(Lua:L, const fmt[], ...);
native Pointer:lua_pushuserdata(Lua:L, const data[], size=sizeof(data));
native lua_getuserdata(Lua:L, idx, data[], size=sizeof(data));
native lua_setuserdata(Lua:L, idx, const data[], size=sizeof(data));

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
native lua_type:lua_type(Lua:L, idx);

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
native lua_pushlightuserdata(Lua:L, {_,Pointer}:p);
native lua_pushthread(Lua:L);

native lua_type:lua_rawget(Lua:L, idx);
native lua_type:lua_rawgeti(Lua:L, idx, n);
native lua_type:lua_rawgetp(Lua:L, idx, Pointer:p);

native lua_createtable(Lua:L, narr, nrec);
native Pointer:lua_newuserdata(Lua:L, size);
native bool:lua_getmetatable(Lua:L, objindex);
native lua_type:lua_getuservalue(Lua:L, idx);

native lua_rawset(Lua:L, idx);
native lua_rawseti(Lua:L, idx, n);
native lua_rawsetp(Lua:L, idx, Pointer:p);
native lua_setmetatable(Lua:L, objindex);
native lua_setuservalue(Lua:L, idx);

native lua_status:lua_resume(Lua:L, Lua:from, narg);
native lua_status:lua_status(Lua:L);

const lua_gc_command:LUA_GCSTOP = lua_gc_command:0;
const lua_gc_command:LUA_GCRESTART = lua_gc_command:1;
const lua_gc_command:LUA_GCCOLLECT = lua_gc_command:2;
const lua_gc_command:LUA_GCCOUNT = lua_gc_command:3;
const lua_gc_command:LUA_GCCOUNTB = lua_gc_command:4;
const lua_gc_command:LUA_GCSTEP = lua_gc_command:5;
const lua_gc_command:LUA_GCSETPAUSE = lua_gc_command:6;
const lua_gc_command:LUA_GCSETSTEPMUL = lua_gc_command:7;
const lua_gc_command:LUA_GCISRUNNING = lua_gc_command:9;

native lua_gc(Lua:L, lua_gc_command:what, data);

native bool:lua_next(Lua:L, idx);

const lua_arith_op:LUA_OPADD = lua_arith_op:0;
const lua_arith_op:LUA_OPSUB = lua_arith_op:1;
const lua_arith_op:LUA_OPMUL = lua_arith_op:2;
const lua_arith_op:LUA_OPMOD = lua_arith_op:3;
const lua_arith_op:LUA_OPPOW = lua_arith_op:4;
const lua_arith_op:LUA_OPDIV = lua_arith_op:5;
const lua_arith_op:LUA_OPIDIV = lua_arith_op:6;
const lua_arith_op:LUA_OPBAND = lua_arith_op:7;
const lua_arith_op:LUA_OPBOR = lua_arith_op:8;
const lua_arith_op:LUA_OPBXOR = lua_arith_op:9;
const lua_arith_op:LUA_OPSHL = lua_arith_op:10;
const lua_arith_op:LUA_OPSHR = lua_arith_op:11;
const lua_arith_op:LUA_OPUNM = lua_arith_op:12;
const lua_arith_op:LUA_OPBNOT = lua_arith_op:13;

native lua_arith(Lua:L, lua_arith_op:op);

const lua_rel_op:LUA_OPEQ = lua_rel_op:0;
const lua_rel_op:LUA_OPLT = lua_rel_op:1;
const lua_rel_op:LUA_OPLE = lua_rel_op:2;

native bool:lua_compare(Lua:L, idx1, idx2, lua_rel_op:op);
