// TextureManager.cpp: implementation of the CResourceManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#pragma hdrstop

#pragma warning(disable:4995)
#include <d3dx9.h>
#pragma warning(default:4995)

#include "ResourceManager.h"
#include "tss.h"
#include "blenders\blender.h"
#include "blenders\blender_recorder.h"
#include "XR_IOConsole.h"

#pragma optimize("gyts", off)

extern void stat_memory_short();

// DLL_API void* find_stored_var(void *storage, LPCSTR key);

bool ptr_compare(PVOID a, PVOID b)
{
	return a < b;
}

#define SHAPER_BLOCK 1048576

class CMemoryShaper 
{
protected:
	xr_vector <PVOID> m_blocks;
public:
	virtual ~CMemoryShaper()
	{
		free_upper(4096, 0);
	}

	u64			alloc_max()
	{
		u64 result = 0;
		while (1)
		{
			PVOID p = VirtualAlloc(NULL, SHAPER_BLOCK, MEM_COMMIT, PAGE_READWRITE);
			if (!p) break;
			m_blocks.push_back(p);
			result += SHAPER_BLOCK;
		}
		std::sort(m_blocks.begin(), m_blocks.end(),  ptr_compare);

		return result;
	}
	void		free_upper(s16 blocks, DWORD_PTR above) // освободить блоки памяти с адресом выше такого-то
	{		
		while (m_blocks.size() > 0 && blocks-- >= 0)
		{
			PVOID p = m_blocks.back();
			if ((DWORD_PTR)p < above) break;

			m_blocks.pop_back();
			if (p)
			    VirtualFree(p, 0, MEM_RELEASE); 
		}
	}
};



XRTEXTURES_API void fix_texture_name(LPSTR fn);

//--------------------------------------------------------------------------------------------------------------
template <class T>
BOOL	reclaim		(xr_vector<T*>& vec, const T* ptr)
{
	R_ASSERT(&vec);
	xr_vector<T*>::iterator it	= vec.begin	();
	xr_vector<T*>::iterator end	= vec.end	();
	for (; it!=end; it++)
		if (*it == ptr)	{ vec.erase	(it); return TRUE; }
		return FALSE;
}

//--------------------------------------------------------------------------------------------------------------
IBlender* CResourceManager::_GetBlender		(LPCSTR Name)
{
	R_ASSERT(Name && Name[0]);

	LPSTR N = LPSTR(Name);
	map_Blender::iterator I = m_blenders.find	(N);
#ifdef _EDITOR
	if (I==m_blenders.end())	return 0;
#else
	if (I==m_blenders.end())	{ Debug.fatal(DEBUG_INFO,"Shader '%s' not found in library.",Name); return 0; }
#endif
	else					return I->second;
}

IBlender* CResourceManager::_FindBlender		(LPCSTR Name)
{
	if (!(Name && Name[0])) return 0;

	LPSTR N = LPSTR(Name);
	map_Blender::iterator I = m_blenders.find	(N);
	if (I==m_blenders.end())	return 0;
	else						return I->second;
}

CTexture*	CResourceManager::_FindTexture(LPCSTR Name)
{
	// copypaste from _CreateTexture
	if (0 == xr_strcmp(Name, "null"))	return 0;
	R_ASSERT(Name && Name[0]);
	string_path		filename;
	strcpy_s(filename, Name); //. andy if (strext(Name)) *strext(Name)=0;
	fix_texture_name(filename);

	LPSTR N = LPSTR(filename);
	char *ch = strstr(N, "*");
	if (NULL == ch) // no wildcard?
	{
		map_TextureIt I = m_textures.find(N);
		if (I != m_textures.end())	return	I->second;
	}
	else
	{
		// alpet: test for wildcard matching
		ch[0] = 0; // remove *

		for (map_TextureIt t = m_textures.begin(); t != m_textures.end(); t++)
		if (strstr(t->second->cName.c_str(), N))
			return t->second;
	}

	return NULL;
}


