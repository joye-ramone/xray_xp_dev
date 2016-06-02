#include "stdafx.h"
#include "string_table.h"

#include "ui/xrUIXmlParser.h"
#include "xr_level_controller.h"
#include "pch_script.h"
#include "../lua_tools.h"
#include "script_engine.h"


STRING_TABLE_DATA* CStringTable::pData = NULL;
BOOL CStringTable::m_bWriteErrorsToLog = FALSE;

// #pragma optimize("gyts", off)
LPCSTR lua_execute(lua_State *L, LPCSTR code, LPCSTR value)
{
	LPCSTR result = code;
	static string64 temp;

	__try
	{
		int save_top = lua_gettop(L);
		// using namespace luabind::detail;
		// lua_pushcfunction(L, CScriptEngine::lua_panic);
		lua_getglobal(L, "AtPanicHandler");
		int err_func = lua_gettop(L);
		if (!lua_iscfunction(L, -1))
		{
			lua_pop(L, 1);
			err_func = 0;
		}


		int err = luaL_loadstring(L, code);
		if (0 == err)
		{
			lua_pushstring(L, value);
			lua_setglobal(L, "g_value");
			if (0 == lua_pcall(L, 0, LUA_MULTRET, err_func))
			{
				int t = lua_type(L, -1);
				result = temp;
				if (LUA_TSTRING == t)
					result = lua_tostring(L, -1);
				else				
					sprintf_s(temp, 64, "#ERROR: invalid value type returned by %s = %d", code, t);
			}

			lua_settop(L, save_top);
		}
	}
	__except (SIMPLE_FILTER)
	{	
		Msg("! #EXCEPTION: catched in script_translate");
		MsgCB("$#DUMP_CONTEXT");
	}

	return result;
}


shared_str script_translate(const shared_str& text, const shared_str &value)
{
	
	shared_str result = text;
	if (text.size() < 50 && strstr(*text, "(") && strstr(*text, ")"));
	else 
		return CStringTable().translate(result);

	LPCSTR func_name = text.c_str();

	std::string code("return ");
	code += func_name;
	size_t index = code.find("$value$");
	if (index != std::string::npos)
  	   code.replace(index, 7, value.c_str());
		
	lua_State *L = AuxLua();
	MsgCB("$#CONTEXT: script_translate('%s', '%s'), lua_State = 0x%p", func_name, value.c_str(), (void*)L);
	result = lua_execute(L, code.c_str(), value.c_str());
    return CStringTable().translate(result);

}

CStringTable::CStringTable	()
{
	Init();
}

void CStringTable::Destroy	()
{
	xr_delete(pData);
}


void CStringTable::Init		()
{
	if(NULL != pData) return;
    
	pData				= xr_new<STRING_TABLE_DATA>();
	MsgCB("##PERF_WARN: CStringTable::Init		() from %d ", GetCurrentThreadId());

	//имя языка, если не задано (NULL), то первый <text> в <string> в XML
	pData->m_sLanguage	= pSettings->r_string("string_table", "language");

	LPCSTR S			= pSettings->r_string("string_table", "files");
	if (S && S[0]) 
	{
		string128	xml_file;
		int			count = _GetItemCount	(S);
		for (int it=0; it<count; ++it)	
		{
			_GetItem	(S,it, xml_file);
			Load		(xml_file);
		}
		MsgCB("##PERF_WARN: CStringTable::Init		() loaded %d files ", count);
	}


}

void CStringTable::Load	(LPCSTR xml_file)
{
	CUIXml						uiXml;
	string_path					xml_file_full;
	strconcat					(sizeof(xml_file_full),xml_file_full, xml_file, ".xml");
	string_path					_s;
	strconcat					(sizeof(_s),_s, STRING_TABLE_PATH, "\\", *(pData->m_sLanguage) );

	bool xml_result				= uiXml.Init(CONFIG_PATH, _s, xml_file_full);
	if(!xml_result)
		Debug.fatal(DEBUG_INFO,"string table xml file not found %s, for language %s", xml_file_full, *(pData->m_sLanguage));

	//общий список всех записей таблицы в файле
	int string_num = uiXml.GetNodesNum		(uiXml.GetRoot(), "string");

	for(int i=0; i<string_num; ++i)
	{
		LPCSTR string_name = uiXml.ReadAttrib(uiXml.GetRoot(), "string", i, "id", NULL);

		VERIFY3					(pData->m_StringTable.find(string_name) == pData->m_StringTable.end(), "duplicate string table id", string_name);

		LPCSTR string_text		= uiXml.Read(uiXml.GetRoot(), "string:text", i,  NULL);

		if(m_bWriteErrorsToLog && string_text)
			Msg("[string table] '%s' no translation in '%s'", string_name, *(pData->m_sLanguage));
		
		VERIFY3						(string_text, "string table entry does not has a text", string_name);
		
		STRING_VALUE str_val		= ParseLine(string_text, string_name, true);
		
		pData->m_StringTable[string_name] = str_val;
	}
}
void CStringTable::ReparseKeyBindings()
{
	if(!pData)					return;
	STRING_TABLE_MAP_IT it		= pData->m_string_key_binding.begin();
	STRING_TABLE_MAP_IT it_e	= pData->m_string_key_binding.end();

	for(;it!=it_e;++it)
	{
		pData->m_StringTable[it->first]			= ParseLine(*it->second, *it->first, false);
	}
}


STRING_VALUE CStringTable::ParseLine(LPCSTR str, LPCSTR skey, bool bFirst)
{
//	LPCSTR str = "1 $$action_left$$ 2 $$action_right$$ 3 $$action_left$$ 4";
	xr_string			res;
	int k = 0;
	const char*			b;
	#define ACTION_STR "$$ACTION_"

//.	int LEN				= (int)xr_strlen(ACTION_STR);
	#define LEN			9

	string256				buff;
	string256				srcbuff;
	bool	b_hit			= false;

	while( (b = strstr( str+k,ACTION_STR)) !=0 )
	{
		buff[0]				= 0;
		srcbuff[0]			= 0;
		res.append			(str+k, b-str-k);
		const char* e		= strstr( b+LEN,"$$" );

		int len				= (int)(e-b-LEN);

		strncpy				(srcbuff,b+LEN, len);
		srcbuff[len]		= 0;
		GetActionAllBinding	(srcbuff, buff, sizeof(buff) );
		res.append			(buff, xr_strlen(buff) );

		k					= (int)(b-str);
		k					+= len;
		k					+= LEN;
		k					+= 2;
		b_hit				= true;
	};

	if(k<(int)xr_strlen(str)){
		res.append(str+k);
	}

	if(b_hit&&bFirst) pData->m_string_key_binding[skey] = str;

	return STRING_VALUE(res.c_str());
}

STRING_VALUE CStringTable::translate (const STRING_ID& str_id) const
{
	VERIFY					(pData);

	STRING_VALUE res =  pData->m_StringTable[str_id];

	if(!res)
	{
		if(m_bWriteErrorsToLog && *str_id != NULL && xr_strlen(*str_id)>0)
			Msg("[string table] '%s' has no entry", *str_id);
		return str_id;
	}
	return					pData->m_StringTable[str_id];
}
