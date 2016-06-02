////////////////////////////////////////////////////////////////////////////
//	Module 		: script_storage.cpp
//	Created 	: 01.04.2004
//  Modified 	: 17.02.2016
//	Author		: Dmitriy Iassenev
//	Description : XRay Script Storage
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_storage.h"
#include "script_thread.h"
#include <stdarg.h>
#include "../common/doug_lea_memory_allocator.h"
#include "../lua_tools.h"
#include "../../build_config_defines.h"

LPCSTR	file_header_old = "\
local function script_name() \
return \"%s\" \
end \
local this = {} \
%s this %s \
setmetatable(this, {__index = _G}) \
setfenv(1, this)  \
this.script_modified = '%s' \
this.script_namespace = script_name() \
		";

LPCSTR	file_header_new = "\
local function script_name() \
return \"%s\" \
end \
local this = {} \
this._G = _G \
%s this %s \
setfenv(1, this) \
this.script_modified = '%s' \
this.script_namespace = script_name() \
		";

DLL_API			LPCSTR			   file_header		= NULL;
DLL_API const	CLocatorAPI::file *script_file		= NULL;

int g_error_count  = 0;
int g_load_session = 0;

xr_map<lua_State*, shared_str> lvm_names;


#ifndef ENGINE_BUILD
#	include "script_engine.h"
#	include "ai_space.h"
#else
#	define NO_XRGAME_SCRIPT_ENGINE
#endif

#ifndef XRGAME_EXPORTS
#	define NO_XRGAME_SCRIPT_ENGINE
#endif

#ifndef NO_XRGAME_SCRIPT_ENGINE
#	include "ai_debug.h"
#endif

#ifdef USE_DEBUGGER
#	include "script_debugger.h"
#endif

#ifndef PURE_ALLOC
// #	ifndef USE_MEMORY_MONITOR
// #		define USE_DL_ALLOCATOR
#define  USE_HEAP_ALLOCATOR
// #	endif // USE_MEMORY_MONITOR
#endif // PURE_ALLOC

#pragma optimize("gyts", off)

extern void dump_strerr();

#ifdef USE_HEAP_ALLOCATOR

#define HEAP_SERIALIZE  0
#define HEAP_FLAGS  HEAP_GENERATE_EXCEPTIONS|HEAP_SERIALIZE
// |HEAP_NO_SERIALIZE

xr_map <PVOID, HANDLE> g_lua_heaps;
bool g_heap_dbg = false;


u32 game_lua_memory_usage()
{
	u32 result = 0;
	for (auto it = g_lua_heaps.begin(); it != g_lua_heaps.end(); it++)
	{
		result += mem_usage_impl(it->second, NULL, NULL);
	}
	return result;
}


static void *lua_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {	
	HANDLE heap = (HANDLE)ud;			 

#define STATIC_OVERHEAD 8 // дл€ изучени€ веро€тности overrun
	R_ASSERT(heap);
	size_t cb = nsize + STATIC_OVERHEAD;
	dump_strerr(); // лучше чаще, чтобы не тер€ть сообщени€

	if (0 == nsize)
	{
		if (g_heap_dbg) R_ASSERT(HeapValidate(heap, HEAP_SERIALIZE, ptr));
	 	HeapFree(heap, HEAP_FLAGS, ptr);
		return NULL;
	}
	if (0 == osize && nsize)
	{		
		PBYTE r = (PBYTE) HeapAlloc(heap, HEAP_FLAGS|HEAP_ZERO_MEMORY, cb);		
		R_ASSERT2(r, make_string("HeapAlloc failed to allocate block with size %d, LastError = 0x%x", cb, GetLastError()));
		PDWORD tail = PDWORD(r + nsize);
		*tail = 0; tail++;
		*tail = 0xff000000;
		return r;
	}
	if (osize && nsize)
	{
		if (g_heap_dbg) R_ASSERT(HeapValidate(heap, HEAP_SERIALIZE, ptr));
		PBYTE r = (PBYTE) HeapReAlloc(heap, HEAP_FLAGS|HEAP_ZERO_MEMORY, ptr, cb);
		R_ASSERT2(r, make_string("HeapReAlloc failed to change block size from %d to %d, LastError = 0x%x", osize + STATIC_OVERHEAD, cb, GetLastError()));
		PDWORD tail = PDWORD(r + nsize);
		*tail = 0; tail++;
		*tail = 0xff000000;
		return r;
	}

	return NULL;
}


