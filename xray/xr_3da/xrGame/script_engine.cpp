////////////////////////////////////////////////////////////////////////////
//	Module 		: script_engine.cpp
//	Created 	: 01.04.2004
//  Modified 	: 17.02.2016
//	Author		: Dmitriy Iassenev
//	Description : XRay Script Engine
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_engine.h"
#include "ai_space.h"
#include "object_factory.h"
#include "script_process.h"
#include "../lua_tools.h"
#include "../../build_config_defines.h"
#include <Windows.h>
#include <Shlwapi.h>

#ifdef USE_DEBUGGER
#	include "script_debugger.h"
#endif

#ifndef XRSE_FACTORY_EXPORTS
#	ifdef DEBUG
#		include "ai_debug.h"
		extern Flags32 psAI_Flags;
#	endif
#endif

extern void export_classes(lua_State *L);

// extern void   set_lvm_name(lua_State* L, LPCSTR name, bool add_index = true);
// extern LPCSTR get_lvm_name(lua_State* L);

#ifdef LUAICP_COMPAT
void luaicp_error_handler(lua_State *L)
{
	lua_getglobal(L, "AtPanicHandler");
	if lua_isfunction(L, -1)
		lua_call(L, 0, 0);
	else
		lua_pop(L, 1);
}
#endif

void ResetLuaStates() // удал€ет поток VM AUX Lua
{
	if (g_game_lua)
	{
		lua_pushnil(g_game_lua);
		lua_setglobal(g_game_lua, "g_aux_lua"); // remove thread
	}	
	g_aux_lua    = NULL;
}

XRCORE_API LPCSTR  BuildStackTrace();

void PrintStackTrace(LPCSTR msg)
{
	lua_State *L = GameLua();
	LPCSTR tb;
	if (L)
	{
		tb = GetLuaTraceback(L, 2);
		Msg("# #CRASH report: game_lua_state = 0x%p, msg = %s %s", L, msg, tb);
	}
	L = AuxLua();
	if (L)
	{
		tb = GetLuaTraceback(L, 2);
		Msg("# #CRASH report: aux_lua_state = 0x%p, msg = %s %s", L, msg, tb);
	}	

	Msg("* PrintStackTrace executed from:\n %s", BuildStackTrace());
}


LPCSTR ExtractFileName(LPCSTR fname)
{
	LPCSTR result = fname;
	for (size_t c = 0; c < xr_strlen(fname); c++)
		if (fname[c] == '\\') result = &fname[c + 1];
	return result;
}


void find_scripts(CScriptEngine::SCRIPTS_MAP &map, LPCSTR path)
{
	if (!xr_strlen(path)) return;
	string_path buff;
	string_path fname;
	// recursive scan folders first
	auto *folders = FS.file_list_open(path, FS_ListFolders);
	if (folders)
	{
		auto &folder = *folders;
		for (size_t f = 0; f < folder.size(); f++)
		if (!strstr(folder[f], "."))
		{			
			strconcat(sizeof(fname), fname, path, folder[f]);
			find_scripts(map, fname);
		}
		FS.file_list_close(folders);
	}

	// scan files next
	auto *files = FS.file_list_open(path, FS_ListFiles);
	if (!files) return;
	auto &folder = *files;

	for (int i = 0; i < (int)folder.size(); i++)
	{		
		strconcat(sizeof(fname), fname, path, folder[i]);

		if ((strstr(fname, ".script") || strstr(fname, ".lua")) && FS.exist(fname))
		{
			LPCSTR fstart = ExtractFileName(fname);
			strcpy_s(buff, sizeof(buff), fstart);
			_strlwr_s(buff, sizeof(buff));
			LPCSTR nspace = strtok(buff, ".");
			map[nspace] = fname;
		}
			
	}

	FS.file_list_close(files);
}



CScriptEngine::CScriptEngine			()
{
	m_stack_level			= 0;
	m_reload_modules		= false;
	m_last_no_file_length	= 0;
	*m_last_no_file			= 0;

#ifdef USE_DEBUGGER
	m_scriptDebugger		= NULL;
	restartDebugger			();	
#endif
	SetCrashCB(PrintStackTrace);
}

