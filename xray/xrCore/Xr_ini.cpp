#include "stdafx.h"
#pragma hdrstop

#include "fs_internal.h"

XRCORE_API CInifile *pSettings	= NULL;

#pragma optimize("gyts", off)


xr_map<shared_str, int> g_IniFiles;

extern bool shared_str_initialized;

void DumpIniFiles(bool bClear)
{
	if (g_IniFiles.empty() || !shared_str_initialized) return;

	MsgCB("#PERF: not closed ini-files dump:");
	auto it = g_IniFiles.begin();
	for (; it != g_IniFiles.end(); it++)
	{
		MsgCB("\t\t %-40s = %d", it->first.c_str(), it->second);		
	}
	if (bClear)
		g_IniFiles.clear();
}



CInifile* CInifile::Create(const char* szFileName, BOOL ReadOnly)
{	return xr_new<CInifile>(szFileName,ReadOnly); }

void CInifile::Destroy(CInifile* ini)
{	xr_delete(ini); }

XRCORE_API bool sect_pred(const CInifile::Sect *x, LPCSTR val)
{
	return xr_strcmp(*x->Name,val)<0;
};

XRCORE_API bool item_pred(const CInifile::Item& x, LPCSTR val)
{
    if ((!x.first) || (!val))	return x.first<val;
    else				   		return xr_strcmp(*x.first,val)<0;
}

//------------------------------------------------------------------------------
//Тело функций Inifile
//------------------------------------------------------------------------------
XRCORE_API void _parse(LPSTR dest, LPCSTR src)
{
	if (src) {
		BOOL bInsideSTR = false;
		while (*src) {
			if (isspace((u8)*src)) {
				if (bInsideSTR) { *dest++ = *src++; continue; }
				while (*src && isspace(*src)) src++;
				continue;
			} else if (*src=='"') {
				bInsideSTR = !bInsideSTR;
			}
			*dest++ = *src++;
		}
	}
	*dest = 0;
}

XRCORE_API void _decorate(LPSTR dest, LPCSTR src)
{
	if (src) {
		BOOL bInsideSTR = false;
		while (*src) {
			if (*src == ',') {
				if (bInsideSTR) { *dest++ = *src++; }
				else			{ *dest++ = *src++; *dest++ = ' '; }
				continue;
			} else if (*src=='"') {
				bInsideSTR = !bInsideSTR;
			}
			*dest++ = *src++;
		}
	}
	*dest = 0;
}
//------------------------------------------------------------------------------

BOOL	CInifile::Sect::line_exist( LPCSTR L, LPCSTR* val )
{
	SectCIt A = std::lower_bound(Data.begin(),Data.end(),L,item_pred);
    if (A!=Data.end() && xr_strcmp(*A->first,L)==0){
    	if (val) *val = *A->second;
    	return TRUE;
    }
	return FALSE;
}
//------------------------------------------------------------------------------


CInifile::CInifile(IReader* F ,LPCSTR path)
{
	ref_count   = 1;
	fName		= 0;
	bReadOnly	= TRUE;
	bSaveAtEnd	= FALSE;
	get_section = NULL;
	if (pSettings)	
		smIncludeMacro = pSettings->smIncludeMacro;	

	Load		(F,path);
}

CInifile::CInifile(LPCSTR szFileName, BOOL ReadOnly, BOOL bLoad, BOOL SaveAtEnd)
{

	ref_count   = 1;
    bSaveAtEnd	= SaveAtEnd;
	get_section = NULL;
	if (pSettings)	
		smIncludeMacro = pSettings->smIncludeMacro;	


	if (bLoad)
		Load(szFileName, ReadOnly);
	else
		fName = xr_strdup(szFileName); // TODO: leak probability high
}