#endif


#ifdef USE_XR_ALLOCATOR
static void *lua_alloc	(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    xr_free	(ptr);
    return	NULL;
  }
  else
#ifdef DEBUG_MEMORY_NAME
    return Memory.mem_realloc		(ptr, nsize, "LUA");
#else // DEBUG_MEMORY_MANAGER
    return Memory.mem_realloc		(ptr, nsize);
#endif // DEBUG_MEMORY_MANAGER
}
#endif 

extern "C"
{
	void dlfatal(char *file, int line, char *function)
	{
		Debug.fatal(file, line, function, "Doug lea fatal error!");
	}
};


#ifdef USE_DL_ALLOCATOR

u32 game_lua_memory_usage	()
{
	return					((u32)dlmallinfo().uordblks);
}


static void *lua_alloc	(void *ud, void *ptr, size_t osize, size_t nsize) {
  // (void)ud;
  // (void)osize;
  static xrCriticalSection cs;

  __try
  {
	  BOOL sync = FALSE;
	  for (int i = 0; i < 1000; i++)
		{
			sync = cs.TryEnter();
			if (sync) break;
			Sleep(1);
		}
	  R_ASSERT3(sync, "cannot lock critical section in lua_alloc: ", cs.Dump());

	  __try
	  {
		  if (nsize == 0)	{ dlfree(ptr);	 return	NULL; }
		  else				return dlrealloc(ptr, nsize);
	  }
	  __finally
	  {
		  if (sync) cs.Leave();
	  }
  } 
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
	  Msg("! #EXCEPTION: in lua_alloc, ud = 0x%p, ptr = 0x%p, osize = %d, nsize = %d ", ud, ptr, osize, nsize);
	  Msg("*  allocated %d ", game_lua_memory_usage());
  }
  return NULL;
}

#endif // USE_DL_ALLOCATOR

DLL_API void set_lvm_name(lua_State* L, LPCSTR name, bool add_index)
{
	string256 temp;
	if (add_index)
		sprintf_s(temp, 256, "%s_%d", name, g_load_session);
	else
		strcpy_s(temp, 256, name);
	lvm_names[L] = temp;
}

DLL_API LPCSTR get_lvm_name(lua_State* L)
{
	static string256 temp;
	sprintf_s(temp, 256, "unknown_0x%p", L);
	auto it = lvm_names.find(L);
	if (it != lvm_names.end())
		strcpy_s(temp, 256, *it->second);
	return temp;
}



int load_module(lua_State *L) // prefetch alternative
{
	LPCSTR path = lua_tostring(L, 1);
	LPCSTR nspace = lua_tostring(L, 2);	

	string_path initial;
	strcpy_s(initial, sizeof(initial), path);
	strtok(initial, "\\");
	path = strtok(NULL, "\\");	
	xr_string name = path;
	name += "\\";
	name += nspace;

	string_path fname;
	if (FS.exist(fname, initial, (name + ".lua").c_str()))
	{
		ai().script_engine().load_file_into_namespace(L, fname, nspace);
		lua_pushboolean(L, TRUE);
	} else
	if (FS.exist(fname, initial, (name + ".script").c_str()))
	{
		ai().script_engine().load_file_into_namespace(L, fname, nspace);
		lua_pushboolean(L, TRUE);
	}
	else lua_pushboolean(L, FALSE);


	return 1;

}


lua_State *lvm_create(LPCSTR name)
{
	lua_State *m_virtual_machine;
	void *ud = NULL;
	
#ifdef USE_HEAP_ALLOCATOR	
	HANDLE new_lua_heap = HeapCreate(HEAP_FLAGS, 512 * 1024, 0);
	ud = (PVOID)new_lua_heap;	
	MsgCB("# #PERF: created lua heap 0x%x for lua_State %s", new_lua_heap, name);
#endif
	m_virtual_machine		= lua_newstate(lua_alloc, ud);
#ifdef USE_HEAP_ALLOCATOR
	g_lua_heaps[m_virtual_machine] = new_lua_heap;
#endif
	set_lvm_name(m_virtual_machine, name);

	
	if (!m_virtual_machine) {
		Msg					("! ERROR : Cannot initialize script virtual machine!");
		return NULL;
	}	
	luaL_openlibs(m_virtual_machine);		
	


#ifdef LUAICP_COMPAT
 #include "luaicp_attach.inc" // added by alpet 05.07.14
#endif
	return m_virtual_machine;
}

