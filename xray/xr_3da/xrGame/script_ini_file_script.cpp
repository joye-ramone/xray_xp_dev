////////////////////////////////////////////////////////////////////////////
//	Module 		: script_ini_file_script.cpp
//	Created 	: 25.06.2004
//  Modified 	: 28.12.2015
//	Author		: Dmitriy Iassenev, Alexander Petrov
//	Description : Script ini file class export + object extension
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_ini_file.h"
#include "script_engine.h"

using namespace luabind;
#pragma optimize("gyts", off)

void get_ini_object(CScriptIniFile *pIniFile, lua_State *L);


CInifile::Sect *ScriptCreateSection(CInifile *ini, LPCSTR szName)
{
	CInifile::Sect *result = NULL;

	lua_State *L = AuxLua();
	R_ASSERT2(L, "Cannot retrieve AuxLua");

	int save_top = lua_gettop(L);
	int err_idx = 0;
	lua_getglobal(L, "AtPanicHandler");
	if (lua_isfunction(L, 1))
		err_idx = save_top + 1;
	else
		lua_pop(L, 1);
	
	lua_getglobal(L, "create_ini_section");
	if (lua_isfunction(L, -1))
		__try
	{
		get_ini_object((CScriptIniFile*)ini, L);
		lua_pushstring(L, szName);
		int err = lua_pcall(L, 2, LUA_MULTRET, err_idx);
		if (0 == err && lua_isuserdata(L, -1))
		{
			SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*)lua_touserdata(L, -1);
			if (pSect)
				result = pSect->pSection;
		}
	}

	__except (SIMPLE_FILTER)
	{
		Log("!#EXCEPTION: catched in ScriptCreateSection");
	}
	else
		Debug.fatal(DEBUG_INFO, "!#ERROR: not found function create_ini_section in global Lua namespace.\n Verify what script file included in common list in script.ltx");

	lua_settop(L, save_top);
	return result;
}


CScriptIniFile *get_system_ini()
{
	pSettings->get_section = ScriptCreateSection;
	return	((CScriptIniFile*)pSettings);
}

xr_map<shared_str, CScriptIniFile*>  g_ini_files;

#ifdef XRGAME_EXPORTS
CScriptIniFile *get_game_ini()
{
	return	((CScriptIniFile*)pGameIni);
}

void CleanSharedIniFiles()
{
	auto it = g_ini_files.begin();
	for (; it != g_ini_files.end(); it++)
	try
	{
		CScriptIniFile *fini = it->second;
		if (fini) xr_delete(fini);
		it->second = NULL;		
	}
	catch (...)
	{
		Msg("!#EXCEPTION: catched in CleanSharedIniFiles()");
	}
	g_ini_files.clear();
}

#endif // XRGAME_EXPORTS





CScriptIniFile *shared_ini_file(LPCSTR szFileName)
{
	shared_str name = szFileName;

	auto it = g_ini_files.find(name);
	if (it != g_ini_files.end())
		return (CScriptIniFile *) it->second->add_ref();
	
	CScriptIniFile *fini = xr_new <CScriptIniFile> (szFileName, TRUE, FALSE, TRUE);
	fini->get_section = ScriptCreateSection;
	__try
	{
		fini->reload();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Msg("! #EXCEPTION: shared_ini_file('%s')", szFileName);
	}
	g_ini_files[name] = fini;
	return (CScriptIniFile *) fini->add_ref();
}


bool r_line(CScriptIniFile *self, LPCSTR S, int L,	xr_string &N, xr_string &V)
{
	THROW3			(self->section_exist(S),"Cannot find section",S);
	THROW2			((int)self->line_count(S) > L,"Invalid line number");
	
	N				= "";
	V				= "";
	
	LPCSTR			n,v;
	bool			result = !!self->r_line(S,L,&n,&v);
	if (!result)
		return		(false);

	N				= n;
	if (v)
		V			= v;
	return			(true);
}