void	CResourceManager::ED_UpdateBlender	(LPCSTR Name, IBlender* data)
{
	LPSTR N = LPSTR(Name);
	map_Blender::iterator I = m_blenders.find	(N);
	if (I!=m_blenders.end())	{
		R_ASSERT	(data->getDescription().CLS == I->second->getDescription().CLS);
		xr_delete	(I->second);
		I->second	= data;
	} else {
		m_blenders.insert	(mk_pair(xr_strdup(Name),data));
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
void	CResourceManager::_ParseList(sh_list& dest, LPCSTR names)
{
	if (0==names) 		names 	= "$null";

	ZeroMemory			(&dest, sizeof(dest));
	char*	P			= (char*) names;
	svector<char,128>	N;

	while (*P)
	{
		if (*P == ',') {
			// flush
			N.push_back	(0);
			strlwr		(N.begin());

			fix_texture_name( N.begin() );
//. andy			if (strext(N.begin())) *strext(N.begin())=0;
			dest.push_back(N.begin());
			N.clear		();
		} else {
			N.push_back	(*P);
		}
		P++;
	}
	if (N.size())
	{
		// flush
		N.push_back	(0);
		strlwr		(N.begin());

		fix_texture_name( N.begin() );
//. andy		if (strext(N.begin())) *strext(N.begin())=0;
		dest.push_back(N.begin());
	}
}

ShaderElement* CResourceManager::_CreateElement			(ShaderElement& S)
{
	if (S.passes.empty())		return	0;

	// Search equal in shaders array
	for (u32 it=0; it<v_elements.size(); it++)
		if (S.equal(*(v_elements[it])))	return v_elements[it];

	// Create _new_ entry
	ShaderElement*	N		=	xr_new<ShaderElement>(S);
	N->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
	v_elements.push_back	(N);
	return N;
}

void CResourceManager::_DeleteElement(const ShaderElement* S)
{
	if (0==(S->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_elements,S))						return;
	Msg	("! ERROR: Failed to find compiled 'shader-element'");
}

Shader*	CResourceManager::_cpp_Create	(IBlender* B, LPCSTR s_shader, LPCSTR s_textures, LPCSTR s_constants, LPCSTR s_matrices)
{
	CBlender_Compile	C;
	Shader				S;

	//.
	// if (strstr(s_shader,"transparent"))	__asm int 3;

	// Access to template
	C.BT				= B;
	C.bEditor			= FALSE;
	C.bDetail			= FALSE;
#ifdef _EDITOR
	if (!C.BT)			{ ELog.Msg(mtError,"Can't find shader '%s'",s_shader); return 0; }
	C.bEditor			= TRUE;
#endif

	// Parse names
	_ParseList			(C.L_textures,	s_textures	);
	_ParseList			(C.L_constants,	s_constants	);
	_ParseList			(C.L_matrices,	s_matrices	);

	// Compile element	(LOD0 - HQ)
	{
		C.iElement			= 0;		
#if defined(DEBUG) && _SECURE_SCL		
#pragma message("alpet: вылету здесь удивляться не стоит")
		R_ASSERT(C.L_textures._Myfirst);
		R_ASSERT(C.L_textures._Myproxy);
#endif		
		C.bDetail			= m_textures_description.GetDetailTexture(C.L_textures[0],C.detail_texture,C.detail_scaler);
//.		C.bDetail			= _GetDetailTexture(*C.L_textures[0],C.detail_texture,C.detail_scaler);
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[0]				= _CreateElement	(E);
	}

	// Compile element	(LOD1)
	{
		C.iElement			= 1;
//.		C.bDetail			= _GetDetailTexture(*C.L_textures[0],C.detail_texture,C.detail_scaler);
		C.bDetail			= m_textures_description.GetDetailTexture(C.L_textures[0],C.detail_texture,C.detail_scaler);
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[1]				= _CreateElement	(E);
	}

	// Compile element
	{
		C.iElement			= 2;
		C.bDetail			= FALSE;
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[2]				= _CreateElement	(E);
	}

	// Compile element
	{
		C.iElement			= 3;
		C.bDetail			= FALSE;
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[3]				= _CreateElement	(E);
	}

	// Compile element
	{
		C.iElement			= 4;
		C.bDetail			= TRUE;	//.$$$ HACK :)
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[4]				= _CreateElement	(E);
	}

	// Compile element
	{
		C.iElement			= 5;
		C.bDetail			= FALSE;
		ShaderElement		E;
		C._cpp_Compile		(&E);
		S.E[5]				= _CreateElement	(E);
	}

	// Search equal in shaders array
	for (u32 it=0; it<v_shaders.size(); it++)
		if (S.equal(v_shaders[it]))	return v_shaders[it];

	// Create _new_ entry
	Shader*		N			=	xr_new<Shader>(S);
	N->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
	v_shaders.push_back		(N);
	return N;
}

Shader*	CResourceManager::_cpp_Create	(LPCSTR s_shader, LPCSTR s_textures, LPCSTR s_constants, LPCSTR s_matrices)
{
#ifndef DEDICATED_SERVER
	return	_cpp_Create(_GetBlender(s_shader?s_shader:"null"),s_shader,s_textures,s_constants,s_matrices);
#else
	return NULL;
#endif
}

Shader*		CResourceManager::Create	(IBlender*	B,		LPCSTR s_shader,	LPCSTR s_textures,	LPCSTR s_constants, LPCSTR s_matrices)
{
#ifndef DEDICATED_SERVER
	return	_cpp_Create	(B,s_shader,s_textures,s_constants,s_matrices);
#else
	return NULL;
#endif
}

Shader*		CResourceManager::Create	(LPCSTR s_shader,	LPCSTR s_textures,	LPCSTR s_constants,	LPCSTR s_matrices)
{
#ifndef DEDICATED_SERVER
	#ifndef _EDITOR
		if	(_lua_HasShader(s_shader))		return	_lua_Create	(s_shader,s_textures);
		else								
	#endif
		return	_cpp_Create	(s_shader,s_textures,s_constants,s_matrices);
#else
	return NULL;
#endif
}

void CResourceManager::Delete(const Shader* S)
{
	if (0==(S->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_shaders,S))						return;
	Msg	("! ERROR: Failed to find complete shader");
}


// volatile ULONG g_tex_index  = 0;
volatile ULONG g_tex_ldrs = 1;
volatile ULONG g_async_loaded = 0;
CTexture **g_load_list = NULL;  // главный список загрузки

#define MAX_UPLOAD_ONCE 8192
#define MAX_TEX_CHILDS  16


u32 SilentFilter(PEXCEPTION_POINTERS pExPtrs)
{
	return EXCEPTION_EXECUTE_HANDLER;
}


void TexLoadAsync(void *param)
{
	CTexture **items = (CTexture **)param;
	ULONG index = 0;
	CTexture *current = NULL;
	bool main = (param == g_load_list);
	__try
	{

		while (1)
		{
			// index = InterlockedIncrement(&g_tex_index); // гонка за последней текстурой
			if (index > MAX_UPLOAD_ONCE) break;
			current = items[index];
			items [index++] = NULL;
			if (!current) break;			
			if (main)
				current->Load();
			else
				current->LoadImpl();
			InterlockedIncrement(&g_async_loaded);
		}
	}
	__except (SilentFilter(0))
	{
		Msg("!FATAL: exception catched in TexAsyncLoad on thread ~I, index = %d ", index);
		MsgCB("$#DUMP_CONTEXT");
		if (current)
		{
			MsgCB("! texture pointer = 0x%p ", current);
			MsgCB("! texture name    = %s ", current->cName.c_str());
		}

	}
	if (index > 100)
		MsgCB("##PERF: async loader [%4d] completed execution, loaded %4d textures", GetCurrentThreadId(), index);

	InterlockedDecrement(&g_tex_ldrs);
}

void	CResourceManager::DeferredUpload	(BOOL bForce)
{
	if (!Device.b_is_Ready)				return;

	bDeferredLoad = FALSE;
	size_t _free = 0, resv = 0, comm = 0;
	vminfo(&_free, NULL, &comm);
	if (!bForce && _free < 400 * 1024 * 1024)
	{
		Msg("!WARN: free VM = %.1f MiB, DeferredUpload disabled.", (float)_free / 1048576.f );
		return;
	}
	
	Console->Execute("stat_memory"); // before busy

	CMemoryShaper *shaper = NULL;
	if (_free > 2000 * SHAPER_BLOCK)
	{
		
		shaper = xr_new<CMemoryShaper>();
		shaper->alloc_max();
		shaper->free_upper(2048, 0x80000000);
		vminfo(&_free, &resv, &comm);
		MsgCB("##PERF: after memory shaper init free mem = %.3f blocks ", float(_free) / float(SHAPER_BLOCK) );
	}
		 

	CTimer perf;
	perf.Start();


	u32 childs = 0;
	DWORD_PTR mask = 0, sys_mask;
	GetProcessAffinityMask(GetCurrentProcess(), &mask, &sys_mask);
	while (mask > 0) { childs++; mask >>= 1; } // по числу логических процессоров
	childs = __min(childs, MAX_TEX_CHILDS); // alpet: не создавать слишком много потоков



	// g_tex_index = (ULONG)-1; // начинать с 0 многопоточную загрузку
	
	typedef CTexture* TexturesBlock [MAX_UPLOAD_ONCE];
	
	TexturesBlock tx_blocks[MAX_TEX_CHILDS]; // по одному блоку на ЦП, включая главный
	int			  tx_counts[MAX_TEX_CHILDS]; // счетчики
	ZeroMemory(tx_blocks, sizeof(tx_blocks));	
	ZeroMemory(tx_counts, sizeof(tx_counts));	
	int n_tex = 0;	


	int target_block = 0;

	for (map_TextureIt it=m_textures.begin(); it != m_textures.end(); it++)
	{		
		CTexture &t = *it->second;
		if (t.flags.bLoaded || t.flags.MemoryUsage > 0) continue; // already loaded
		float diff_time = Device.fTimeGlobal - t.m_time_apply;
		if (diff_time > 300.f || (t.m_count_apply < 100 &&  t.m_skip_prefetch)) continue; // если последнее применение было давно
		shared_str name(it->first);
		// if (pv && find_stored_var(pv, itoa(name._get()->dwCRC, buff, 10)))  continue;

		// теперь выделенная к загрузке текстура помещается в список одного из потоков
		int block = target_block + 1; // по умолчанию обрабатывает
				
		if (strstr(*name, "$"))
			block = 0;						 // отладка: мелкие/пустые текстуры грузить только в главном потоке
		else
			if (!strstr(*name, "_bump") && childs > 1)		
				target_block = (++target_block) % (childs - 1);  // следующая текстура достанется другому потоку. 

		int &index = tx_counts[block];			
		CTexture **items = tx_blocks[block];		
		items[index ++] = &t;	
		t.m_need_now = true;
		n_tex++;
		if (index > MAX_UPLOAD_ONCE) break;
	}

	if (n_tex > 10)
	{
		MsgCB("##PERF: for DeferredUpload scheduled %d / %d textures ", n_tex, m_textures.size());		
	}


	childs = _min(childs, n_tex); // не требуется много потоков на мало текстур
	g_tex_ldrs = 1;
	g_async_loaded = 0;

	// if (n_tex > 25)
	// ============ создание дополнительных потоков ==============================================
	for (u32 nc = 1; nc < childs; nc++) // реальных надо потоков на 1 меньше!		
		{
			InterlockedIncrement(&g_tex_ldrs);
			string32 name;
			sprintf_s(name, 32, "OnceTexLoader_%d", nc);
			thread_spawn(TexLoadAsync, name, 0, tx_blocks[nc]);
		}

	HANDLE hEvent = CreateEvent (NULL, TRUE, FALSE, "MT_TEXURES_PREFETCH_0");
	if (g_tex_ldrs > 1)
	{
		MsgCB("##PERF: for MT-load used %d additional threads, main thread = ~I", g_tex_ldrs - 1);
		while (g_tex_ldrs > 1)
		{
			Sleep(1); // подождать пока работа закончится	
			size_t resv;
			vminfo(&_free, &resv, &comm);
			if (shaper && _free < 256 * SHAPER_BLOCK)
				shaper->free_upper(16, 0x20000000);
		}
	}
	
	SetEvent(hEvent);
	// g_tex_index = (ULONG)-1;  // load from first
	g_load_list = tx_blocks[0];
	TexLoadAsync (tx_blocks[0]);	
	
	HANDLE hEventExt = OpenEvent (EVENT_ALL_ACCESS, FALSE, "MT_TEXURES_PREFETCH_1");
	if (hEventExt)
	{
		MsgCB("##PERF: Waiting external textures prefetch complete...");
		WaitForSingleObject(hEventExt, 100);
		CloseHandle(hEventExt);
	}
	
	CloseHandle(hEvent);
	if (shaper)
		xr_delete(shaper);

	float elps = perf.GetElapsed_sec();
	if (g_async_loaded)
	{
		MsgCB("##PERF: CResourceManager::DeferredUpload	() time = %.1f ms, loaded %d / %d textures ", elps * 1000.f, g_async_loaded, n_tex);		
		if (g_async_loaded > 100)
			Console->Execute("stat_memory");
	}


	// R_ASSERT(g_async_loaded == n_tex);

	/*
	__try
	{
		if (g_async_loaded > 100) stat_memory_short();
	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("!#EXCEPTION_CATCHED: in stat_memory_short()");
	}
	*/
}
/*
void	CResourceManager::DeferredUnload	()
{
	if (!Device.b_is_Ready)				return;
	for (map_TextureIt t=m_textures.begin(); t!=m_textures.end(); t++)
		t->second->Unload();
}
*/
#ifdef _EDITOR
void	CResourceManager::ED_UpdateTextures(AStringVec* names)
{
	// 1. Unload
	if (names){
		for (u32 nid=0; nid<names->size(); nid++)
		{
			map_TextureIt I = m_textures.find	((*names)[nid].c_str());
			if (I!=m_textures.end())	I->second->Unload();
		}
	}else{
		for (map_TextureIt t=m_textures.begin(); t!=m_textures.end(); t++)
			t->second->Unload();
	}

	// 2. Load
	// DeferredUpload	();
}
#endif

bool  CompareTex(CTexture *a, CTexture *b)
{
	// TODO: сначала надо выгружать текстуры с наибольшим размером, и наименьшим временем загрузки
	if ( a->m_load_delay < b->m_load_delay ) return true;
	if ( a->m_load_delay > b->m_load_delay ) return false;
	return a->flags.MemoryUsage > b->flags.MemoryUsage;
}

void	CResourceManager::_GetMemoryUsage(u32& m_base, u32& c_base, u32& m_lmaps, u32& c_lmaps)
{
	m_base=c_base=m_lmaps=c_lmaps=0;

	map_Texture::iterator I = m_textures.begin	();
	map_Texture::iterator E = m_textures.end	();
	int iters = 0, count = m_textures.size();
	string_path fname;
	FS.update_path(fname, "$logs$", "textures.lst");
	FILE *fh = fopen(fname, "w");

	__int64 need_release = 0;
	size_t _free, comm;
	vminfo(&_free, 0, &comm);

	need_release = 400 * 1048576ll -_free;

	xr_map <shared_str, u32> usage_map;
	xr_vector <LPCSTR> ignore_list;
	ignore_list.push_back("map\\");
	ignore_list.push_back("crosshair");
	ignore_list.push_back("addon");	
	ignore_list.push_back("sky\\");
	ignore_list.push_back("glow");
	ignore_list.push_back("ui\\");

	xr_vector <CTexture *> trash;

	for (; I!=E; I++)
	{
		iters++;
		if (iters > count) 
		{
			MsgCB("!#ERROR: _GetMemoryUsage hang detected, iters = %d / %d", iters, count);
			break;
		}
		CTexture *t = I->second;
		u32 m = 0;
		LPCSTR rname = I->first;

		string_path buff;

		m = t->flags.MemoryUsage;
		if (t->flags.bLoaded && m > 0)
		{
			
			strcpy_s(buff, sizeof(buff) -1, *t->cName);
			LPSTR ctx;
			LPSTR tok = strtok_s(buff, "\\", &ctx);
			if (tok)
				usage_map[tok] = usage_map[tok] + m;

			
			bool unload = false;

			float diff_time = Device.fTimeGlobal - t->m_time_apply;
			if (diff_time > 300.f && m > 16384 && t->m_count_apply < 100)
			{
				unload = true;
				for (size_t it = 0; it < ignore_list.size(); it++)
					if (strstr(rname, ignore_list[it])) unload = false;										
			}
			
			if (unload)
			{
				trash.push_back(t);
			}
			else
			{
				fprintf_s(fh, "%s;%.3f;%.3f\n", rname, float(m) / 1048576.f, float(m_base) / 1048576.f);
				t->m_count_apply = 0; // пусть живет, там посмотрим
			}
		}

		if (!m) continue;
		
		if (strstr(rname,"lmap"))
		{
			c_lmaps	++;
			m_lmaps	+= m;
		} else {
			c_base	++;
			m_base	+= m;
		}
	}

	std::sort(trash.begin(), trash.end(), CompareTex);

	for (size_t i = 0; i < trash.size(); i++)
	{		
		CTexture *t = trash[i];
		size_t m = t->flags.MemoryUsage;

		if (need_release < 0)
		{
			Msg("##PERF: free memory goal reached ");
			break;
		}

		if (m > 256 * 1024)
			MsgCB("##PERF: unload not need texture %s with size = %.3f MiB, apply_count = %d", *(t->cName), float(m) / 1048576.f, t->m_count_apply);
		need_release -= m;
		
		t->Unload(); // выгрузка не востребованной текстуры
		t->m_skip_prefetch = true;
	}
	trash.clear_not_free();

	for (auto it = usage_map.begin(); it != usage_map.end(); it++)
	{
		float size = float(it->second) / 1048576.f;
		if (size > 0.01f)
			fprintf_s(fh, "%s = %.3f\n", *it->first, size);
	}


	fclose(fh);
}
void	CResourceManager::_DumpMemoryUsage		()
{
	xr_multimap<u32,std::pair<u32,shared_str> >		mtex	;

	// sort
	{
		map_Texture::iterator I = m_textures.begin	();
		map_Texture::iterator E = m_textures.end	();
		for (; I!=E; I++)
		{
			u32			m = I->second->flags.MemoryUsage;
			shared_str	n = I->second->cName;
			mtex.insert (mk_pair(m,mk_pair(I->second->dwReference,n) ));
		}
	}

	// dump
	{
		xr_multimap<u32,std::pair<u32,shared_str> >::iterator I = mtex.begin	();
		xr_multimap<u32,std::pair<u32,shared_str> >::iterator E = mtex.end		();
		for (; I!=E; I++)
			Msg			("* %4.1f : [%4d] %s",float(I->first)/1024.f, I->second.first, I->second.second.c_str());
	}
}

void	CResourceManager::Evict()
{
	CHK_DX	(HW.pDevice->EvictManagedResources());
}
/*
BOOL	CResourceManager::_GetDetailTexture(LPCSTR Name,LPCSTR& T, R_constant_setup* &CS)
{
	LPSTR N = LPSTR(Name);
	map_TD::iterator I = m_td.find	(N);
	if (I!=m_td.end())
	{
		T	= I->second.T;
		CS	= I->second.cs;
		return TRUE;
	} else {
		return FALSE;
	}
}*/