void lvm_destroy(lua_State* &m_virtual_machine, LPCSTR context)
{
	lua_State *L = m_virtual_machine;
	m_virtual_machine = NULL;

	if (L)
	__try {
		MsgCB("$#CONTEXT: lvm_destroy lua_State = %s (0x%p), context = '%s' ", get_lvm_name(L), (PVOID)L, context);

		lua_close(L);  // сброс дл€ лучшей очистки глобальных переменных		

#ifdef USE_HEAP_ALLOCATOR		
		auto it = g_lua_heaps.find((PVOID)L);
		if (it != g_lua_heaps.end())
		{			
			u32 cb, b_used, b_free;
			HeapValidate(it->second, HEAP_SERIALIZE, NULL);
			cb = mem_usage_impl(it->second, &b_used, &b_free);
			MsgCB("# #PERF: destroying lua heap 0x%x, size = %d, blocks used = %d, block free = %d ", it->second, cb, b_used, b_free);
			
			HeapDestroy(it->second);
			g_heap_dbg = false;
			g_lua_heaps.erase(it);
		}
#endif

		
	}
	__except (SIMPLE_FILTER) {
		Log("!#EXCEPTION: lua_close failed ");
		MsgCB("$#DUMP_CONTEXT");
	}
}



CScriptStorage::CScriptStorage		()
{
	m_current_thread		= 0;

#ifdef DEBUG
	m_stack_is_ready		= false;
#endif // DEBUG
	
	m_virtual_machine		= NULL;
	g_game_lua = g_aux_lua = g_render_lua = NULL;
}

CScriptStorage::~CScriptStorage		()
{
	lvm_destroy(m_virtual_machine, "CScriptStorage destructor #1");
	lvm_destroy(g_render_lua,	   "CScriptStorage destructor #2");	
}

#ifdef LUA_STACK_LEAK_FIND
lua_State *CScriptStorage::lua					()
{

	static int max_top = 0;
	if (m_virtual_machine)
	{
		int top = lua_gettop(m_virtual_machine);
		if (top > max_top)
		{
			Msg("##LEAK2: lua stack top raised to %d", top);
			max_top = top;			
		}
		if (max_top - top > 200) max_top = top; // recycle
	}

	return				(m_virtual_machine);
}
#endif


void CScriptStorage::reinit	()
{	
	g_active_lua  = NULL;
	g_game_lua    = NULL;
	g_aux_lua     = NULL;
	g_error_count = 0;
	g_load_session ++;
	

	lvm_destroy(m_virtual_machine, "CScriptStorage::reinit()");		
			
	m_virtual_machine		= lvm_create("g_game_lua");
	g_game_lua = m_virtual_machine;
	// lvm_destroy(g_render_lua, "CScriptStorage::reinit()");		

	if (NULL == g_render_lua)
	{
		g_render_lua = lvm_create("g_render_lua"); // независимые глобальные переменные, и не загруженные скрипты!		
	}

	if (strstr(Core.Params,"-_g"))
		file_header			= file_header_new;
	else
		file_header			= file_header_old;
}

