#include "stdafx.h"
#include "lua_tools.h"


ENGINE_API LPCSTR get_lua_traceback(lua_State *L, int depth)
{
	static char  buffer[32768]; // global buffer
	int top = lua_gettop(L);
	// alpet: Lua traceback added
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	lua_getfield(L, -1, "traceback");
	lua_pushstring(L, "\t");
	lua_pushinteger(L, 1);

	const char *traceback = "cannot get Lua traceback ";
	strcpy_s(buffer, 32767, traceback);

	if (0 == lua_pcall(L, 2, 1, 0))
	{
		traceback = lua_tostring(L, -1);
		strcpy_s(buffer, 32767, traceback);
		lua_pop(L, 1);
	}

	lua_settop (L, top);
	return buffer;
}