#pragma warning(push)
#pragma warning(disable:4238)
CScriptIniFile *create_ini_file	(LPCSTR ini_string)
{

	return			(
		(CScriptIniFile*)
		xr_new<CInifile>(
			&IReader			(
				(void*)ini_string,
				xr_strlen(ini_string)
			),
			FS.get_path("$game_config$")->m_Path
		)
	);


}
#pragma warning(pop)

XRCORE_API void	insert_item(CInifile::Sect *tgt, const CInifile::Item& I);
XRCORE_API bool item_pred(const CInifile::Item& x, LPCSTR val);

void push_auto_type(lua_State *L, LPCSTR val)
{
	int dots = 0;
	int coms = 0;
	int digs = 0;
	int len = xr_strlen(val);
	for (int i = 0; i < len; i++)
	{
		char ch = val[i];
		if ('.' == ch) dots++;
		if (',' == ch) coms++;
		if ('0' <= ch && ch <= '9') digs++;
	}

	if (coms + dots + digs == len)
	{
		if (dots <= 1) { lua_pushnumber(L, atof(val)); return; }
		if (2 == coms && dots == 3)
		{
			Fvector3	V={0.f,0.f,0.f};
			sscanf		(val,"%f,%f,%f",&V.x,&V.y,&V.z);
			using namespace luabind::detail;
			convert_to_lua<Fvector3>(L, V);
			return;
		}
		
	}
	
	// unknown type
	lua_pushstring(L, val);
}

shared_str* find_value (CInifile::Sect *pSection, LPCSTR key)
{	
	auto A = std::lower_bound(pSection->Data.begin(), pSection->Data.end(), key, item_pred);
	if (A != pSection->Data.end() && xr_strcmp(*A->first, key) == 0)
		return &A->second;
	return NULL;
}

inline void push_value(lua_State *L, CInifile::Sect *pSection, LPCSTR key, bool bAutoType)
{
	shared_str *pstr = find_value(pSection, key);
	if (pstr)
	{
		LPCSTR sval = pstr->c_str();	
		if (bAutoType)
			push_auto_type(L, sval);
		else
			lua_pushstring(L, sval);		
	}
	else
		lua_pushnil(L);
}

int script_sect_diff(lua_State *L)
{
	SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*) lua_touserdata(L, 1);
	LPCSTR ref = lua_tostring(L, 2);
	lua_newtable(L);
	int tidx = lua_gettop(L);
	if (NULL == pSect || NULL == pSect->pFileRef || NULL == ref)
		return 1;
	CInifile *ini = pSect->pFileRef;
	if (!ini->section_exist(ref)) return 1;

	auto &sect	   = *pSect->pSection;
	auto &ref_sect = ini->r_section(ref);
	// параметром предполагаетс€ дочерн€€ секци€, надо сдампить отличи€
	for (size_t it = 0; it < sect.Data.size(); it++)
	{
		auto &I = sect.Data[it];
		shared_str *rv = find_value(&ref_sect, *I.first);
		if (rv && *rv != I.second)
		{
			lua_pushstring(L, *I.first);
			lua_pushstring(L, **rv);
			lua_settable(L, tidx);
		}
	}
	return 1;
}


int script_sect_r_line(lua_State *L)
{
	SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*) lua_touserdata(L, 1);
	 
	if (!pSect)
	{
		lua_pushboolean (L, FALSE);
		lua_pushstring(L, "#fatal_error");
		lua_pushstring(L, "invalid object value");
		return 3;
	}

	auto pSection		= pSect->pSection;

	int index = lua_tointeger(L, 2);
	if (index >= 0 && index < (int)pSection->Data.size())
	{
		auto &pair = pSection->Data[index];
		lua_pushboolean (L, TRUE);
		lua_pushstring  (L, *pair.first);
		push_value(L, pSection, *pair.first, pSect->bAutoType);
	}
	else
	{
		lua_pushboolean (L, FALSE);
		lua_pushstring  (L, "#error_invalid_index");
		lua_pushinteger (L, index);
	}

	return 3;
}