int CScriptStorage::vscript_log		(ScriptStorage::ELuaMessageType tLuaMessageType, LPCSTR caFormat, va_list marker)
{
#ifndef NO_XRGAME_SCRIPT_ENGINE
#	ifdef DEBUG
	if (!psAI_Flags.test(aiLua) && (tLuaMessageType != ScriptStorage::eLuaMessageTypeError))
		return(0);
#	endif
#endif

#ifndef DEBUG
	return		(0);
#else // DEBUG

	LPCSTR		S = "", SS = "";
	LPSTR		S1;
	string4096	S2;
	switch (tLuaMessageType) {
		case ScriptStorage::eLuaMessageTypeInfo : {
			S	= "* [LUA] ";
			SS	= "[INFO]        ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeError : {
			S	= "! [LUA] ";
			SS	= "[ERROR]       ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeMessage : {
			S	= "[LUA] ";
			SS	= "[MESSAGE]     ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeHookCall : {
			S	= "[LUA][HOOK_CALL] ";
			SS	= "[CALL]        ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeHookReturn : {
			S	= "[LUA][HOOK_RETURN] ";
			SS	= "[RETURN]      ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeHookLine : {
			S	= "[LUA][HOOK_LINE] ";
			SS	= "[LINE]        ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeHookCount : {
			S	= "[LUA][HOOK_COUNT] ";
			SS	= "[COUNT]       ";
			break;
		}
		case ScriptStorage::eLuaMessageTypeHookTailReturn : {
			S	= "[LUA][HOOK_TAIL_RETURN] ";
			SS	= "[TAIL_RETURN] ";
			break;
		}
		default : NODEFAULT;
	}
	
	strcpy	(S2,S);
	S1		= S2 + xr_strlen(S);
	int		l_iResult = vsprintf(S1,caFormat,marker);
	Msg		("%s",S2);
	
	strcpy	(S2,SS);
	S1		= S2 + xr_strlen(SS);
	vsprintf(S1,caFormat,marker);
	strcat	(S2,"\r\n");

#ifndef ENGINE_BUILD
#	ifdef DEBUG
		ai().script_engine().m_output.w(S2,xr_strlen(S2)*sizeof(char));
#	endif // DEBUG
#endif // DEBUG

	return	(l_iResult);
#endif
}

#ifdef DEBUG
void CScriptStorage::print_stack		()
{
	if (!m_stack_is_ready)
		return;

	m_stack_is_ready		= false;

	lua_State				*L = game_lua();
	lua_Debug				l_tDebugInfo;
	for (int i=0; lua_getstack(L,i,&l_tDebugInfo);++i ) {
		lua_getinfo			(L,"nSlu",&l_tDebugInfo);
		if (!l_tDebugInfo.name)
			script_log		(ScriptStorage::eLuaMessageTypeError,"%2d : [%s] %s(%d) : %s",i,l_tDebugInfo.what,l_tDebugInfo.short_src,l_tDebugInfo.currentline,"");
		else
			if (!xr_strcmp(l_tDebugInfo.what,"C"))
				script_log	(ScriptStorage::eLuaMessageTypeError,"%2d : [C  ] %s",i,l_tDebugInfo.name);
			else
				script_log	(ScriptStorage::eLuaMessageTypeError,"%2d : [%s] %s(%d) : %s",i,l_tDebugInfo.what,l_tDebugInfo.short_src,l_tDebugInfo.currentline,l_tDebugInfo.name);
	}
}
#endif

int __cdecl CScriptStorage::script_log	(ScriptStorage::ELuaMessageType tLuaMessageType, LPCSTR caFormat, ...)
{

	va_list		marker;
	va_start	(marker,caFormat);
	int			result = vscript_log(tLuaMessageType,caFormat,marker);
	va_end		(marker);

#ifdef DEBUG
#	ifndef ENGINE_BUILD
	static bool	reenterability = false;
	if (!reenterability) {
		reenterability = true;
		if (eLuaMessageTypeError == tLuaMessageType)
			ai().script_engine().print_stack	();
		reenterability = false;
	}
#	endif
#endif

	return		(result);
}

bool CScriptStorage::parse_namespace(LPCSTR caNamespaceName, LPSTR b, LPSTR c)
{
	strcpy			(b,"");
	strcpy			(c,"");
	LPSTR			S2	= xr_strdup(caNamespaceName);
	LPSTR			S	= S2;
	for (int i=0;;++i) {
		if (!xr_strlen(S)) {
			script_log	(ScriptStorage::eLuaMessageTypeError,"the namespace name %s is incorrect!",caNamespaceName);
			xr_free		(S2);
			return		(false);
		}
		LPSTR			S1 = strchr(S,'.');
		if (S1)
			*S1				= 0;

		if (i)
			strcat		(b,"{");
		strcat			(b,S);
		strcat			(b,"=");
		if (i)
			strcat		(c,"}");
		if (S1)
			S			= ++S1;
		else
			break;
	}
	xr_free			(S2);
	return			(true);
}

