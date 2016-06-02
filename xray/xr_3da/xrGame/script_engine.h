////////////////////////////////////////////////////////////////////////////
//	Module 		: script_engine.h
//	Created 	: 01.04.2004
//  Modified 	: 17.02.2016
//	Author		: Dmitriy Iassenev
//	Description : XRay Script Engine
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "script_storage.h"
#include "script_export_space.h"
#include "script_space_forward.h"
#include "associative_vector.h"

extern "C" {
	#include <lua.h>
	#include <luajit.h>
	#include <lcoco.h>
};
//#define DBG_DISABLE_SCRIPTS

//namespace ScriptEngine {
//	enum EScriptProcessors;
//};
#include "script_engine_space.h"

class CScriptProcess;
class CScriptThread;
struct lua_State;
struct lua_Debug;

#ifdef USE_DEBUGGER
	class CScriptDebugger;
#endif

class CScriptEngine : public CScriptStorage {
public:
	typedef CScriptStorage											inherited;
	typedef ScriptEngine::EScriptProcessors							EScriptProcessors;
	typedef associative_vector<EScriptProcessors,CScriptProcess*>	CScriptProcessStorage;
	typedef xr_map<shared_str, shared_str>							SCRIPTS_MAP;

private:
	bool						m_reload_modules;

protected:
	CScriptProcessStorage		m_script_processes;
	int							m_stack_level;
	shared_str					m_class_registrators;
	SCRIPTS_MAP					m_script_files;

protected:
#ifdef USE_DEBUGGER
	CScriptDebugger				*m_scriptDebugger;
#endif

private:
	string128					m_last_no_file;
	u32							m_last_no_file_length;

			bool				no_file_exists				(LPCSTR file_name, u32 string_length);
			void				add_no_file					(LPCSTR file_name, u32 string_length);

public:
								CScriptEngine				();
	virtual						~CScriptEngine				();
			void				init						();
	virtual	void				unload						();
	ICF		lua_State*			lua							()			{ return game_lua(); }
	static	int					lua_panic					(lua_State *L);
	static	void				lua_error					(lua_State *L);
	static	int					lua_pcall_failed			(lua_State *L);
	static	void				lua_hook_call				(lua_State *L, lua_Debug *dbg);
			void				setup_callbacks				();
			void				load_common_scripts			();
			bool				load_file					(lua_State *L, LPCSTR	caScriptName, LPCSTR namespace_name);
			bool				lookup_script				(string_path &fname, LPCSTR base);
	IC		CScriptProcess		*script_process				(const EScriptProcessors &process_id) const;
	IC		void				add_script_process			(const EScriptProcessors &process_id, CScriptProcess *script_process);
			void				remove_script_process		(const EScriptProcessors &process_id);
			void				setup_auto_load				();
			void				process_file_if_exists		(lua_State *L, LPCSTR file_name, bool warn_if_not_exist);
			void				process_file				(lua_State *L, LPCSTR file_name);
			void				process_file				(lua_State *L, LPCSTR file_name, bool reload_modules);
			bool				function_object				(LPCSTR function_to_call, luabind::object &object, int type = LUA_TFUNCTION);
			void				register_script_classes		();
	IC		void				parse_script_namespace		(LPCSTR function_to_call, LPSTR name_space, LPSTR functor);

	template <typename _result_type>
	IC		bool				functor						(LPCSTR function_to_call, luabind::functor<_result_type> &lua_function);
	

#ifdef USE_DEBUGGER
			void				stopDebugger				();
			void				restartDebugger				();
			CScriptDebugger		*debugger					();
#endif
			void				collect_all_garbage			();
			LPCSTR				try_call					(LPCSTR func_name, LPCSTR param);

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CScriptEngine)
#undef script_type_list
#define script_type_list save_type_list(CScriptEngine)

#include "script_engine_inline.h"

extern DLL_API void log_script_error(LPCSTR format, ...);
extern DLL_API lua_State* ActiveLua ();
extern DLL_API lua_State* AuxLua();
extern DLL_API lua_State* GameLua ();
extern DLL_API void   SetActiveLua (lua_State* L);
extern DLL_API LPCSTR TryCallLuafunc (LPCSTR func_name, LPCSTR param);