int script_sect_set(lua_State *L)
{
	SCRIPT_INI_SECTION *pSectDst = (SCRIPT_INI_SECTION*) lua_touserdata(L, 1);
	SCRIPT_INI_SECTION *pSectSrc = (SCRIPT_INI_SECTION*) lua_touserdata(L, 2);
	if (pSectDst && pSectSrc && pSectDst->pSection && pSectSrc->pSection)
	{
		auto &src = *pSectSrc->pSection;
		auto &dst = *pSectDst->pSection;

		for (auto it = src.Data.begin(); it != src.Data.end(); it++)		
			dst.Data.push_back(*it);
		
		lua_pushinteger(L, src.Data.size());
	}
	else
		lua_pushinteger(L, 0);
	return 1;
}


int script_sect_index(lua_State *L)
{
	SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*) lua_touserdata(L, 1);

	static xr_map<shared_str, lua_CFunction> methods;
	 
	if (!pSect)
	{
		lua_pushstring(L, "invalid object value");
		return 1;
	}
	
	if (0 == methods.size())
	{
		methods["r_line"]	  = script_sect_r_line;
		methods["diff_items"] = script_sect_diff;
		methods["sync_items"] = script_sect_set;
	}

	CInifile *pIniFile	= pSect->pFileRef;
	auto *pSection		= pSect->pSection;

	switch (lua_type(L, 2))
	{
		 case LUA_TNUMBER:
		{
			size_t index = (size_t) lua_tointeger(L, 2);
			if (index < pSection->Data.size())
			{				
				LPCSTR key = pSection->Data.at(index).first.c_str();
				// lua_pushstring(L, key);
				push_value(L, pSection, key, pSect->bAutoType);
				// lua_pushstring(L, pair.second.c_str());
			}
			else
				lua_pushnil(L);			
			return 1;
		}
		 case LUA_TSTRING:
		 {
			LPCSTR key = lua_tostring(L, 2);
			auto it = methods.find(key);

			if (0 == xr_strcmp(key, "auto_type"))
				lua_pushboolean(L, pSect->bAutoType);
			else
				if (0 == xr_strcmp(key, "lines_count"))
					lua_pushinteger (L, pSection->Data.size());
				else
					if (it != methods.end())
						lua_pushcfunction(L, it->second);
					else					
						push_value(L, pSection, key, pSect->bAutoType);
			return 1;
		}
			 		 
		 case LUA_TTABLE:
		 {			 
			 int keys = lua_objlen(L, 2);
			 string32 keylist[32];
			 ZeroMemory(&keylist, sizeof(keylist));

			 if (keys > 32) keys = 32;
			 // load keys array
			 for (int i = 1; i <= keys; i++)
			 {
				 lua_pushinteger(L, i);
				 lua_gettable(L, 2);
				 strcpy_s (keylist[i - 1], 32, lua_tostring(L, -1));
				 lua_pop(L, 1);	 // delete value			  
			 }

			 // return as array
			 lua_newtable(L);
			 int tidx = lua_gettop(L);
			 for (int i = 0; i < keys; i++)
			 {
				 lua_pushinteger(L, i + 1);
				 push_value(L, pSection, keylist[i], pSect->bAutoType);
				 lua_settable(L, tidx);
			 }

			 return 1;
		 }

	};

	// default
	lua_pushnil(L);
	return 1;

}