CScriptEngine::~CScriptEngine			()
{
	collect_all_garbage();

	g_game_lua = NULL;

	while (!m_script_processes.empty())
		remove_script_process(m_script_processes.begin()->first);

#ifdef DEBUG
	flush_log				();
#endif // DEBUG

#ifdef USE_DEBUGGER
	xr_delete (m_scriptDebugger);
#endif
}

void CScriptEngine::unload				()
{
	lua_settop				(lua(),m_stack_level);
	m_last_no_file_length	= 0;
	*m_last_no_file			= 0;
}

int CScriptEngine::lua_panic			(lua_State *L)
{
	LPCSTR traceback = GetLuaTraceback(L, 1);	
	print_output	(L,"PANIC",LUA_ERRRUN);	
	Msg("! %s", traceback);
	return			(0);
}

void CScriptEngine::lua_error			(lua_State *L)
{
	print_output			(L,"",LUA_ERRRUN);

#if !XRAY_EXCEPTIONS
	LPCSTR traceback = GetLuaTraceback(L, 1);
	const char *error = lua_tostring(L, -1);
  #ifndef LUAICP_COMPAT   
	Debug.fatal(DEBUG_INFO, "LUA error: %s \n %s ", error ? error : "NULL", traceback);
  #endif
#else
	throw					lua_tostring(L,-1);
#endif
}

int  CScriptEngine::lua_pcall_failed	(lua_State *L)
{
	print_output			(L,"",LUA_ERRRUN);
#if !XRAY_EXCEPTIONS
  #ifndef LUAICP_COMPAT   
	Debug.fatal				(DEBUG_INFO,"LUA error: %s",lua_isstring(L,-1) ? lua_tostring(L,-1) : "");
  #endif
#endif
	if (lua_isstring(L,-1))
		lua_pop				(L,1);
	return					(LUA_ERRRUN);
}

void lua_cast_failed					(lua_State *L, LUABIND_TYPE_INFO info)
{
	CScriptEngine::print_output	(L,"",LUA_ERRRUN);

	// Debug.fatal				(DEBUG_INFO,"LUA error: cannot cast lua value to %s",info->name());
}

void CScriptEngine::setup_callbacks		()
{
#ifdef USE_DEBUGGER
	if( debugger() )
		debugger()->PrepareLuaBind	();
#endif

#ifdef USE_DEBUGGER
	if (!debugger() || !debugger()->Active() ) 
#endif
	{
#ifdef LUAICP_COMPAT
		lua_getglobal(lua(), "AtPanicHandler");
		if (lua_isfunction(lua(), -1))
			luabind::set_pcall_callback(lua_tocfunction(lua(), -1));
		lua_pop(lua(), 1);
#endif

#if !XRAY_EXCEPTIONS
		luabind::set_error_callback		(CScriptEngine::lua_error);
		
#endif
#ifndef MASTER_GOLD
		luabind::set_pcall_callback		(CScriptEngine::lua_pcall_failed);
#endif // MASTER_GOLD
	}

#if !XRAY_EXCEPTIONS
	luabind::set_cast_failed_callback	(lua_cast_failed);
#endif
	lua_atpanic							(lua(),CScriptEngine::lua_panic);
}

#ifdef DEBUG
#	include "script_thread.h"
void CScriptEngine::lua_hook_call		(lua_State *L, lua_Debug *dbg)
{
	if (ai().script_engine().current_thread())
		ai().script_engine().current_thread()->script_hook(L,dbg);
	else
		ai().script_engine().m_stack_is_ready	= true;
}
#endif

int auto_load				(lua_State *L)
{
	if ((lua_gettop(L) < 2) || !lua_istable(L,1) || !lua_isstring(L,2)) {
		lua_pushnil	(L);
		return		(1);
	}

	ai().script_engine().process_file_if_exists(L, lua_tostring(L,2),false);
	lua_rawget		(L,1);
	return			(1);
}


void CScriptEngine::setup_auto_load		()
{	
	lua_pushstring 						(lua(),"_G"); 
	lua_gettable 						(lua(),LUA_GLOBALSINDEX); 
	int value_index	= lua_gettop		(lua());  // alpet: во избежани€ оставлени€ в стеке лишней метатаблицы
	luaL_newmetatable					(lua(),"XRAY_AutoLoadMetaTable");
	lua_pushstring						(lua(),"__index");
	lua_pushcfunction					(lua(), auto_load);
	lua_settable						(lua(),-3);
	// luaL_getmetatable					(lua(),"XRAY_AutoLoadMetaTable");
	lua_setmetatable					(lua(), value_index);
}

