#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include "lua/lualibs.h"

namespace lua
{
	namespace timer
	{
		int loader(lua_State *L);
		void close();
		void tick();
		bool pushyielded(lua_State *L, lua_State *from);
	}
}

#endif