int script_sect_newindex(lua_State *L)
{
	SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*) lua_touserdata(L, 1);
	LPCSTR key = lua_tostring(L, 2);
	if (0 == xr_strcmp(key, "auto_type"))
	{
		pSect->bAutoType = (lua_toboolean(L, 3) != FALSE);
		return 0;
	}

	if (!xr_strlen(key)) return 0;

	CInifile *pIniFile = pSect->pFileRef;
	auto pSection = pSect->pSection;


	string4096 buff = { 0 };

	LPCSTR new_val = buff;
	if (!lua_isnil(L, 3))
		strcpy_s(buff, sizeof(buff), lua_tostring(L, 3));	
		
	if (!xr_strlen(new_val))
		 new_val = NULL;
	
	shared_str *pval = find_value(pSection, key);
	if (pval)
		*pval = new_val;		
	else
	{
		CInifile::Item I;		
		I.first = key;
		I.second = new_val;
		insert_item(pSection, I);		
	}

	return 0;
}

void push_section(lua_State *L, CInifile *pIniFile, CInifile::Sect* pSection){

	SCRIPT_INI_SECTION *pSect = (SCRIPT_INI_SECTION*) lua_newuserdata(L, sizeof(SCRIPT_INI_SECTION));
	pSect->pFileRef = pIniFile;
	pSect->pSection = pSection;
	pSect->bAutoType = false;
	int value_index = lua_gettop(L);
	luaL_getmetatable(L, "GMT_SCRIPT_INI_SECT");
	int mt = lua_gettop(L);
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		luaL_newmetatable(L, "GMT_SCRIPT_INI_SECT");
		lua_pushcfunction(L, script_sect_index);	
		lua_setfield(L, mt, "__index");
		lua_pushcfunction(L, script_sect_newindex);	
		lua_setfield(L, mt, "__newindex");

		lua_pushvalue(L, mt);
		lua_setfield(L, mt, "__metatable");
	}
	lua_setmetatable(L, value_index);	
}

CInifile::Sect *find_add_section(CInifile *ini, shared_str name)
{
	const auto &map = ini->sections();
	auto it = map.find(name);
	if (it != map.end())	
			return it->second;
		
	CInifile::Sect *pSection = xr_new<CInifile::Sect>();
	pSection->Name = name;	
	ini->AddSection(pSection);
	return pSection;
}


int script_ini_add_section(lua_State *L)
{	
	SCRIPT_INI_OBJECT *pObj = (SCRIPT_INI_OBJECT*) lua_touserdata(L, 1);
	shared_str name = lua_tostring(L, 2);
	if (!pObj || 0 == name.size())
	{
		lua_pushnil(L);
		return 1;
	}
	CInifile *ini = pObj->pFileRef;
	push_section(L, ini, find_add_section(ini, name));
	return 1;
}


int script_ini_index(lua_State *L)
{
	SCRIPT_INI_OBJECT *pObj = (SCRIPT_INI_OBJECT*) lua_touserdata(L, 1);
	 
	if (pObj && lua_gettop(L) > 1 && !lua_isnil(L, 2))
	{
		CScriptIniFile *pFile = (CScriptIniFile*) pObj->pFileRef;
		size_t sect_count = pFile->sections().size();

		if (lua_type(L, 2) == LUA_TNUMBER)
		{			
			LPCSTR S = pFile->section_name(lua_tointeger(L, 2));	 // медленный поиск, только дл€ дампа секций!
			if (xr_strcmp(S, "NULL"))
				push_section(L, pFile, &pFile->r_section(S));
			else
				lua_pushnil(L);
			return 1;
		}

		LPCSTR key = lua_tostring(L, 2);
		if (0 == strcmp(key, "section_count"))
			lua_pushinteger(L, sect_count);
		else
			if (0 == strcmp(key, "add_section"))
				lua_pushcfunction(L, script_ini_add_section);
			else
				if (0 == strcmp(key, "file_name"))
					lua_pushstring(L, pFile->fname());
				else
				{
					if (pFile->section_exist(key))
						push_section(L, pFile, &pFile->r_section(key));
					else
						lua_pushnil(L);
				}
	}
	else
	{
		lua_pushstring(L, "#ERROR: invalid context/key");		
	}

	return 1;
}


