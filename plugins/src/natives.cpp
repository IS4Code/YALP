#include "natives.h"
#include "amxutils.h"
#include "lua_api.h"
#include "lua_utils.h"
#include "lua_adapt.h"

#include <string>
#include <iomanip>
#include <bitset>
#include <cctype>
#include <cstring>
#include <sstream>

// native Lua:lua_newstate(lua_lib:load=lua_baselibs, lua_lib:preload=lua_newlibs, memlimit=-1);
static cell AMX_NATIVE_CALL n_lua_newstate(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 0)) return 0;
	auto L = luaL_newstate();
	if(L)
	{
		lua_atpanic(L, lua::atpanic);
		long long memlimit = optparam(3, -1) * 1024;

		if(memlimit >= 0)
		{
			long long total = lua_gc(L, LUA_GCCOUNT, 0);
			total = total * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);

			void *ud;
			auto oldalloc = lua_getallocf(L, &ud);
			lua::setallocf(L, [=](void *ptr, size_t osize, size_t nsize) mutable
			{
				if(total + nsize > memlimit)
				{
					if(ptr == nullptr || nsize > osize)
					{
						return static_cast<void*>(nullptr);
					}
				}
				void *ret = oldalloc(ud, ptr, osize, nsize);
				if(ret)
				{
					if(ptr != nullptr)
					{
						total -= osize;
					}
					total += nsize;
				}
				return ret;
			});
		}

		lua::initlibs(L, optparam(1, 0xCD), optparam(2, 0x1C00));
	}
	return reinterpret_cast<cell>(L);
}

// native lua_dostring(Lua:L, const str[]);
static cell AMX_NATIVE_CALL n_lua_dostring(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	char *str;
	amx_StrParam(amx, params[2], str);
		
	return luaL_dostring(L, str);
}

// native bool:lua_close(Lua:L);
static cell AMX_NATIVE_CALL n_lua_close(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 1)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	if(lua::active(L))
	{
		logprintf("cannot close a running Lua state");
		amx_RaiseError(amx, AMX_ERR_NATIVE);
		return 0;
	}
	lua_close(L);
	lua::cleanup(L);
	return 1;
}

// native lua_pcall(Lua:L, nargs, nresults, errfunc=0);
static cell AMX_NATIVE_CALL n_lua_pcall(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 3)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	return lua_pcall(L, params[2], params[3], optparam(4, 0));
}

// native lua_call(Lua:L, nargs, nresults);
static cell AMX_NATIVE_CALL n_lua_call(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 3)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	switch(lua_pcall(L, params[2], params[3], 0))
	{
		case LUA_OK:
			break;
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

// native lua_load(Lua:L, const reader[], data, bufsize, chunkname[]="");
static cell AMX_NATIVE_CALL n_lua_load(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 4)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	const char *reader;
	amx_StrParam(amx, params[2], reader);

	cell data = params[3];
	cell cellsize = params[4];
	if(cellsize < 0) cellsize = 128;

	const char *chunkname;
	amx_OptStrParam(amx, 5, chunkname, nullptr);

	int error;

	cell buffer, *addr;
	error = amx_Allot(amx, cellsize, &buffer, &addr);
	if(error != AMX_ERR_NONE)
	{
		amx_RaiseError(amx, error);
		return 0;
	}
	
	int index;
	error = amx_FindPublic(amx, reader, &index);
	if(error != AMX_ERR_NONE)
	{
		amx_RaiseError(amx, error);
		return 0;
	}
	
	bool last = false;
	int result = lua::load(L, [&](lua_State *L, size_t *size)
	{
		if(last)
		{
			*size = 0;
		}else{
			amx_Push(amx, cellsize);
			amx_Push(amx, data);
			amx_Push(amx, buffer);
			amx_Push(amx, reinterpret_cast<cell>(L));
			cell rsize;
			if(amx_Exec(amx, &rsize, index) == AMX_ERR_NONE)
			{
				if(rsize < 0)
				{
					*size = -rsize;
					last = true;
				}else{
					*size = rsize;
				}
			}else{
				*size = 0;
			}
		}
		return reinterpret_cast<const char*>(addr);
	}, chunkname, nullptr);

	amx_Release(amx, buffer);

	return result;
}