bool CScriptStorage::load_buffer	(lua_State *L, LPCSTR caBuffer, size_t tSize, LPCSTR caScriptName, LPCSTR caNameSpaceName)
{
	int					l_iErrorCode;
	if (caNameSpaceName && xr_strcmp("_G", caNameSpaceName)) {
		string512		insert, a, b;
		string64		modified = { 0 };

		LPCSTR			header = file_header;
		if (script_file)
		{
			struct tm *mtime = _localtime64 (&script_file->modif);
			strftime(modified, 63, "%d.%m.%Y %H:%M", mtime);
		}

		if (!parse_namespace(caNameSpaceName,a,b))
			return		(false);
		sprintf_s		(insert, 512, header, caNameSpaceName, a, b, modified);
		u32				str_len = xr_strlen(insert);
		LPSTR			script = xr_alloc<char>(str_len + tSize);
		strcpy			(script,insert);
		CopyMemory	(script + str_len,caBuffer,u32(tSize));
//		try 
		{
			l_iErrorCode= luaL_loadbuffer(L,script,tSize + str_len,caScriptName);
		}
//		catch(...) {
//			l_iErrorCode= LUA_ERRSYNTAX;
//		}
		xr_free			(script);
	}
	else {
//		try
		{
			l_iErrorCode= luaL_loadbuffer(L,caBuffer,tSize,caScriptName);
		}
//		catch(...) {
//			l_iErrorCode= LUA_ERRSYNTAX;
//		}
	}

	if (l_iErrorCode) {
// #ifdef DEBUG
		print_output(L,caScriptName,l_iErrorCode);
// #endif
		return			(false);
	}
	return				(true);
}

bool CScriptStorage::do_file	(lua_State *L, LPCSTR caScriptName, LPCSTR caNameSpaceName)
{
	void* lua = NULL;


	int				start = lua_gettop(L);


	string_path		l_caLuaFileName;
	IReader			*l_tpFileReader = FS.r_open(caScriptName);
	if (!l_tpFileReader) {
		script_log	(eLuaMessageTypeError,"Cannot open file \"%s\"",caScriptName);
		return		(false);
	}

	script_file		= FS.exist(caScriptName);
	R_ASSERT(script_file);

	strconcat		(sizeof(l_caLuaFileName),l_caLuaFileName,"@",caScriptName);
		
	LPCSTR lua_text = static_cast<LPCSTR>(l_tpFileReader->pointer());
	if (!load_buffer (L, lua_text,  (size_t)l_tpFileReader->length(),  l_caLuaFileName,caNameSpaceName)) {
//		VERIFY		(lua_gettop(L) >= 4);
//		lua_pop		(L,4);
//		VERIFY		(lua_gettop(L) == start - 3);
		lua_settop	(L,start);
		FS.r_close	(l_tpFileReader);
		return		(false);
	}
	FS.r_close		(l_tpFileReader);

	int errFuncId = -1;
#ifdef LUAICP_COMPAT2 // exception dangerous
	lua_getglobal(L, "AtPanicHandler");
	if ( lua_isfunction(L, -1) )
		errFuncId = lua_gettop(L);
	else
	    lua_pop(L, 1);
#endif

#ifdef USE_DEBUGGER
	if( ai().script_engine().debugger() && errFuncId < 0 )
	    errFuncId = ai().script_engine().debugger()->PrepareLua(L);
#endif
	if (0)	//.
	{
		for (int i = 0; lua_type(L, -i - 1); i++)
		{
			Msg("%2d : %s", -i - 1, lua_typename(L, lua_type(L, -i - 1)));			
		}
	}

	// because that's the first and the only call of the main chunk - there is no point to compile it
//	luaJIT_setmode	(L,0,LUAJIT_MODE_ENGINE|LUAJIT_MODE_OFF);						// Oles
	int	l_iErrorCode = lua_pcall(L, 0, 0,  (-1 == errFuncId) ? 0 : errFuncId);				// new_Andy
//	luaJIT_setmode	(L,0,LUAJIT_MODE_ENGINE|LUAJIT_MODE_ON);						// Oles

#ifdef USE_DEBUGGER
	if( ai().script_engine().debugger() )
		ai().script_engine().debugger()->UnPrepareLua(L, errFuncId);
#endif
	if (l_iErrorCode) {
#ifdef LUAICP_COMPAT
		print_output(L, caScriptName, l_iErrorCode);
#endif
#ifdef DEBUG
		print_output(L,caScriptName,l_iErrorCode);
#endif
		lua_settop	(L, start);
		return		(false);
	}

	return			(true);
}