void LuaTableCleanup(lua_State *L, xr_string space, xr_string context)
{
	xr_vector<xr_string> table_keys;
	int tidx = lua_gettop(L);
	lua_pushnil		(L);
	while (lua_next(L, tidx)) 
	{
		//  ||
		if (lua_istable(L, -1) && lua_isstring(L, -2))
			 	table_keys.push_back(lua_tostring(L, -2));
		lua_pop(L, 1);
	}

	if (table_keys.size())
		MsgCB("$#CONTEXT: %s will be removed %d from %s ", space.c_str(), table_keys.size(), context.c_str());

	for (u32 i = 0; i < table_keys.size(); i++)
	{
		LPCSTR s = table_keys[i].c_str();		

		if (xr_strlen(s) > 0)
		{
			// MsgCB("*  %s %s", space.c_str(), s);
			lua_pushstring(L, s);
			lua_rawget(L, tidx);
			if (lua_istable(L, -1) && space.size() < 3 && !lua_rawequal(L, -1, tidx))
				LuaTableCleanup(L, space + "  ", context + "." + s);
			lua_pop(L, 1);
			lua_pushnil(L);
			lua_setfield (L, tidx, s);
		}
	}
	lua_settop(L, tidx);
}

void CScriptEngine::init				()
{
	Log("*  CScriptEngine::init - (re)creating LUA virtual machine.");
	LuaFinalize   ();
	ResetLuaStates();			
	g_active_lua = NULL;	

	if (lua()) 
	{
		lua_State *L = lua();	
		set_lvm_name(L, "~g_game_lua");
		/*		
		lua_pushstring	(L, "_G");
		lua_gettable	(L, LUA_GLOBALSINDEX);
		LuaTableCleanup(L, " ", "_G");		
		lua_pushnil(L);
		lua_setglobal(L, "_G");				
		*/
	}	

	collect_all_garbage();		

	CScriptStorage::reinit				();
	g_game_lua = lua();

	luabind::open						(lua());	
	export_classes						(lua());

	if (g_render_lua)
	{
		luabind::open						(g_render_lua);	
		export_classes						(g_render_lua);
		process_file_if_exists				(g_render_lua, "render", false);
	}
	setup_callbacks						();
	setup_auto_load						();

#ifdef DEBUG
	m_stack_is_ready					= true;
#endif

#ifdef DEBUG
#	ifdef USE_DEBUGGER
		if( !debugger() || !debugger()->Active()  )
#	endif
			lua_sethook					(lua(),lua_hook_call,	LUA_MASKLINE|LUA_MASKCALL|LUA_MASKRET,	0);
#endif

	bool								save = m_reload_modules;
	m_reload_modules					= true;
	process_file_if_exists				(lua(), "_G", false);

	m_reload_modules					= save;

	register_script_classes				();
	object_factory().register_script	();

#ifdef XRGAME_EXPORTS
	load_common_scripts					();
#endif
	m_stack_level						= lua_gettop(lua());
	g_game_lua = lua();	
	AuxLua();	
}

void CScriptEngine::remove_script_process	(const EScriptProcessors &process_id)
{
	CScriptProcessStorage::iterator	I = m_script_processes.find(process_id);
	if (I != m_script_processes.end()) {
		xr_delete						((*I).second);
		m_script_processes.erase		(I);
	}
}

void CScriptEngine::load_common_scripts()
{
#ifdef DBG_DISABLE_SCRIPTS
	return;
#endif
	string_path		S;
	FS.update_path	(S,"$game_config$","script.ltx");
	CInifile		*l_tpIniFile = xr_new<CInifile>(S);
	R_ASSERT		(l_tpIniFile);
	if (!l_tpIniFile->section_exist("common")) {
		xr_delete			(l_tpIniFile);
		return;
	}

	if (l_tpIniFile->line_exist("common","script")) {
		LPCSTR			caScriptString = l_tpIniFile->r_string("common","script");
		u32				n = _GetItemCount(caScriptString);
		string256		I;
		for (u32 i=0; i<n; ++i) {
			process_file(lua(), _GetItem(caScriptString,i,I));
			if (object("_G",strcat(I,"_initialize"),LUA_TFUNCTION)) {
//				lua_dostring			(lua(),strcat(I,"()"));
				luabind::functor<void>	f;
				R_ASSERT				(functor(I,f));
				f						();
			}
		}
	}

	xr_delete			(l_tpIniFile);
}