CInifile::~CInifile( )
{
	if (!bReadOnly&&bSaveAtEnd) {
		if (!save_as())
			Log		("!Can't save inifile:",fName);
	}

	if (fName && shared_str_initialized)
	{
		auto it = g_IniFiles.find(fName);
		if (it != g_IniFiles.end())
		{
			if (--it->second <= 0)
				g_IniFiles.erase(it);
		}
	}
	xr_free			(fName);

	RootIt			I = DATA.begin();
	RootIt			E = DATA.end();
	for ( ; I != E; ++I)
		xr_delete	(I->second);
}

void CInifile::release()
{
	 ref_count--; 
	 if (ref_count <= 0) 
		xr_delete( (CInifile*)this); 
}


XRCORE_API void	insert_item(CInifile::Sect *tgt, const CInifile::Item& I)
{
	CInifile::SectIt_	sect_it		= std::lower_bound(tgt->Data.begin(),tgt->Data.end(),*I.first,item_pred);
	if (sect_it!=tgt->Data.end() && sect_it->first.equal(I.first)){ 
		sect_it->second	= I.second;
#ifdef DEBUG
		sect_it->comment= I.comment;
#endif
	}else{
		tgt->Data.insert	(sect_it,I);
	}
}

void	CInifile::Load(LPCSTR szFileName, BOOL ReadOnly) {

	DATA.clear(); // Root reloading, not recursive

	fName		= szFileName ? xr_strdup(szFileName) : NULL;
	if (fName && shared_str_initialized)
	{
		auto it = g_IniFiles.find(fName);
		if (it != g_IniFiles.end())
			MsgCB("#PERF: already opened ini-file %s, count = %d ", fName, it->second++);
		else
			g_IniFiles[fName] = 0;
	}

    bReadOnly	= ReadOnly;
	
    string_path	path,folder; 
	_splitpath	(fName, path, folder, 0, 0 );
    strcat_s		(path, sizeof(path), folder);
	IReader* R 	= FS.r_open(szFileName);
    if (R){
		Load		(R,path);
		FS.r_close	(R);
    }
	
}


void	CInifile::AddSection(Sect *pSection)
{	
	// RootIt I		= std::lower_bound(DATA.begin(),DATA.end(),*pSection->Name,sect_pred);
	shared_str name (pSection->Name);
	RootIt I =  DATA.find(name);
	if ((I!=DATA.end()) && (I->first == name))
		Debug.fatal(DEBUG_INFO, "Duplicate section '%s' found in file %s. ",*name, (fName ? fName : "NULL"));

	 DATA[name] = pSection;	
}


