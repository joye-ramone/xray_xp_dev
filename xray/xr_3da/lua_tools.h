////////////////////////////////////////////////////////////////////////////
//	Module 		: lua_tools.h
//	Created 	: 29.07.2014
//  Modified 	: 17.02.2016
//	Author		: Alexander Petrov
//	Description : Lua functionality extension
////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
extern "C" {
#include <lua.h>
#include <luajit.h>
#include <lcoco.h>
};
#include <luabind\luabind.hpp>




class CLuaObjectParamAbstract
{
public:
	virtual	  bool	push_value(lua_State *L) = NULL;
};

extern ENGINE_API CLuaObjectParamAbstract* object_param_cache(CLuaObjectParamAbstract* obj);


template <class T>
class CLuaObjectParam:
	public CLuaObjectParamAbstract
{
protected:
	T*				m_pObject;
public:
	
	CLuaObjectParam(T* pObject) { m_pObject = pObject; }

	virtual	  bool	push_value(lua_State *L)    // non-inline uniform method
	{
		using namespace luabind::detail;
		if (m_pObject && get_class_rep<T>(L))
		{
			convert_to_lua<T*>(L, m_pObject);
			return true;
		}
		return false;
	};
};

template <class T>
ICF CLuaObjectParamAbstract *object_param(T* obj)
{
	return object_param_cache ( xr_new<CLuaObjectParam<T>>(obj) );
}


typedef  const xr_string CXR_STR;
typedef struct _LUA_RESULT  // alpet: 16 byte struct, consist result with type tag and error field
{
	int					type;
	int 				error;
	union
	{
		CXR_STR			*sval;
		lua_Number		 nval;
		lua_CFunction    fval;
		lua_State		*lval;  // thread
		BOOL			 bval;
		luabind::object *oval;  // object/table
		const void		*pval;  // lightuserdata
	};

	ICF LPCSTR c_str(LPCSTR def = NULL)
	{
		if (LUA_TSTRING == type && sval)
			return sval->c_str();
		return def;
	};

} LUA_RESULT, *PLUA_RESULT;


ENGINE_API LPCSTR		        GetLuaTraceback  (lua_State *L = NULL, int depth = 2);
// alpet: функция задействует глобальные переменные ограниченные числом. Рекурсивное использование недопустимо!
ENGINE_API PLUA_RESULT __cdecl  LuaExecute	(lua_State *L, LPCSTR func_name, LPCSTR args, ...); 
ENGINE_API void		   __cdecl	LuaFinalize (); // удаление глобальных переменных
ENGINE_API extern lua_State* g_aux_lua;
ENGINE_API extern lua_State* g_active_lua;
ENGINE_API extern lua_State* g_game_lua;
ENGINE_API extern lua_State* g_render_lua;