// native lua_stackdump(Lua:L, depth=-1);
static cell AMX_NATIVE_CALL n_lua_stackdump(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 1)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	int top = lua_gettop(L);
	int bottom = 1;
	cell depth = optparam(2, -1);
	if(depth >= 0 && depth <= top) bottom = top - depth + 1;
	bool tostring = lua_getglobal(L, "tostring") == LUA_TFUNCTION;
	if(!tostring) lua_pop(L, 1);
	for(int i = top; i >= bottom; i--)
	{
		if(!tostring)
		{
			if(auto str = lua_tostring(L, i))
			{
				logprintf("%s", str);
			}else{
				logprintf("%s", luaL_typename(L, i));
			}
		}else{
			lua_pushvalue(L, -1);
			lua_pushvalue(L, i);
			lua_pcall(L, 1, 1, 0);
			if(auto str = lua_tostring(L, -1))
			{
				logprintf("%s", str);
			}else{
				logprintf("%s", luaL_typename(L, i));
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_loopback(AMX *amx, cell *params)
{
	int index, error;
	error = amx_FindPublic(amx, "#lua", &index);
	if(error != AMX_ERR_NONE)
	{
		amx_RaiseError(amx, error);
		return 0;
	}
	size_t numargs = params[0] / sizeof(cell);
	for(size_t i = numargs; i >= 1; i--)
	{
		error = amx_Push(amx, params[i]);
		if(error != AMX_ERR_NONE)
		{
			amx_RaiseError(amx, error);
			return 0;
		}
	}
	cell retval;
	error = amx_Exec(amx, &retval, index);
	if(error != AMX_ERR_NONE)
	{
		amx_RaiseError(amx, error);
	}
	return retval;
}

static cell AMX_NATIVE_CALL n_lua_tostring(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 4)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	int idx = lua_absindex(L, params[2]);
	size_t len;
	auto str = lua_tolstring(L, idx, &len);
	bool pop = false;
	if(!str)
	{
		str = luaL_tolstring(L, idx, &len);
		pop = true;
	}
		
	cell *addr;
	amx_GetAddr(amx, params[3], &addr);

	amx_SetString(addr, str, true, false, params[4]);

	if(pop)
	{
		lua_pop(L, 1);
	}

	return len;
}

static cell AMX_NATIVE_CALL n_lua_tointeger(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	return static_cast<cell>(lua_tointeger(L, params[2]));
}

static cell AMX_NATIVE_CALL n_lua_tonumber(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	float fval = static_cast<float>(lua_tonumber(L, params[2]));
	return amx_ftoc(fval);
}

static cell AMX_NATIVE_CALL n_lua_pop(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	lua_pop(L, params[2]);
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_bind(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 1)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	cell ret = lua::init_bind(L, amx);
	if(ret)
	{
		amx_RaiseError(amx, AMX_ERR_SLEEP);
		return ret;
	}else{
		if(lua_gettop(L) == 0 || !lua_isfunction(L, -1))
		{
			lua::pushliteral(L, "a function must be provided");
		}else{
			lua::pushliteral(L, "this Lua state is already bound to an AMX");
		}
	}
	return 0;
}


static cell AMX_NATIVE_CALL n_lua_pushpfunction(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	char *name;
	amx_StrParam(amx, params[2], name);

	lua::pushuserdata(L, std::weak_ptr<AMX*>(amx::GetHandle(amx)));
	lua_pushstring(L, name);

	lua_pushcclosure(L, [](lua_State *L)
	{
		auto name = lua_tostring(L, lua_upvalueindex(2));
		if(auto lock = lua::touserdata<std::weak_ptr<AMX*>>(L, lua_upvalueindex(1)).lock())
		{
			auto amx = *lock;
			int index, error;
			error = amx_FindPublic(amx, name, &index);
			if(error != AMX_ERR_NONE)
			{
				return luaL_error(L, "function '%s' cannot be found in the AMX", name);
			}
			amx_Push(amx, reinterpret_cast<cell>(L));
			cell retval;
			{
				lua::jumpguard guard(L);
				error = amx_Exec(amx, &retval, index);
			}
			if(error != AMX_ERR_NONE)
			{
				return lua::amx_error(L, error);
			}
			return retval;
		}else{
			return luaL_error(L, "function '%s' cannot be found because the AMX no longer exists", name);
		}
	}, 2);

	return 0;
}

static cell AMX_NATIVE_CALL n_lua_gettable(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	switch(lua::pgettable(L, params[2]))
	{
		case LUA_OK:
			return lua_type(L, -1);
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_getfield(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 3)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	char *name;
	amx_StrParam(amx, params[3], name);

	switch(lua::pgetfield(L, params[2], name))
	{
		case LUA_OK:
			return lua_type(L, -1);
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_getglobal(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	char *name;
	amx_StrParam(amx, params[2], name);

	lua_pushglobaltable(L);
	switch(lua::pgetfield(L, -1, name))
	{
		case LUA_OK:
			lua_remove(L, -2);
			return lua_type(L, -1);
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 2);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 2);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_settable(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	switch(lua::psettable(L, params[2]))
	{
		case LUA_OK:
			break;
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_setfield(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 3)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	char *name;
	amx_StrParam(amx, params[3], name);
	
	switch(lua::psetfield(L, params[2], name))
	{
		case LUA_OK:
			break;
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_setglobal(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	char *name;
	amx_StrParam(amx, params[2], name);

	lua_pushglobaltable(L);
	lua_insert(L, -2);
	switch(lua::psetfield(L, -2, name))
	{
		case LUA_OK:
			lua_pop(L, 1);
			break;
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 2);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 2);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_len(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	cell len;
	switch(lua::plen(L, params[2]))
	{
		case LUA_OK:
			len = (cell)lua_tointeger(L, -1);
			lua_pop(L, 1);
			return len;
		case LUA_ERRMEM:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_MEMORY);
			break;
		default:
			logprintf("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
			amx_RaiseError(amx, AMX_ERR_GENERAL);
			break;
	}
	return 0;
}

static cell AMX_NATIVE_CALL n_lua_pushstring(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);
	char *str;
	amx_StrParam(amx, params[2], str);

	lua_pushstring(L, str);
	return 0;
}

void add_query(std::string &buf, const cell *arg)
{
	int len;
	amx_StrLen(arg, &len);

	char *str = reinterpret_cast<char*>(alloca(len + 1));
	amx_GetString(str, arg, 0, len + 1);

	buf.reserve(len);

	const char *start = str;
	while(len--)
	{
		if(*str == '\'')
		{
			buf.append(start, str - start + 1);
			buf.push_back('\'');
			start = str + 1;
		}
		str++;
	}
	buf.append(start, str - start);
}

namespace aux
{
	void push_args(std::ostream &ostream)
	{

	}

	template <class Arg, class... Args>
	void push_args(std::ostream &ostream, Arg &&arg, Args&&... args)
	{
		ostream << std::forward<Arg>(arg);
		push_args(ostream, std::forward<Args>(args)...);
	}

	template <class Obj, class... Args>
	std::string to_string(Obj &&obj, Args&&... args)
	{
		std::ostringstream ostream;
		push_args(ostream, std::forward<Args>(args)...);
		ostream << std::forward<Obj>(obj);
		return ostream.str();
	}

	template <class NumType>
	NumType parse_num(const char *str, size_t &pos)
	{
		bool neg = str[pos] == '-';
		if(neg) pos++;
		NumType val = 0;
		char c;
		while(std::isdigit(c = str[pos++]))
		{
			val = (val * 10) + (c - '0');
		}
		return neg ? -val : val;
	}
}

void add_format(std::string &buf, const char *begin, const char *end, cell *arg)
{
	ptrdiff_t flen = end - begin;
	switch(*end)
	{
		case 's':
		{
			int len;
			amx_StrLen(arg, &len);
			size_t begin = buf.size();
			buf.resize(begin + len, '\0');
			amx_GetString(&buf[begin], arg, 0, len + 1);
		}
		break;
		case 'q':
		{
			add_query(buf, arg);
		}
		break;
		case 'd':
		case 'i':
		{
			buf.append(std::to_string(*arg));
		}
		break;
		case 'f':
		{
			if(*begin == '.')
			{
				size_t pos = 0;
				auto precision = aux::parse_num<std::streamsize>(begin + 1, pos);
				buf.append(aux::to_string(amx_ctof(*arg), std::setprecision(precision), std::fixed));
			}else{
				buf.append(std::to_string(amx_ctof(*arg)));
			}
		}
		break;
		case 'c':
		{
			buf.append(1, static_cast<char>(*arg));
		}
		break;
		case 'h':
		case 'x':
		{
			buf.append(aux::to_string(*arg, std::hex, std::uppercase));
		}
		break;
		case 'o':
		{
			buf.append(aux::to_string(*arg, std::oct));
		}
		break;
		case 'b':
		{
			std::bitset<8> bits(*arg);
			buf.append(bits.to_string());
		}
		break;
		case 'u':
		{
			buf.append(std::to_string(static_cast<ucell>(*arg)));
		}
		break;
	}
}

static cell AMX_NATIVE_CALL n_lua_pushfstring(AMX *amx, cell *params)
{
	if(!lua::check_params(amx, params, 2)) return 0;
	auto L = reinterpret_cast<lua_State*>(params[1]);

	cell *addr;
	amx_GetAddr(amx, params[2], &addr);
	int len;
	amx_StrLen(addr, &len);
	char *fmt = reinterpret_cast<char*>(alloca(len + 1));
	amx_GetString(fmt, addr, 0, len + 1);

	cell argc = params[0] / sizeof(cell) - 2;

	std::string buf;
	buf.reserve(len + 8 * argc);

	int argn = 0;

	char *c = fmt;
	while(len--)
	{
		if(*c == '%' && len > 0)
		{
			buf.append(fmt, c - fmt);

			const char *start = ++c;
			if(*c == '%')
			{
				buf.push_back('%');
				fmt = c + 1;
				len--;
			}else{
				while(len-- && !std::isalpha(*c)) c++;
				if(len < 0) break;
				if(argn >= argc)
				{
					//error
				}else{
					cell *argv;
					amx_GetAddr(amx, params[3 + argn++], &argv);
					add_format(buf, start, c, argv);
				}
				fmt = c + 1;
			}
		}
		c++;
	}
	buf.append(fmt, c - fmt);

	lua_pushlstring(L, &buf[0], buf.size());
	return 0;
}

template <AMX_NATIVE Native>
static cell AMX_NATIVE_CALL error_wrapper(AMX *amx, cell *params)
{
	try{
		return Native(amx, params);
	}catch(const lua::panic_error &error)
	{
		switch(error.code)
		{
			case LUA_ERRMEM:
				amx_RaiseError(amx, AMX_ERR_MEMORY);
				break;
			default:
				amx_RaiseError(amx, AMX_ERR_NATIVE);
				break;
		}
		return 0;
	}
}

#define AMX_DECLARE_NATIVE(Name) {#Name, error_wrapper<n_##Name>}
#define AMX_DECLARE_LUA_NATIVE(Name) {#Name, error_wrapper<lua::adapt<decltype(&Name), &Name>::native>}

static AMX_NATIVE_INFO native_list[] =
{
	AMX_DECLARE_NATIVE(lua_bind),
	AMX_DECLARE_NATIVE(lua_loopback),
	AMX_DECLARE_NATIVE(lua_stackdump),

	AMX_DECLARE_NATIVE(lua_newstate),
	AMX_DECLARE_NATIVE(lua_close),
	AMX_DECLARE_NATIVE(lua_load),
	AMX_DECLARE_NATIVE(lua_pcall),
	AMX_DECLARE_NATIVE(lua_call),
	AMX_DECLARE_NATIVE(lua_dostring),
	AMX_DECLARE_NATIVE(lua_tostring),
	AMX_DECLARE_NATIVE(lua_tonumber),
	AMX_DECLARE_NATIVE(lua_tointeger),
	AMX_DECLARE_NATIVE(lua_pop),
	AMX_DECLARE_NATIVE(lua_pushpfunction),
	AMX_DECLARE_NATIVE(lua_settable),
	AMX_DECLARE_NATIVE(lua_setfield),
	AMX_DECLARE_NATIVE(lua_setglobal),
	AMX_DECLARE_NATIVE(lua_gettable),
	AMX_DECLARE_NATIVE(lua_getfield),
	AMX_DECLARE_NATIVE(lua_getglobal),
	AMX_DECLARE_NATIVE(lua_len),
	AMX_DECLARE_NATIVE(lua_pushstring),
	AMX_DECLARE_NATIVE(lua_pushfstring),

	AMX_DECLARE_LUA_NATIVE(lua_absindex),
	AMX_DECLARE_LUA_NATIVE(lua_checkstack),
	AMX_DECLARE_LUA_NATIVE(lua_copy),
	AMX_DECLARE_LUA_NATIVE(lua_createtable),
	AMX_DECLARE_LUA_NATIVE(lua_gc),
	AMX_DECLARE_LUA_NATIVE(lua_getmetatable),
	AMX_DECLARE_LUA_NATIVE(lua_gettop),
	AMX_DECLARE_LUA_NATIVE(lua_getuservalue),
	AMX_DECLARE_LUA_NATIVE(lua_iscfunction),
	AMX_DECLARE_LUA_NATIVE(lua_isinteger),
	AMX_DECLARE_LUA_NATIVE(lua_isnumber),
	AMX_DECLARE_LUA_NATIVE(lua_isstring),
	AMX_DECLARE_LUA_NATIVE(lua_isuserdata),
	AMX_DECLARE_LUA_NATIVE(lua_newthread),
	AMX_DECLARE_LUA_NATIVE(lua_newuserdata),
	AMX_DECLARE_LUA_NATIVE(lua_next),
	AMX_DECLARE_LUA_NATIVE(lua_pushboolean),
	AMX_DECLARE_LUA_NATIVE(lua_pushinteger),
	AMX_DECLARE_LUA_NATIVE(lua_pushlightuserdata),
	AMX_DECLARE_LUA_NATIVE(lua_pushnil),
	AMX_DECLARE_LUA_NATIVE(lua_pushnumber),
	AMX_DECLARE_LUA_NATIVE(lua_pushthread),
	AMX_DECLARE_LUA_NATIVE(lua_pushvalue),
	AMX_DECLARE_LUA_NATIVE(lua_rawequal),
	AMX_DECLARE_LUA_NATIVE(lua_rawget),
	AMX_DECLARE_LUA_NATIVE(lua_rawgeti),
	AMX_DECLARE_LUA_NATIVE(lua_rawgetp),
	AMX_DECLARE_LUA_NATIVE(lua_rawlen),
	AMX_DECLARE_LUA_NATIVE(lua_rawset),
	AMX_DECLARE_LUA_NATIVE(lua_rawseti),
	AMX_DECLARE_LUA_NATIVE(lua_rawsetp),
	AMX_DECLARE_LUA_NATIVE(lua_resume),
	AMX_DECLARE_LUA_NATIVE(lua_rotate),
	AMX_DECLARE_LUA_NATIVE(lua_setmetatable),
	AMX_DECLARE_LUA_NATIVE(lua_settop),
	AMX_DECLARE_LUA_NATIVE(lua_setuservalue),
	AMX_DECLARE_LUA_NATIVE(lua_status),
	AMX_DECLARE_LUA_NATIVE(lua_toboolean),
	AMX_DECLARE_LUA_NATIVE(lua_topointer),
	AMX_DECLARE_LUA_NATIVE(lua_tothread),
	AMX_DECLARE_LUA_NATIVE(lua_touserdata),
	AMX_DECLARE_LUA_NATIVE(lua_type),
	AMX_DECLARE_LUA_NATIVE(lua_version),
	AMX_DECLARE_LUA_NATIVE(lua_xmove),
};

int RegisterNatives(AMX *amx)
{
	return amx_Register(amx, native_list, sizeof(native_list) / sizeof(*native_list));
}
