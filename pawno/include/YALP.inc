/** YALP v0.1 by IllidanS4 **/
//github.com/IllidanS4/YALP

#if defined _inc_YALP
#undef _inc_YALP
#endif
#if defined _YALP_included
#endinput
#endif
#define _YALP_included

native Lua:lua_newstate();
native lua_dostring(Lua:L, str[]);
native bool:lua_close(Lua:L);
native lua_load(Lua:L, const reader[], data, bufsize=-1, chunkname[]="");
native lua_pcall(Lua:L, nargs, nresults, errfunc);
native lua_call(Lua:L, nargs, nresults);

stock lua_dofile(Lua:L, File:file, bufsize=-1, chunkname[]="")
{
    new err = lua_loadfile(L, file, bufsize, chunkname);
    if(err) return err;
    return lua_pcall(L, 0, -1, 0);
}

stock lua_loadfile(Lua:L, File:file, bufsize=-1, chunkname[]="")
{
    return lua_load(L, #_lua_freader, _:file, bufsize, chunkname);
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