bool CScriptEngine::lookup_script(string_path &fname, LPCSTR base)
{
	string_path lc_base;
	Memory.mem_fill(fname, 0, sizeof(fname));

	if (0 == m_script_files.size())
	{
		FS.update_path(lc_base, "$game_scripts$", "");
		find_scripts(m_script_files, lc_base);
	}

	strcpy_s(lc_base, sizeof(lc_base), base);
	_strlwr_s(lc_base, sizeof(lc_base));
	


	auto it = m_script_files.find(lc_base);
	if (it != m_script_files.end())
	{
		strcpy_s(fname, sizeof(fname), *it->second);
		return true;
	}

	return false;
}


void CScriptEngine::process_file_if_exists	(lua_State *L, LPCSTR file_name, bool warn_if_not_exist)
{
	u32						string_length = xr_strlen(file_name);
	if (!warn_if_not_exist && no_file_exists(file_name,string_length))
		return;

	string_path				S;
	if (m_reload_modules || (*file_name && !namespace_loaded(L, file_name))) {
		lookup_script  (S, file_name);
		if (!warn_if_not_exist && !FS.exist(S)) {
#ifdef DEBUG
#	ifndef XRSE_FACTORY_EXPORTS
			if (psAI_Flags.test(aiNilObjectAccess))
#	endif
			{
				print_stack			();
				Msg					("* trying to access variable %s, which doesn't exist, or to load script %s, which doesn't exist too",file_name,S1);
				m_stack_is_ready	= true;
			}
#endif
			add_no_file		(file_name,string_length);
			return;
		}
		if (0 == xr_strlen(S)) return;

#ifdef MASTER_GOLD
		MsgV					("5LOAD_SCRIPT", "* loading script %s", ExtractFileName(S));
#endif // MASTER_GOLD
		m_reload_modules	= false;
		load_file_into_namespace (L, S, *file_name ? file_name : "_G");
	}
}

void CScriptEngine::process_file	(lua_State *L, LPCSTR file_name)
{
	process_file_if_exists	(L, file_name,true);
}

void CScriptEngine::process_file	(lua_State *L, LPCSTR file_name, bool reload_modules)
{
	m_reload_modules		= reload_modules;
	process_file_if_exists	(L, file_name, true);
	m_reload_modules		= false;
}

void CScriptEngine::register_script_classes		()
{
#ifdef DBG_DISABLE_SCRIPTS
	return;
#endif
	string_path					S;
	FS.update_path				(S,"$game_config$","script.ltx");
	CInifile					*l_tpIniFile = xr_new<CInifile>(S);
	R_ASSERT					(l_tpIniFile);

	if (!l_tpIniFile->section_exist("common")) {
		xr_delete				(l_tpIniFile);
		return;
	}

	m_class_registrators		= READ_IF_EXISTS(l_tpIniFile,r_string,"common","class_registrators","");
	xr_delete					(l_tpIniFile);

	u32							n = _GetItemCount(*m_class_registrators);
	string256					I;
	for (u32 i=0; i<n; ++i) {
		_GetItem				(*m_class_registrators,i,I);
		luabind::functor<void>	result;
		if (!functor(I,result)) {
			script_log			(eLuaMessageTypeError,"Cannot load class registrator %s!",I);
			continue;
		}
		result					(const_cast<CObjectFactory*>(&object_factory()));
	}
}

bool CScriptEngine::function_object(LPCSTR function_to_call, luabind::object &object, int type)
{
	if (!xr_strlen(function_to_call))
		return				(false);

	
#ifdef LUAICP_COMPAT
	int save_top = lua_gettop(lua());
	string256 code;
	// sprintf_s(code, 256, "return %s", function_to_call);
	strconcat(sizeof(code), code, "return ", function_to_call);
	luaL_loadstring(lua(), code);
	

	if (0 == lua_pcall(lua(), 0, 1, 0) && !lua_isnil(lua(), -1))
	{	
		object = luabind::object(lua());
		object.set();		
		lua_settop(lua(), save_top);
		return (object.type() == type);
	}
	else
	{
		lua_settop(lua(), save_top);
		return (false);
	}

#else
	string256				name_space, function;	
	parse_script_namespace	(function_to_call,name_space,function);
	if (xr_strcmp(name_space,"_G"))
		process_file		(name_space);

	if (!this->object(name_space,function,type))
		return				(false);

	luabind::object			lua_namespace	= this->name_space(name_space);
	object					= lua_namespace[function];
	return					(true);
#endif

}