bool CScriptStorage::load_file_into_namespace(lua_State *L, LPCSTR caScriptName, LPCSTR caNamespaceName)
{
	int				start = lua_gettop(L);
	if (!do_file(L, caScriptName, caNamespaceName)) {
		lua_settop	(L, start);
		return		(false);
	}
	FORCE_VERIFY	(lua_gettop(L) == start);
	return			(true);
}

bool CScriptStorage::namespace_loaded(lua_State *L, LPCSTR N, bool remove_from_stack)
{
	if (NULL == L) L = game_lua();

	int						start = lua_gettop(L);
	lua_pushstring 			(L,"_G"); 
	lua_rawget 				(L,LUA_GLOBALSINDEX); 
	string256				S2;
	strcpy					(S2,N);
	LPSTR					S = S2;
	for (;;) { 
		if (!xr_strlen(S))
			return			(false); 
		LPSTR				S1 = strchr(S,'.'); 
		if (S1)
			*S1				= 0; 
		lua_pushstring 		(L, S); 
		lua_rawget 			(L, -2); 
		if (lua_isnil(L,-1)) { 
//			lua_settop		(L,0);
			VERIFY			(lua_gettop(L) >= 2);
			lua_pop			(L,2); 
			VERIFY			(start == lua_gettop(L));
			return			(false);	//	there is no namespace!
		}
		else 
			if (!lua_istable(L,-1)) { 
//				lua_settop	(L,0);
				Msg("!ERROR: in stack value type = %s", lua_typename(L, -1));
				VERIFY		(lua_gettop(L) >= 1);
				lua_pop		(L,1); 
				VERIFY		(start == lua_gettop(L));
				// FATAL		(" Error : the namespace name is already being used by the non-table object!\n");				
				Debug.fatal		(DEBUG_INFO, " Error : the namespace name %s is already being used by the non-table object!\n", S);
				return		(false); 
			} 
			lua_remove		(L,-2); 
			if (S1)
				S			= ++S1; 
			else
				break; 
	} 
	if (!remove_from_stack) {
		VERIFY				(lua_gettop(L) == start + 1);
	}
	else {
		VERIFY				(lua_gettop(L) >= 1);
		lua_pop				(L,1);
		VERIFY				(lua_gettop(L) == start);
	}
	return					(true); 
}

bool CScriptStorage::object	(LPCSTR identifier, int type)
{
	lua_State *L = game_lua();

	int						start = lua_gettop(L);
	lua_pushnil				(L); 
	while (lua_next(L, -2)) { 
		if ((lua_type(L, -1) == type) && !xr_strcmp(identifier,lua_tostring(L, -2))) { 
			VERIFY			(lua_gettop(L) >= 3);
			lua_pop			(L, 3); 
			VERIFY			(lua_gettop(L) == start - 1);
			return			(true); 
		} 
		lua_pop				(L, 1); 
	} 
	VERIFY					(lua_gettop(L) >= 1);
	lua_pop					(L, 1); 
	VERIFY					(lua_gettop(L) == start - 1);
	return					(false); 
}

bool CScriptStorage::object	(LPCSTR namespace_name, LPCSTR identifier, int type)
{
	lua_State *L = game_lua();

	int						start = lua_gettop(L);
	if (xr_strlen(namespace_name) && !namespace_loaded(L, namespace_name,false)) {
		VERIFY				(lua_gettop(L) == start);
		return				(false); 
	}
	bool					result = object(identifier,type);
	VERIFY					(lua_gettop(L) == start);
	return					(result); 
}

luabind::object CScriptStorage::name_space(lua_State *L, LPCSTR namespace_name)
{
	if (NULL == L) L = game_lua();

	string256			S1;
	strcpy				(S1,namespace_name);
	LPSTR				S = S1;
	luabind::object		lua_namespace = luabind::get_globals(L);
	for (;;) {
		if (!xr_strlen(S))
			return		(lua_namespace);
		LPSTR			I = strchr(S,'.');
		if (!I)
			return		(lua_namespace[S]);
		*I				= 0;
		lua_namespace	= lua_namespace[S];
		S				= I + 1;
	}
}