void	CInifile::Load(IReader* F, LPCSTR path)
{
	R_ASSERT(F);
	Sect		*Current = 0;
	string4096	str;
	string4096	str2;	

	while (!F->eof())
	{
		F->r_string		(str,sizeof(str));
		_Trim			(str);
		LPSTR semi	= strchr(str,';');
		LPSTR semi_1	= strchr(str,'/');
		
		if(semi_1 && (*(semi_1+1)=='/') && ((!semi) || (semi && (semi_1<semi) )) ){
			semi = semi_1;
		}

#ifdef DEBUG
		LPSTR comment	= 0;
#endif
		if (semi) {
			*semi		= 0;
#ifdef DEBUG
			comment		= semi+1;
#endif
		}

        if (str[0] && (str[0]=='#') && strstr(str, "#include")) {
        	xr_string	inc_name;	
			R_ASSERT	(path&&path[0]);
        	if (_GetItem	(str, 1, inc_name, '"')) {
            	string_path	fn, inc_path, folder;

				for (auto it = smIncludeMacro.begin(); it != smIncludeMacro.end(); it++)
				{
					const shared_str &macro = it->first;
					auto pos = inc_name.find(macro.c_str());
					if (pos == xr_string::npos) continue;
					inc_name.replace(pos, macro.size(), it->second.c_str());
				}

                strconcat	(sizeof(fn), fn, path, inc_name.c_str());
				_splitpath	(fn,inc_path,folder, 0, 0 );
				strcat		(inc_path,folder);
            	IReader* I 	= FS.r_open(fn); R_ASSERT2(I, make_string("Can't find include file %s for %s", inc_name.c_str(), fName));
            	Load		(I,inc_path);
                FS.r_close	(I);
            }
        }else if (str[0] && (str[0]=='[')){
			// insert previous filled section
			if (Current)
				AddSection(Current);
			Current				= xr_new<Sect>();
			Current->Name		= 0;
			// start new section
			R_ASSERT3(strchr(str,']'),"Bad ini section found: ",str);
			LPCSTR inherited_names = strstr(str,"]:");
			if (0!=inherited_names){
				VERIFY2			(bReadOnly,"Allow for readonly mode only.");
				inherited_names	+= 2;
				int cnt			= _GetItemCount(inherited_names);
				for (int k=0; k<cnt; ++k){
					xr_string	tmp;
					_GetItem	(inherited_names,k,tmp);
					Sect& inherited_section = r_section(tmp.c_str());
					for (SectIt_ it =inherited_section.Data.begin(); it!=inherited_section.Data.end(); it++)
						insert_item	(Current,*it);
				}
			}
			*strchr(str,']') 	= 0;
			Current->Name 		= strlwr(str+1);
		} else {
			if (Current){
				char*		name	= str;
				char*		t		= strchr(name,'=');
				if (t)		{
					*t		= 0;
					_Trim	(name);
					_parse	(str2,++t);
				} else {
					_Trim	(name);
					str2[0]	= 0;
				}

				Item		I;
				I.first		= (name[0]?name:NULL);
				I.second	= (str2[0]?str2:NULL);
#ifdef DEBUG
				I.comment	= bReadOnly?0:comment;
#endif

				if (bReadOnly) {
					if (*I.first)							insert_item	(Current,I);
				} else {
					if	(
							*I.first
							|| *I.second 
#ifdef DEBUG
							|| *I.comment
#endif
						)
						insert_item	(Current,I);
				}
			}
		}
	}
	if (Current)
		AddSection(Current);	
}

bool	CInifile::save_as( LPCSTR new_fname )
{
	// save if needed
    if (new_fname&&new_fname[0]){
        xr_free			(fName);
        fName			= xr_strdup(new_fname);
    }
    R_ASSERT			(fName&&fName[0]);
    IWriter* F			= FS.w_open_ex(fName);
    if (F){
        string512		temp,val;
        for (RootIt r_it=DATA.begin(); r_it!=DATA.end(); ++r_it)
		{
			Sect *sect = r_it->second;
			Items &list = sect->Data;
			sprintf_s		(temp,sizeof(temp),"[%s]", *sect->Name);
            F->w_string	(temp);
            for (SectCIt s_it= list.begin(); s_it != list.end(); ++s_it)
            {
                const Item&	I = *s_it;
                if (*I.first) {
                    if (*I.second) {
                        _decorate	(val,*I.second);
#ifdef DEBUG
                        if (*I.comment) {
                            // name, value and comment
                            sprintf_s	(temp,sizeof(temp),"%8s%-32s = %-32s ;%s"," ",*I.first,val,*I.comment);
                        } else
#endif
						{
                            // only name and value
                            sprintf_s	(temp,sizeof(temp),"%8s%-32s = %-32s"," ",*I.first,val);
                        }
                    } else {
#ifdef DEBUG
                        if (*I.comment) {
                            // name and comment
                            sprintf_s(temp,sizeof(temp),"%8s%-32s = ;%s"," ",*I.first,*I.comment);
                        } else
#endif
						{
                            // only name
                            sprintf_s(temp,sizeof(temp),"%8s%-32s = "," ",*I.first);
                        }
                    }
                } else {
                    // no name, so no value
#ifdef DEBUG
                    if (*I.comment)
						sprintf_s		(temp,sizeof(temp),"%8s;%s"," ",*I.comment);
                    else
#endif
						temp[0]		= 0;
                }
                _TrimRight			(temp);
                if (temp[0])		F->w_string	(temp);
            }
            F->w_string		(" ");
        }
        FS.w_close			(F);
	    return true;
    }
    return false;
}

