#include "stdafx.h"
#include "lua_tools.h"

ENGINE_API lua_State* g_active_lua		= NULL;
ENGINE_API lua_State* g_aux_lua			= NULL;
ENGINE_API lua_State* g_game_lua		= NULL;
ENGINE_API lua_State* g_render_lua		= NULL;

#pragma optimize("gyts", off)

/*
typedef struct _OBJECT_REF  {
	PVOID		pThis;
	shared_str	class_name;
} LUA_OBJECT_REF, *PLUA_OBJECT_REF;

LUA_OBJECT_REF g_obj_params[64];
volatile ULONG g_last_param = 0;

*/

#define MAX_RESULTS 16


LUA_RESULT	     g_results[MAX_RESULTS];
xr_string		 s_results[MAX_RESULTS];
luabind::object *o_results[MAX_RESULTS];

CLuaObjectParamAbstract* op_cache[MAX_RESULTS];

ENGINE_API CLuaObjectParamAbstract* object_param_cache(CLuaObjectParamAbstract* obj)
{
	static volatile ULONG n_param = 0;
	ULONG n = InterlockedIncrement(&n_param) & (MAX_RESULTS - 1);
	xr_delete(op_cache[n]);
	op_cache[n] = obj;
	return obj;
}


// alpet: опасно чрезмерное многопоточное использование из-за ограниченного списка глобальных переменных (особенно с глобальными g_*_lua)
ENGINE_API PLUA_RESULT __cdecl LuaExecute(lua_State *L, LPCSTR func_name, LPCSTR args, ...)
{
	static volatile ULONG n_result = 0;
	
	ULONG n = InterlockedIncrement(&n_result) & (MAX_RESULTS - 1);
	LUA_RESULT &result =  g_results[n];
	result.type		   = LUA_TSTRING;
	result.error	   = 0;
	result.sval		   = &s_results[n];
	xr_string &s_result = s_results[n];

	if (!args)
		args = "";

	int save_top = lua_gettop(L);
	lua_getglobal(L, "AtPanicHandler");
	int erridx = 0;
	if (lua_isfunction(L, -1))
		erridx = lua_gettop(L);
	else
		lua_pop(L, 1);
	string256 func;
	strcpy_s(func, 255, func_name);
	LPSTR ctx = NULL;
	LPCSTR name_space = strtok_s(func, ".", &ctx);
	func_name  = strtok_s(NULL, ".", &ctx);
	if (func_name)
	{
		lua_getglobal(L, name_space);
		if (lua_istable(L, -1))
			lua_getfield(L, -1, func_name);
		else
		{
			lua_settop(L, save_top);
			s_result = "#ERROR: not found lua name_space ";
			s_result += name_space;
			result.error = -1;
			return &result;
		}
	}
	else
	{		
		func_name = name_space;
		name_space = "_G";
		lua_getglobal(L, func_name); // 
	}
	if (!lua_isfunction(L, -1))
	{
		s_result = "#ERROR: not found function ";
		s_result = (s_result + name_space) + "." + func_name;		
		result.error = -1;
		return &result;
	}

	
	va_list list;
	va_start(list, args);
	int argc = xr_strlen(args);

	__try
	{
		double  nv = 0;
		int     iv = 0;
		LPCSTR  sv = NULL;


		for (int i = 0; i < argc; i ++)
			switch (args[i])
			{
			case 'b': lua_pushboolean(L, va_arg(list, bool)); break;
			case 'd':;
			case 'u': {
						iv = va_arg(list, int);
						lua_pushinteger(L, iv);
					}	break;			
			case 'f':;
			case 'n': {
						nv = va_arg(list, double);
						lua_pushnumber(L, (lua_Number)nv);
					} break;
			case 's': {
						sv = va_arg(list, LPCSTR);
						lua_pushstring(L, sv); break;
					}
			case 'o': {
						void *pv = va_arg(list, PVOID);
						CLuaObjectParamAbstract *op = static_cast<CLuaObjectParamAbstract *>(pv);
						op->push_value(L);
					} break;
			case 'p': {	
						void* pv = va_arg(list, PVOID);
						lua_pushlightuserdata(L, pv);
					} break;
				  
			default:
			{
				va_arg(list, int);
				lua_pushnil(L);
			}
			};
		va_end(list);
		if (iv || nv > 0 || sv)
			__asm nop;

	 	int err = lua_pcall(L, argc, 1, erridx);
		if (0 == err)
		{
			int t = lua_type(L, -1);
			if (LUA_TNIL == t)
				s_result = "OK";		
			else
			{
				result.type = t;
				switch (t)
				{
				case LUA_TBOOLEAN:
					result.bval = lua_toboolean(L, -1);
					break;
				case LUA_TLIGHTUSERDATA:				
					result.pval = lua_topointer(L, -1);
					break;		

				case LUA_TNUMBER:
					result.nval = lua_tonumber(L, -1);
					break;
				case LUA_TSTRING:
				{
					size_t len = 0;
					LPCSTR str = lua_tolstring(L, -1, &len);
					s_result.assign (str, len);
				}; break;
				case LUA_TFUNCTION:				
					result.fval = lua_tocfunction(L, -1);
					break;
				case LUA_TTABLE:
				case LUA_TUSERDATA:
				{
					if (o_results[n])
						xr_delete(o_results[n]);
					o_results[n] = xr_new<luabind::object>(L);
					o_results[n]->at(-1);
					result.oval = o_results[n];
				}; break;			
				case LUA_TTHREAD:
					result.lval = lua_tothread(L, -1);
					break;
				}; // switch

			}
		}
		else
		{
			s_result.assign(xr_sprintf("lua_pcall returned error %d", err));			
		}

	}
	__except (SIMPLE_FILTER)
	{
		s_result = "#EXCEPTION";
		Msg("!%s in LuaExecute, func_name = '%s', args = '%s' ", func_name, args);
	}
	lua_settop(L, save_top);
	return &result;

}

void __cdecl LuaFinalize()
{
	for (int i = 0; i < MAX_RESULTS; i++)
	{
		xr_delete(o_results[i]);
		xr_delete(op_cache[i]);
		s_results[i].empty();
	}
}


ENGINE_API LPCSTR GetLuaTraceback(lua_State *L, int depth)
{
	if (L)  g_active_lua = L;
	if (!L) L = g_active_lua;
	if (!L) return "L = NULL";

	static char  buffer[32768]; // global buffer
	Memory.mem_fill(buffer, 0, sizeof(buffer));
	int top = lua_gettop(L);
	// alpet: Lua traceback added
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	lua_getfield(L, -1, "traceback");
	lua_pushstring(L, "\t");
	lua_pushinteger(L, 1);

	const char *traceback = "cannot get Lua traceback ";
	strcpy_s(buffer, 32767, traceback);
	__try
	{
		if (0 == lua_pcall(L, 2, 1, 0))
		{
			traceback = lua_tostring(L, -1);
			strcpy_s(buffer, 32767, traceback);
			lua_pop(L, 1);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Msg("!#EXCEPTION(get_lua_traceback): buffer = %s ", buffer);
	}
	lua_settop (L, top);
	return buffer;
}