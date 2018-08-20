#ifndef REMOTE_H_INCLUDED
#define REMOTE_H_INCLUDED

#include "lua/lua.hpp"

namespace lua
{
	namespace remote
	{
		int loader(lua_State *L);
		void close();
	}
}

#endif