BOOL	CInifile::section_exist( LPCSTR S )
{
	// RootIt I = std::lower_bound(DATA.begin(),DATA.end(),S,sect_pred);
	RootIt I = DATA.find(S);
	if (I != DATA.end() && xr_strcmp(I->second->Name, S) == 0) 
		return TRUE;
	if (get_section)
		return get_section(this, S) != NULL;
	return FALSE;
}

void    CInifile::SetIncludeMacro(const shared_str &key, const shared_str &value)
{
	smIncludeMacro[key] = value;
}

BOOL	CInifile::line_exist( LPCSTR S, LPCSTR L )
{
	if (!section_exist(S)) return FALSE;
	Sect&	I = r_section(S);
	SectCIt A = std::lower_bound(I.Data.begin(), I.Data.end(), L, item_pred);
	return (A!=I.Data.end() && xr_strcmp(*A->first,L)==0);
}

u32		CInifile::line_count(LPCSTR Sname)
{
	Sect&	S = r_section(Sname);
	SectCIt	I = S.Data.begin();
	u32	C = 0;
	for (; I!=S.Data.end(); I++)	if (*I->first) C++;
	return  C;
}


//--------------------------------------------------------------------------------------
CInifile::Sect&	CInifile::r_section		( const shared_str& S	)					{ return	r_section(*S);		}
BOOL			CInifile::line_exist	( const shared_str& S, const shared_str& L )	{ return	line_exist(*S,*L);	}
u32				CInifile::line_count	( const shared_str& S	)					{ return	line_count(*S);		}
BOOL			CInifile::section_exist	( const shared_str& S	)					{ return	section_exist(*S);	}

//--------------------------------------------------------------------------------------
// Read functions
//--------------------------------------------------------------------------------------

CInifile::Sect* unsafe_read_section(CInifile &source, LPCSTR S)
{
	CInifile::Sect *result = NULL;
	string256	section; 
	strcpy_s(section,sizeof(section), S); 
	strlwr(section);

	// RootIt I = std::lower_bound(DATA.begin(), DATA.end(), section, sect_pred);
	auto &map = source.sections();
	auto I = map.find(section);			

	if (!(I != map.end() && (I->second->Name == section)))
	{
		if (source.get_section)
			result = source.get_section(&source, section);

		if (!result)
			Debug.fatal(DEBUG_INFO, "Can't open/get section '%s'", S);
	}
	else
		result = I->second;
	
	return result;
}


CInifile::Sect& CInifile::r_section( LPCSTR S )
{	
	__try
	{
		R_ASSERT2(this, "CInifile::r_section, this = NULL!");
		R_ASSERT2(S, "r_section parameter unassigned!");
		return *unsafe_read_section(*this, S);		
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!#EXCEPTION: catched in CIinifile::r_section %s", S);
	}
	
	NODEFAULT;
}


LPCSTR	CInifile::r_string(LPCSTR S, LPCSTR L)
{
	Sect&	I = r_section(S);
	SectCIt	A = std::lower_bound(I.Data.begin(),I.Data.end(),L,item_pred);
	if (A!=I.Data.end() && xr_strcmp(*A->first,L)==0)	return *A->second;
	else
		Debug.fatal(DEBUG_INFO,"Can't find variable %s in [%s], file %s",L,S, this->fName);
	return 0;
}

shared_str		CInifile::r_string_wb(LPCSTR S, LPCSTR L)	{
	LPCSTR		_base		= r_string(S,L);
	
	if	(0==_base)					return	shared_str(0);

	string512						_original;
	strcpy_s						(_original,_base);
	u32			_len				= xr_strlen(_original);
	if	(0==_len)					return	shared_str("");
	if	('"'==_original[_len-1])	_original[_len-1]=0;				// skip end
	if	('"'==_original[0])			return	shared_str(&_original[0] + 1);	// skip begin
	return									shared_str(_original);
}

