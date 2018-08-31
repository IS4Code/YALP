#ifndef TAGS_H_INCLUDED
#define TAGS_H_INCLUDED

#include "lua/lualibs.h"
#include "sdk/amx/amx.h"

#include <unordered_map>
#include <string>

namespace lua
{
	namespace interop
	{
		void init_tags(lua_State *L, AMX *amx, const std::unordered_map<cell, std::string> &init);
		bool amx_find_tag_id(AMX *amx, cell tag_id, char *tagname);
		bool amx_get_tag(AMX *amx, int index, char *tagname, cell *tag_id);
		bool amx_num_tags(AMX *amx, int *number);
	}
}

#endif