#ifdef USE_DEBUGGER
void CScriptEngine::stopDebugger				()
{
	if (debugger()){
		xr_delete	(m_scriptDebugger);
		Msg			("Script debugger succesfully stoped.");
	}
	else
		Msg			("Script debugger not present.");
}

void CScriptEngine::restartDebugger				()
{
	if(debugger())
		stopDebugger();

	m_scriptDebugger = xr_new<CScriptDebugger>();
	debugger()->PrepareLuaBind();
	Msg				("Script debugger succesfully restarted.");
}
#endif

bool CScriptEngine::no_file_exists	(LPCSTR file_name, u32 string_length)
{
	if (m_last_no_file_length != string_length)
		return				(false);

	return					(!memcmp(m_last_no_file,file_name,string_length*sizeof(char)));
}

void CScriptEngine::add_no_file		(LPCSTR file_name, u32 string_length)
{
	m_last_no_file_length	= string_length;
	CopyMemory				(m_last_no_file,file_name,(string_length+1)*sizeof(char));
}

void CScriptEngine::collect_all_garbage	()
{		
	if (NULL == lua())		return;
	__try
	{
		lua_gc(lua(), LUA_GCCOLLECT, 0);
		lua_gc(lua(), LUA_GCCOLLECT, 0);
	}
	__except (SIMPLE_FILTER)
	{
		Msg("! #EXCEPTION: in CScriptEngine::collect_all_garbage	() for lua() = %s ", get_lvm_name(lua()));
	}

}

ENGINE_API BOOL g_appLoaded;

LPCSTR CScriptEngine::try_call(LPCSTR func_name, LPCSTR param)
{   
	if (NULL == this || NULL == lua()) 
		return "#ERROR: Script engine not ready";
	// максимально быстрый вызов функции
	MsgCB("$#CONTEXT: CScriptEngine::try_call('%s', '%s') with L = '%s' ", func_name, (param ? param : "(null"), get_lvm_name(lua()));

	PLUA_RESULT result;
	if (param)
		result = LuaExecute(lua(), func_name, "s", param);
	else
		result = LuaExecute(lua(), func_name, "");

	if (result->type == LUA_TSTRING)
		return result->c_str();
	else
		if (0 == result->error)
			return "OK";
	return "undefined";
}




DLL_API void log_script_error(LPCSTR format, ...)
{
	string1024 line_buf;
	va_list mark;	
	va_start(mark, format);
	int sz = _vsnprintf(line_buf, sizeof(line_buf)-1, format, mark); 	
	line_buf[sizeof(line_buf) - 1] = 0;
	va_end(mark);

	ai().script_engine().script_log(ScriptStorage::ELuaMessageType::eLuaMessageTypeError, line_buf);
}
 


DLL_API lua_State* ActiveLua()
{
	if (g_active_lua)
		return g_active_lua;
	else
		return GameLua();
}


DLL_API lua_State* GameLua()
{
	return g_game_lua;
}

DLL_API lua_State* AuxLua()
{
	if (!g_aux_lua)
	{
		lua_State *L = GameLua();
		if (!L) return NULL;
		g_aux_lua = lua_newthread(L);  // глобальные переменные общие, многопоточное использование Ќ≈Ѕ≈«ќѕј—Ќќ!
		// lua_pushlightuserdata(L, g_aux_lua);
		lua_setglobal(L, "g_aux_lua");
		g_aux_lua = L;					// debug test
		set_lvm_name(g_aux_lua, "g_aux_lua");
	}
	return g_aux_lua;
}

DLL_API void SetActiveLua(lua_State* L)
{
	g_active_lua = L;
}

DLL_API LPCSTR TryCallLuafunc(LPCSTR func_name, LPCSTR param)
{
	return ai().script_engine().try_call(func_name, param);
}