u8 CInifile::r_u8(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		u8(atoi(C));
}
u16 CInifile::r_u16(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		u16(atoi(C));
}
u32 CInifile::r_u32(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		u32(atoi(C));
}
s8 CInifile::r_s8(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		s8(atoi(C));
}
s16 CInifile::r_s16(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		s16(atoi(C));
}
s32 CInifile::r_s32(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		s32(atoi(C));
}
float CInifile::r_float(LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S, L);	
	float       R = 0;
	if (C && strlen(C))
	__try
	{
		R = float(atof(C));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{
		Msg("! #EXCEPTION(CInifile::r_float): atof(%s) failed, section = '%s', key = '%s' ", C, S, L);
	}
	return R;
}
Fcolor CInifile::r_fcolor( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Fcolor		V={0,0,0,0};
	sscanf		(C,"%f,%f,%f,%f",&V.r,&V.g,&V.b,&V.a);
	return V;
}
u32 CInifile::r_color( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	u32			r=0,g=0,b=0,a=255;
	sscanf		(C,"%d,%d,%d,%d",&r,&g,&b,&a);
	return color_rgba(r,g,b,a);
}

Ivector2 CInifile::r_ivector2( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Ivector2	V={0,0};
	sscanf		(C,"%d,%d",&V.x,&V.y);
	return V;
}
Ivector3 CInifile::r_ivector3( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Ivector		V={0,0,0};
	sscanf		(C,"%d,%d,%d",&V.x,&V.y,&V.z);
	return V;
}
Ivector4 CInifile::r_ivector4( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Ivector4	V={0,0,0,0};
	sscanf		(C,"%d,%d,%d,%d",&V.x,&V.y,&V.z,&V.w);
	return V;
}
Fvector2 CInifile::r_fvector2( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Fvector2	V={0.f,0.f};
	sscanf		(C,"%f,%f",&V.x,&V.y);
	return V;
}
Fvector3 CInifile::r_fvector3( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Fvector3	V={0.f,0.f,0.f};
	sscanf		(C,"%f,%f,%f",&V.x,&V.y,&V.z);
	return V;
}
Fvector4 CInifile::r_fvector4( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	Fvector4	V={0.f,0.f,0.f,0.f};
	sscanf		(C,"%f,%f,%f,%f",&V.x,&V.y,&V.z,&V.w);
	return V;
}
BOOL	CInifile::r_bool( LPCSTR S, LPCSTR L )
{
	LPCSTR		C = r_string(S,L);
	char		B[8];
	strncpy		(B,C,7);
	strlwr		(B);
    return 		IsBOOL(B);
}
CLASS_ID CInifile::r_clsid( LPCSTR S, LPCSTR L)
{
	LPCSTR		C = r_string(S,L);
	return		TEXT2CLSID(C);
}
int		CInifile::r_token	( LPCSTR S, LPCSTR L, const xr_token *token_list)
{
	LPCSTR		C = r_string(S,L);
	for( int i=0; token_list[i].name; i++ )
		if( !stricmp(C,token_list[i].name) )
			return token_list[i].id;
	return 0;
}
BOOL	CInifile::r_line( LPCSTR S, int L, const char** N, const char** V )
{
	Sect&	SS = r_section(S);
	if (L>=(int)SS.Data.size() || L<0 ) return FALSE;
	for (SectCIt I=SS.Data.begin(); I!=SS.Data.end(); I++)
		if (!(L--)){
			*N = *I->first;
			*V = *I->second;
			return TRUE;
		}
	return FALSE;
}
BOOL	CInifile::r_line( const shared_str& S, int L, const char** N, const char** V )
{
	return r_line(*S,L,N,V);
}

