////////////////////////////////////////////////////////////////////////////
//	Module 		: script_ini_file.h
//	Created 	: 21.05.2004
//  Modified 	: 21.05.2004
//	Author		: Dmitriy Iassenev
//	Description : Script ini file class
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "script_token_list.h"
#include "script_export_space.h"

typedef struct _SCRIPT_INI_OBJECT {
public:	
	CInifile *pFileRef;	
	bool	  bAutoType;
} SCRIPT_INI_OBJECT;


typedef struct _SCRIPT_INI_SECTION: public _SCRIPT_INI_OBJECT {
public:
	CInifile::Sect *pSection;
} SCRIPT_INI_SECTION;

class CScriptIniFile : public CInifile {
protected:
	typedef	CInifile inherited;

	int					ref_count;

public:

						CScriptIniFile		(IReader *F, LPCSTR path=0);
						CScriptIniFile		(LPCSTR szFileName, BOOL ReadOnly=TRUE, BOOL bLoadAtStart=TRUE, BOOL SaveAtEnd=TRUE);
	virtual 			~CScriptIniFile		();

			bool		line_exist			(LPCSTR S, LPCSTR L);
			int			section_count		();
			bool		section_exist		(LPCSTR S);
			LPCSTR		section_name		(int index);
			int			r_clsid				(LPCSTR S, LPCSTR L);
			bool		r_bool				(LPCSTR S, LPCSTR L);
			int			r_token				(LPCSTR S, LPCSTR L, const CScriptTokenList &token_list);
			LPCSTR		r_string_wb			(LPCSTR S, LPCSTR L);
			LPCSTR		update				(LPCSTR file_name);
			u32			line_count			(LPCSTR S);
			bool		load				(LPCSTR file_name);
			LPCSTR		r_string			(LPCSTR S, LPCSTR L);
			u32			r_u32				(LPCSTR S, LPCSTR L);
			int			r_s32				(LPCSTR S, LPCSTR L);
			float		r_float				(LPCSTR S, LPCSTR L);
			Fvector		r_fvector3			(LPCSTR S, LPCSTR L);
			Fcolor		r_fcolor			(LPCSTR S, LPCSTR L);
			DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CScriptIniFile)
#undef script_type_list
#define script_type_list save_type_list(CScriptIniFile)

#include "script_ini_file_inline.h"


