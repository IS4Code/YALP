/** YALP v0.1 by IllidanS4 **/
//github.com/IllidanS4/YALP

native Lua:lua_newstate();
native lua_dostring(Lua:L, str[]);
native bool:lua_close(Lua:L);
