#ifndef TAGS_H_INCLUDED
#define TAGS_H_INCLUDED

#include "lua/lua.hpp"
#include "sdk/amx/amx.h"

namespace lua
{
	namespace interop
	{
		void init_tags(lua_State *L, AMX *amx);
		bool amx_find_tag_id(AMX *amx, cell tag_id, char *tagname);
		bool amx_get_tag(AMX *amx, int index, char *tagname, cell *tag_id);
		bool amx_num_tags(AMX *amx, int *number);
	}
}

#endif
