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
	}
}

#endif