bool CScriptStorage::print_output(lua_State *L, LPCSTR caScriptFileName, int iErorCode)
{
	if (iErorCode)	
		print_error(L, iErorCode);		
	

	if (!lua_isstring(L,-1))
		return			(false);

	LPCSTR				S = lua_tostring(L,-1);
	if (!xr_strcmp(S,"cannot resume dead coroutine")) {
		VERIFY2	("Please do not return any values from main!!!",caScriptFileName);
#ifdef USE_DEBUGGER
		if(ai().script_engine().debugger() && ai().script_engine().debugger()->Active() ){
			ai().script_engine().debugger()->Write(S);
			ai().script_engine().debugger()->ErrorBreak();
		}
#endif
	}
	else {
		if (!iErorCode)
			script_log	(ScriptStorage::eLuaMessageTypeInfo,"Output from %s",caScriptFileName);

		script_log		(iErorCode ? ScriptStorage::eLuaMessageTypeError : ScriptStorage::eLuaMessageTypeMessage,"%s",S);
#ifdef USE_DEBUGGER

		if (iErorCode)
		{
			if (strstr(S, "no overload of  'net_packet:r_vec3'"))  // при загрузке серверных объектов выполн€ютс€ их кор€вые скрипты с тыщей таких ошибок :(
				 return (true);
			g_error_count ++;

			Msg("!LUA_ERROR(L=%s, code=%d, session=%d): %s", get_lvm_name(L), iErorCode, g_load_session, S);			
			LPCSTR traceback = GetLuaTraceback(L, 1);
			Msg("- %s ", traceback);
			MsgCB("$#DUMP_CONTEXT");
			R_ASSERT2(g_error_count < 100, "To many lua errors ");

#ifdef LUAICP_COMPAT
			static bool inside = false;
			if (!inside)
			 __try 
			 {
				inside = true;
				int top = lua_gettop(L);
				lua_getglobal(L, "DebugDumpAll");
				if (lua_isfunction(L, -1))
				{
					lua_pcall(L, 0, LUA_MULTRET, 0);
				}
				lua_settop(L, top);
			 }
			__finally 
			 {
				inside = false;
		 	 } 
			
#endif 

		}


		if (ai().script_engine().debugger() && ai().script_engine().debugger()->Active()) {
			ai().script_engine().debugger()->Write		(S);
			ai().script_engine().debugger()->ErrorBreak	();
		}
#endif
	}
	return				(true);
}

void CScriptStorage::print_error(lua_State *L, int iErrorCode)
{
	switch (iErrorCode) {
		case LUA_ERRRUN : {
			script_log (ScriptStorage::eLuaMessageTypeError,"SCRIPT RUNTIME ERROR");
			break;
		}
		case LUA_ERRMEM : {
			script_log (ScriptStorage::eLuaMessageTypeError,"SCRIPT ERROR (memory allocation)");
			break;
		}
		case LUA_ERRERR : {
			script_log (ScriptStorage::eLuaMessageTypeError,"SCRIPT ERROR (while running the error handler function)");
			break;
		}
		case LUA_ERRFILE : {
			script_log (ScriptStorage::eLuaMessageTypeError,"SCRIPT ERROR (while running file)");
			break;
		}
		case LUA_ERRSYNTAX : {
			script_log (ScriptStorage::eLuaMessageTypeError,"SCRIPT SYNTAX ERROR");
			break;
		}
		case LUA_YIELD : {
			script_log (ScriptStorage::eLuaMessageTypeInfo,"Thread is yielded");
			break;
		}
		default : NODEFAULT;
	}
}

#ifdef DEBUG
void CScriptStorage::flush_log()
{
	string_path			log_file_name;
	strconcat           (sizeof(log_file_name),log_file_name,Core.ApplicationName,"_",Core.UserName,"_lua.log");
	FS.update_path      (log_file_name,"$logs$",log_file_name);
	m_output.save_to	(log_file_name);
}
#endif // DEBUG