void get_ini_object(CScriptIniFile *pIniFile, lua_State *L)
{			
	SCRIPT_INI_OBJECT *pObj = (SCRIPT_INI_OBJECT*) lua_newuserdata(L, sizeof(SCRIPT_INI_OBJECT));
	pObj->pFileRef = pIniFile;
	int value_index = lua_gettop(L);
	luaL_getmetatable(L, "GMT_SCRIPT_INI");
	int mt = lua_gettop(L);
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		luaL_newmetatable(L, "GMT_SCRIPT_INI");
		lua_pushcfunction(L, script_ini_index);	
		lua_setfield(L, mt, "__index");
		lua_pushvalue(L, mt);
		lua_setfield(L, mt, "__metatable");
	}
	lua_setmetatable(L, value_index);	

}

void wrapper_add_section(CScriptIniFile *pIniFile, lua_State *L)
{
	LPCSTR section  = lua_tostring(L, 2);
	if (!section || 0 == xr_strlen(section))
	{
		lua_pushnil(L);
		return;
	}
	push_section(L, pIniFile, find_add_section(pIniFile, section));
}


void wrapper_read_section(CScriptIniFile *pIniFile, lua_State *L)
{
	LPCSTR section  = lua_tostring(L, 2);
	BOOL   r_table  = lua_toboolean(L, 3);
	if (!section || !pIniFile->section_exist(section))
	{
		lua_pushnil(L);
		return;
	}

	auto &sect = pIniFile->r_section(section);
	if (r_table)
	{
		lua_newtable(L);
		int tidx = lua_gettop(L);
		for (size_t it = 0; it < sect.Data.size(); it++)
		{
			auto &I = sect.Data[it];
			lua_pushstring(L, *I.first);
			lua_pushstring(L, *I.second);
			lua_settable(L, tidx);
		}
	}
	else
		push_section(L, pIniFile, &sect);
}



#pragma optimize("s",on)
void CScriptIniFile::script_register(lua_State *L)
{
	module(L)
	[
		class_<CScriptIniFile>("CScriptIniFile")			
			// .def(					constructor<LPCSTR>())	
			.def("release",			&CScriptIniFile::release)	
			.def("section_count",	&CScriptIniFile::section_count	)	
			.def("section_exist",	&CScriptIniFile::section_exist	)
			.def("section_name",	&CScriptIniFile::section_name	)
			.def("line_exist",		&CScriptIniFile::line_exist		)
			.def("load",			&CScriptIniFile::load)	
			.def("r_clsid",			&CScriptIniFile::r_clsid		)
			.def("r_bool",			&CScriptIniFile::r_bool			)
			.def("r_token",			&CScriptIniFile::r_token		)			
			.def("r_string_wq",		&CScriptIniFile::r_string_wb	)
			.def("line_count",		&CScriptIniFile::line_count)
			.def("r_string",		&CScriptIniFile::r_string)
			.def("r_u32",			&CScriptIniFile::r_u32)
			.def("r_s32",			&CScriptIniFile::r_s32)
			.def("r_float",			&CScriptIniFile::r_float)
			.def("r_vector",		&CScriptIniFile::r_fvector3)
			.def("r_fcolor",		&CScriptIniFile::r_fcolor)			
			.def("r_line",			&::r_line, out_value(_4) + out_value(_5))
			// RAW functions
			.def("add_section",		&wrapper_add_section,	raw(_2))
			.def("r_section",		&wrapper_read_section,	raw(_2))
			.def("object",			&get_ini_object,		raw(_2))	
		,		
			

		def("system_ini",			&get_system_ini),
#ifdef XRGAME_EXPORTS
		def("game_ini",				&get_game_ini),
#endif // XRGAME_EXPORTS
		def("ini_file",				&shared_ini_file),
		def("create_ini_file",		&create_ini_file,	adopt(result))
	];
}