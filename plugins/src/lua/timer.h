#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include "lua/lua.hpp"

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