//--------------------------------------------------------------------------------------------------------
// Write functions
//--------------------------------------------------------------------------------------
void	CInifile::w_string	( LPCSTR S, LPCSTR L, LPCSTR			V, LPCSTR comment)
{
	R_ASSERT	(!bReadOnly);

	// section
	char	sect	[256];
	_parse	(sect,S);
	_strlwr	(sect);
	if (!section_exist(sect))	{
		// create _new_ section
		Sect			*NEW = xr_new<Sect>();
		NEW->Name		= sect;
		AddSection(NEW);		
	}

	// parse line/value
	char	line	[256];	_parse	(line,L);
	char	value	[256];	_parse	(value,V);

	// duplicate & insert
	Item	I;
	Sect&	data	= r_section	(sect);
	I.first			= (line[0]?line:0);
	I.second		= (value[0]?value:0);
#ifdef DEBUG
	I.comment		= (comment?comment:0);
#endif
	SectIt_	it		= std::lower_bound(data.Data.begin(),data.Data.end(),*I.first,item_pred);

    if (it != data.Data.end()) {
	    // Check for "first" matching
    	if (0==xr_strcmp(*it->first,*I.first)) {
            *it  = I;
        } else {
			data.Data.insert(it,I);
        }
    } else {
		data.Data.insert(it,I);
    }
}
void	CInifile::w_u8			( LPCSTR S, LPCSTR L, u8				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_u16			( LPCSTR S, LPCSTR L, u16				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_u32			( LPCSTR S, LPCSTR L, u32				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_s8			( LPCSTR S, LPCSTR L, s8				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_s16			( LPCSTR S, LPCSTR L, s16				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_s32			( LPCSTR S, LPCSTR L, s32				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_float		( LPCSTR S, LPCSTR L, float				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%f",V);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_fcolor		( LPCSTR S, LPCSTR L, const Fcolor&		V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%f,%f,%f,%f", V.r, V.g, V.b, V.a);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_color		( LPCSTR S, LPCSTR L, u32				V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d,%d,%d,%d", color_get_R(V), color_get_G(V), color_get_B(V), color_get_A(V));
	w_string	(S,L,temp,comment);
}

void	CInifile::w_ivector2	( LPCSTR S, LPCSTR L, const Ivector2&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d,%d", V.x, V.y);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_ivector3	( LPCSTR S, LPCSTR L, const Ivector3&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d,%d,%d", V.x, V.y, V.z);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_ivector4	( LPCSTR S, LPCSTR L, const Ivector4&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%d,%d,%d,%d", V.x, V.y, V.z, V.w);
	w_string	(S,L,temp,comment);
}
void	CInifile::w_fvector2	( LPCSTR S, LPCSTR L, const Fvector2&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%f,%f", V.x, V.y);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_fvector3	( LPCSTR S, LPCSTR L, const Fvector3&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%f,%f,%f", V.x, V.y, V.z);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_fvector4	( LPCSTR S, LPCSTR L, const Fvector4&	V, LPCSTR comment )
{
	string128 temp; sprintf_s		(temp,sizeof(temp),"%f,%f,%f,%f", V.x, V.y, V.z, V.w);
	w_string	(S,L,temp,comment);
}

void	CInifile::w_bool		( LPCSTR S, LPCSTR L, BOOL				V, LPCSTR comment )
{
	w_string	(S,L,V?"on":"off",comment);
}

void	CInifile::remove_line	( LPCSTR S, LPCSTR L )
{
	R_ASSERT	(!bReadOnly);

    if (line_exist(S,L)){
		Sect&	data	= r_section	(S);
		SectIt_ A = std::lower_bound(data.Data.begin(),data.Data.end(),L,item_pred);
    	R_ASSERT(A!=data.Data.end() && xr_strcmp(*A->first,L)==0);
        data.Data.erase(A);
    }
}

