// xrTextures.cpp : Defines the exported functions for the DLL application.
//
#pragma warning(disable:4995)

#include "stdafx.h"
#include "../stdafx.h"
#include "../api_defines.h"
#include "../defines.h"
#include "../device.h"

#include "xrTextures.h"
#include <_types.h>
#include "../ResourceManager.h"
#include "../xrRender/xrRender_console.h"
#include "raster_inline.h"
//#include "../HW.h"

#pragma comment(lib,"d3dx9")
#pragma comment(lib,"xrCore")
#pragma optimize("gyts", off)
// #pragma optimize("gyts", on)

#define MAX_RECT_X  8192
#define MAX_RECT_Y  8192

#define MIN_RECT_X -8192
#define MIN_RECT_Y -8192



static D3DFORMAT unpacked		= D3DFMT_UNPACKED;
static D3DFORMAT packed			= D3DFMT_PACKED;
XRTEXTURES_API Flags32  ps_r2_tx_flags = { 0xff };

xrCriticalSection				g_dp_access;
xr_map <PVOID, shared_str>      g_dp_textures;  // для отладки нормализации выгрузки текстур DEFAULT_POOL


UINT  g_texmem_available = 0;

LPDIRECT3DDEVICE9 g_pDevice = NULL;

ULONG TexRelease(PDIRECT3DTEXTURE9 &tex, BOOL bWarn = TRUE)
{
	if (NULL == tex) return 0;
	ULONG refs = 0;
	__try
	{
		refs = tex->Release();

		if (refs > 0)
		{
			if (bWarn)
				Msg("! #LEAK_WARN: TexRelease refs = %d", refs);
		}
		else
		{
			g_dp_access.Enter();
			auto it = g_dp_textures.find((PVOID)tex);
			if (it != g_dp_textures.end())
				g_dp_textures.erase(it);
			g_dp_access.Leave();
			tex = NULL;
		}
	}
	__except (SIMPLE_FILTER)
	{
		Msg("! #EXCEPTION: in TexRelease");
	}	
	return refs;
}


template <class INT_RECT> 
void DumpIntRect(LPCSTR msg, const INT_RECT &R)
{
	MsgCB("%s  %4d, %4d, %4d, %4d ", msg, R.left, R.top, R.right, R.bottom);
}

template <class INT_RECT> 
ICF bool VerifyRect(const INT_RECT &rect, LPCSTR msg, LPCSTR file, int line, LPCSTR function)
{
	if (rect.top >= MIN_RECT_X && rect.bottom >= MIN_RECT_Y && rect.right < MAX_RECT_X && rect.bottom < MAX_RECT_Y)
		return true;
	else
	{
		DumpIntRect(xrx_sprintf("%s at %s:%d in function %s ", msg, file, line, function), rect);
		return false;
	}
}



template <class INT_RECT> 
ICF bool SafeSetRect (LPRECT pRect, const INT_RECT &rect, LPCSTR file, int line, LPCSTR function) // TODO: move to inline header
{	
	if (VerifyRect(rect, "! #FAIL(SafeSetRect):", file, line, function))
	{
		pRect->left		= rect.left;
		pRect->top		= rect.top;
		pRect->right		= rect.right;
		pRect->bottom	= rect.bottom;
		return true;
	}
	else
		return false;
}




XRTEXTURES_API SIZE_T  AvailVideoMemory()
{
	if (NULL == g_pDevice) return 0;
	g_texmem_available = g_pDevice->GetAvailableTextureMem();
	return g_texmem_available;
}

IC bool AllowedDefaultPool(LPCSTR name, UINT vmem_rest = 512 * 1048576)
{
	/*
	bool name_check = (NULL == strstr(name, "~vmt\\") && NULL == strstr(name, "$user$") &&
					   NULL == strstr(name, "sky\\")  && NULL == strstr(name, "glow"));
					   */
	bool name_check = (strstr(name, "act\\") || strstr(name, "map\\") || strstr(name, "swmt\\") || strstr(name, "wpn\\"));

	return ( ps_r2_tx_flags.test(R2FLAG_DEFAULT_POOL) && name_check  &&  AvailVideoMemory() > vmem_rest );
}


int get_texture_load_lod(LPCSTR fn)
{
	CInifile::Sect& sect	= pSettings->r_section("reduce_lod_texture_list");
	CInifile::SectCIt it_	= sect.Data.begin();
	CInifile::SectCIt it_e_	= sect.Data.end();

	CInifile::SectCIt it	= it_;
	CInifile::SectCIt it_e	= it_e_;

	for(;it!=it_e;++it)
	{
		if( strstr(fn, it->first.c_str()) )
		{
			if(psTextureLOD < 1)
				return 0;
			else
			if(psTextureLOD < 3)
				return 1;
			else
				return 2;
		}
	}

	if(psTextureLOD < 2)
		return 0;
	else
	if(psTextureLOD < 4)
		return 1;
	else
		return 2;
}

u32 calc_texture_size(int lod, u32 mip_cnt, u32 orig_size)
{
	if(1==mip_cnt)
		return orig_size;

	int _lod		= lod;
	float res		= float(orig_size);

	while(_lod>0){
		--_lod;
		res		-= res/1.333f;
	}
	return iFloor	(res);
}

const float		_BUMPHEIGH = 8.f;
//////////////////////////////////////////////////////////////////////
// Utility pack
//////////////////////////////////////////////////////////////////////
IC u32 GetPowerOf2Plus1	(u32 v)
{
        u32 cnt=0;
        while (v) {v>>=1; cnt++; };
        return cnt;
}

void				TW_Save	(IDirect3DTexture9* T, LPCSTR name, LPCSTR prefix, LPCSTR postfix)
{
	string256		fn;		strconcat	(sizeof(fn),fn,name,"_",prefix,"-",postfix);
	for (int it=0; it<int(xr_strlen(fn)); it++)	
		if ('\\'==fn[it])	fn[it]	= '_';
	string256		fn2;	strconcat	(sizeof(fn2),fn2,"debug\\",fn,".dds");
	Log						("* debug texture save: ",fn2);
	R_CHK					(D3DXSaveTextureToFile	(fn2,D3DXIFF_DDS,T,0));
}

// Класс текстурного контекста

CTextureContext::CTextureContext()
{
	m_info = m_locked = NOT_SELECTED;
	m_owner = false; 
	m_container = NULL;
	m_texture   = NULL; 
	m_locks = m_mem_usage = 0;
	m_rect.pBits = NULL;
	m_rect.Pitch = NULL;
	m_pool = D3DPOOL_MANAGED;
	m_error_count = 0;
	m_load_flags.assign(TXLOAD_APPLY_LOD);
}
CTextureContext::~CTextureContext()
{
	assign_texture(NULL);
	m_last_error = NULL;
}



void				CTextureContext::assign_texture(PDIRECT3DTEXTURE9 pTexture, bool own)
{
	if (m_texture == pTexture) return;	
	

	if (m_texture && m_owner)
	{		
		LPCSTR name = "*independent*";
		if (m_container && NULL != m_container->cName)
			name = *(m_container->cName);
		MsgCB("$#CONTEXT: releasing texture '%s' interface 0x%p", name, (void*)m_texture);	
		if (m_rect.pBits && m_locked != NOT_SELECTED)
			unlock_rect(true);		


		TexRelease(m_texture, FALSE);  // убрать ссылку добавленную assign_texture
		ULONG refs = TexRelease(m_texture, FALSE);  // вообще выгрузить текстуру 
		if (m_texture && !strstr(name, "$user$"))
		{	
			MsgCB("!#LEAK_WARN: after releasing texture '%s%', refs count =~C0D %d", name, refs);
		}		
	}

	m_mem_usage = 0;
	m_cropped = false;
	m_locked = NOT_SELECTED;
	m_info   = NOT_SELECTED;
	m_locks  = 0;
	m_texture = pTexture;
	m_owner	   = own;
	m_rect.pBits = NULL;
	m_rect.Pitch = 0;
	m_error_count = 0;
	m_selection.left = 0;
	m_selection.top  = 0;
	m_selection.right = 0;
	m_selection.bottom = 0;	
	if (NULL == m_texture) return;
	if (m_owner) m_texture->AddRef();
	load_desc(0);
	if (D3DPOOL_DEFAULT == m_desc.Pool)
	{
		shared_str name = m_container ? m_container->cName : "#nonamee";
		g_dp_access.Enter();
		g_dp_textures[(PVOID)pTexture] = name;
		g_dp_access.Leave();
		if (strstr(*name, "artifact_hell1")) // leak check
			__asm nop;  
	}
}

PDIRECT3DTEXTURE9	CTextureContext::create_texture(UINT Width, UINT Height, UINT Levels, D3DFORMAT Format, D3DPOOL Pool)
{
	PDIRECT3DTEXTURE9 result = NULL;
	HRESULT hErr = D3DXCreateTexture(g_pDevice, Width, Height, Levels, 0, Format, Pool, &result);
	// g_pDevice->CreateTexture(Width, Height,  Levels, 0, Format, Pool, &result, NULL);
	if (result && D3D_OK == hErr)
	{
		m_pool = Pool;
		assign_texture(result, true);			

	}
	else
	{
		m_last_error.sprintf("CreateTexture failed with result 0x%08x", hErr);
		MsgCB("!#WARN: %s", *m_last_error);
	}	
	return result;
}



void				CTextureContext::get_selection(PRECT pRect)
{
	if (!m_cropped)			
	{
		if (m_info == NOT_SELECTED) load_desc(0);		
		SetRect(&m_selection, 0, 0, m_desc.Width, m_desc.Height);
	}
		
	SafeSetRect(pRect, m_selection, DEBUG_INFO);		
}

D3DSURFACE_DESC*	CTextureContext::load_desc(UINT Level)
{
	R_ASSERT(m_texture);
	if (Level == m_info) return &m_desc;

	m_info = NOT_SELECTED;
	if (D3D_OK == m_texture->GetLevelDesc(Level, &m_desc))
	{
		m_info = Level;
		if (0 == Level)
			SetRect(&m_selection, 0, 0, m_desc.Width, m_desc.Height);
		return &m_desc;
	}
	else
		return NULL;
}

D3DLOCKED_RECT*		CTextureContext::lock_rect(UINT Level, LPRECT pRect, UINT Flags)
{
	// если области блокировки не совпадают
	if ( pRect && (!m_cropped || !EqualRect(&m_selection, pRect)) )		
		unlock_rect(true);
	if (NULL == pRect && m_cropped)
		unlock_rect(true);		


	if (Level == m_locked)
	{
		m_locks++;
		return &m_rect;
	}

	if (NOT_SELECTED != m_locked) 
		unlock_rect(true);

	if (pRect)
	{
		SafeSetRect(&m_selection, *pRect, DEBUG_INFO);		
		pRect = &m_selection;
		m_cropped = true;
	}
	else 
		m_cropped = false;

	static int total_locks = 0;

	if (D3D_OK == m_texture->LockRect(Level, &m_rect, pRect, Flags))
	{
		m_locks ++;
		m_locked = Level;
		total_locks++;
		if (0 == total_locks % 10000)
			MsgCB("##PERF: total_locks =~C0D %d ", total_locks);

		return &m_rect;
	}
		
	return NULL;
}

bool			CTextureContext::unlock_rect(bool Force)
{
	if (m_locks > 0) m_locks--;
	if (!Force && m_locks > 0) return false;			
	if (m_texture && m_rect.Pitch)
	{
		m_texture->UnlockRect(m_locked);
		ZeroMemory(&m_rect, sizeof(m_rect));
	}
	// if (0 == m_locks)	
	m_locked = NOT_SELECTED;			
	return true;
}

bool		   CTextureContext::load_texture(PDIRECT3DTEXTURE9 pTextureFrom, D3DFORMAT&  DestFmt, int levels_2_skip, SIZE *rect_size, int useDefaultPool)
{
	m_mem_usage = 0;
	if (NULL == pTextureFrom)
	{
		pTextureFrom = get_texture();
	}
	else
		assign_texture(NULL);

	if (!g_pDevice) return false;

	// Calculate levels & dimensions
	IDirect3DTexture9*		t_dest			= NULL;
	D3DSURFACE_DESC			desc;
	R_CHK					(pTextureFrom->GetLevelDesc	(0,&desc));
	int top_width			= desc.Width;
	int top_height			= desc.Height;
	if (D3DPOOL_DEFAULT == desc.Pool)
	{
		m_last_error = "load_texture: invalid pool for source texture";
		return false;
	}
	

	int levels_exist		= pTextureFrom->GetLevelCount();
	ReduceDimensions		(top_width, top_height, levels_exist,levels_2_skip);
	// Create HW-surface
	if (D3DX_DEFAULT == DestFmt)	DestFmt = desc.Format;
	LPCSTR name = "?";
	if (m_container)
		name = *m_container->cName;

	m_pool = D3DPOOL_MANAGED;

	int full_size = top_width * top_height * 4;
	// если распакованный формат, то помещение в D3DPOOL_DEFAULT накладно. Без форсажа отклонить
	if ( (m_load_flags.test(TXLOAD_MANAGED) || DestFmt == unpacked) && useDefaultPool < 2  ) useDefaultPool = 0;

	UINT vmem_rest = 512 * 1048576;
	if (useDefaultPool > 1)
		vmem_rest /= 4;

	if (useDefaultPool && full_size >= 8192 && AllowedDefaultPool(name, vmem_rest))
	{
		MsgCB("$#CONTEXT: allowed D3DPOOL_DEFAULT for %s, video memory rest = %.1f MiB ", name, (float) g_texmem_available / 1048576.f );
		m_pool = D3DPOOL_DEFAULT;
	}

	t_dest = create_texture(top_width, top_height, levels_exist, DestFmt, m_pool);	
	if (NULL == t_dest)
		return false;
	assign_texture(t_dest, true);
	copy_rect(pTextureFrom);
	load_desc(0); // m_selection must be full rect of texture
	
	// OK
	if (rect_size)
	{
		rect_size->cx = top_width;
		rect_size->cy = top_height;
	}
	return					true;
}

bool CTextureContext::load_texture(shared_str fRName, Flags32 load_flags)
{
	SetActiveDevice(g_pDevice);
	m_last_error = "OK";
	m_mem_usage  = 0;

	m_load_flags = load_flags;

	D3DFORMAT Format = D3DFMT_DEFAULT;	

	if (m_load_flags.test(TXLOAD_UNPACKED)) 
		Format = unpacked;

	int useDefaultPool = 1;
	if (m_load_flags.test(TXLOAD_DEFAULT_POOL))
		useDefaultPool = 2;
	if (m_load_flags.test(TXLOAD_MANAGED))
		useDefaultPool = 0;

	IDirect3DTexture9*		pTexture2D		= NULL;
	IDirect3DCubeTexture9*	pTextureCUBE	= NULL;
	string_path				fn;
	u32						dwWidth,dwHeight;
	u32						img_size		= 0;
	int						img_loaded_lod	= 0;
	D3DFORMAT				fmt;
	u32						mip_cnt=u32(-1);
	// validation
	R_ASSERT				(fRName);
	R_ASSERT				(fRName.c_str()[0]);

	// make file name
	string_path				fname;
	strcpy_s (fname, sizeof(fname), *fRName); //. andy if (strext(fname)) *strext(fname)=0;
	fix_texture_name		(fname);
	IReader* S				= NULL;
	//if (FS.exist(fn,"$game_textures$",fname,	".dds")	&& strstr(fname,"_bump"))	goto _BUMP;
	if (!FS.exist(fn,"$game_textures$",	fname,	".dds")	&& strstr(fname,"_bump"))	goto _BUMP_from_base;
	if (FS.exist(fn,"$level$",			fname,	".dds"))							goto _DDS;
	if (FS.exist(fn,"$game_saves$",		fname,	".dds"))							goto _DDS;
	if (FS.exist(fn,"$game_textures$",	fname,	".dds"))							goto _DDS;

#ifdef _EDITOR
	ELog.Msg(mtError,"Can't find texture '%s'",fname);
#else
	// KD: we don't need to die :)
	//Debug.fatal(DEBUG_INFO,"Can't find texture '%s'",fname);
	m_last_error.sprintf("Can't find texture '%s' in all pathes",fname);
	Msg("! #ERROR: %s", *m_last_error);
#endif


	return false;

_DDS:
	{
		// Load and get header
		D3DXIMAGE_INFO			IMG;
		S						= FS.r_open	(fn);
#ifdef DEBUG
		Msg						("* Loaded: %s[%d]b",fn,S->length());
#endif // DEBUG
		img_size				= S->length	();

		R_ASSERT				(S);
		R_CHK2					(D3DXGetImageInfoFromFileInMemory	(S->pointer(),S->length(),&IMG), fn);
		if (IMG.ResourceType	== D3DRTYPE_CUBETEXTURE)			goto _DDS_CUBE;
		else														goto _DDS_2D;

_DDS_CUBE:
		{
			m_pool = useDefaultPool && AllowedDefaultPool(fn) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;

			R_CHK(D3DXCreateCubeTextureFromFileInMemoryEx(
				g_pDevice,
				S->pointer(),S->length(),
				D3DX_DEFAULT,
				IMG.MipLevels,0,
				IMG.Format,
				m_pool,
				D3DX_DEFAULT,
				D3DX_DEFAULT,
				0,&IMG,0,
				&pTextureCUBE
				));
			FS.r_close				(S);

			// OK
			dwWidth					= IMG.Width;
			dwHeight				= IMG.Height;
			fmt						= IMG.Format;
			m_mem_usage				= calc_texture_size(img_loaded_lod, mip_cnt, img_size);
			mip_cnt					= pTextureCUBE->GetLevelCount();

			assign_texture			( (PDIRECT3DTEXTURE9) pTextureCUBE, true );
			return					true;
		}
_DDS_2D:
		{
			// Check for LMAP and compress if needed
			strlwr					(fn);
			// Load   SYS-MEM-surface, bound to device restrictions
			IDirect3DTexture9*		T_sysmem;
			D3DPOOL pool = D3DPOOL_SYSTEMMEM;			
			
			CTimer check;
			check.Start();
			void * fdata = S->pointer();
			size_t fsize = S->length();

			UINT dim = D3DX_DEFAULT;

			if (HW.Caps.raster.bNonPow2  && ps_r2_tx_flags.test(R2FLAG_ALLOW_NONPOW2)) 
				dim = D3DX_DEFAULT_NONPOW2;


			R_CHK2(D3DXCreateTextureFromFileInMemoryEx
					(
						g_pDevice, fdata, fsize,
						// D3DX_DEFAULT,D3DX_DEFAULT,
						dim, dim,
						IMG.MipLevels,0,
						IMG.Format,
						D3DPOOL_SYSTEMMEM,
						D3DX_DEFAULT,
						D3DX_DEFAULT,
						0,&IMG,0,
						&T_sysmem  // увеличивает memory committed
					), fn);
			FS.r_close				(S);		

			float elps = check.GetElapsed_sec();
			
			void *sys_t = (void*)T_sysmem;

			img_loaded_lod			= get_texture_load_lod(fn);
			
			if (!m_load_flags.test(TXLOAD_APPLY_LOD))
				img_loaded_lod = 0;


			load_texture (T_sysmem, Format, img_loaded_lod, NULL, useDefaultPool);		
			auto *pTexture2D		= get_texture();
			if (!pTexture2D)  return false;

			mip_cnt					= pTexture2D->GetLevelCount();
			// OK
			fmt						= Format;
			m_mem_usage				= calc_texture_size(img_loaded_lod, mip_cnt, img_size);

			if (elps > 1.f)
			{
				MsgCB("##PERF: D3DXCreateTextureFromFileInMemoryEx work time %.2f sec, source size = %d, mem usage = %d ",
					  elps, fsize, m_mem_usage);
				MsgCB(" source ptr = 0x%p, load result = 0x%p, surface ptr = 0x%p ", fdata, sys_t, (void*)pTexture2D);
			}

			TexRelease(T_sysmem);// _RELEASE				(T_sysmem);			
			return					true;
		}
	}
	
_BUMP_from_base:
	{
		MsgV			("5PERF", "! auto-generated bump map: %s",fname);
		
//////////////////
		if (strstr(fname,"_bump#"))			
		{
			R_ASSERT2	(FS.exist(fn,"$game_textures$",	"ed\\ed_dummy_bump#",	".dds"), "ed_dummy_bump#");
			S						= FS.r_open	(fn);
			R_ASSERT2				(S, fn);
			img_size				= S->length	();
			goto		_DDS_2D;
		}
		if (strstr(fname,"_bump"))			
		{
			R_ASSERT2	(FS.exist(fn,"$game_textures$",	"ed\\ed_dummy_bump",	".dds"),"ed_dummy_bump");
			S						= FS.r_open	(fn);

			R_ASSERT2	(S, fn);

			img_size				= S->length	();
			goto		_DDS_2D;
		}
//////////////////
		CTimer perf;		
		perf.Start();


		*strstr		(fname,"_bump")	= 0;
		R_ASSERT2	(FS.exist(fn,"$game_textures$",	fname,	".dds"),fname);

		// Load   SYS-MEM-surface, bound to device restrictions
		D3DXIMAGE_INFO			IMG;
		S						= FS.r_open	(fn);
		img_size				= S->length	();
		IDirect3DTexture9*		T_base;
		R_CHK2(D3DXCreateTextureFromFileInMemoryEx(
			g_pDevice,	S->pointer(),S->length(),
			D3DX_DEFAULT,D3DX_DEFAULT,	D3DX_DEFAULT,0,D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM,			D3DX_DEFAULT,D3DX_DEFAULT,
			0,&IMG,0,&T_base	), fn);
		FS.r_close				(S);

		// Create HW-surface
		IDirect3DTexture9*	T_normal_1	= 0;
		dwWidth = IMG.Width;
		dwHeight = IMG.Height;


		R_CHK(D3DXCreateTexture		(g_pDevice, dwWidth, dwHeight, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM, &T_normal_1));
		R_CHK(D3DXComputeNormalMap	(T_normal_1,T_base,0,D3DX_NORMALMAP_COMPUTE_OCCLUSION,D3DX_CHANNEL_LUMINANCE,_BUMPHEIGH));

		// Transfer gloss-map
		UINT mips = IMG.MipLevels;
		CTextureContext src;
		src.assign_texture(T_base);
		CTextureContext dst;
		dst.assign_texture(T_normal_1);

		UINT param = 0;

		for (UINT level = 0; level < mips; level++)
		{
			TW_Iterate_1OP (level, dst, src, NULL, it_gloss_rev_base, param);
		}

		// Compress
		fmt								= D3DFMT_DXT5;
		img_loaded_lod					= get_texture_load_lod(fn);
		dst.load_texture				(T_normal_1, fmt, img_loaded_lod);
		auto *T_normal_1C = dst.get_texture();
		// IDirect3DTexture9*	T_normal_1C	= TW_LoadTextureFromTexture(T_normal_1, fmt, img_loaded_lod, dwWidth, dwHeight);
		mip_cnt							= T_normal_1C->GetLevelCount();

#if RENDER==R_R2	
		// Decompress (back)
		fmt								= D3DFMT_A8R8G8B8;
		src.load_texture (T_normal_1C, fmt, 0);
		IDirect3DTexture9*	T_normal_1U = src.get_texture();
		// Calculate difference
		IDirect3DTexture9*	T_normal_1D = 0;
		const UINT levels = T_normal_1U->GetLevelCount();
		R_CHK(D3DXCreateTexture(g_pDevice,dwWidth,dwHeight,levels,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&T_normal_1D));
		dst.assign_texture(T_normal_1D);
		CTextureContext normal;
		normal.assign_texture(T_normal_1U);
		CTextureContext base;
		base.assign_texture(T_base);

		for (UINT level = 0; level < levels; level++)
		{
			TW_Iterate_2OP(level, dst, src, normal, NULL, it_difference);
			// TW_Iterate_2OP		(T_normal_1D,T_normal_1,T_normal_1U, offset, NULL, it_difference);

			// Reverse channels back + transfer heightmap
			TW_Iterate_1OP(level, dst, base, NULL, it_height_rev_base, param);
		}

		// Compress
		fmt								= D3DFMT_DXT5;
		dst.load_texture(T_normal_1D, fmt);
		IDirect3DTexture9*	T_normal_2C = dst.get_texture();
		// IDirect3DTexture9*	T_normal_2C	= TW_LoadTextureFromTexture(T_normal_1D,fmt,0,dwWidth,dwHeight);
		TexRelease				        (T_normal_1U	);
		TexRelease						(T_normal_1D	);

		// 
		string256			fnameB;
		strconcat			(sizeof(fnameB),fnameB,"$user$",fname,"_bumpX");
		ref_texture			t_temp			= Device.Resources->_CreateTexture	(fnameB);
		t_temp->surface_set	(T_normal_2C	);
		TexRelease			(T_normal_2C	);	// texture should keep reference to it by itself
#endif
		// T_normal_1C	- normal.gloss,		reversed
		// T_normal_2C	- 2*error.height,	non-reversed
		TexRelease			(T_base);
		TexRelease			(T_normal_1);
		m_mem_usage			= calc_texture_size(img_loaded_lod, mip_cnt, img_size);

		float	elps = perf.GetElapsed_sec() * 1000.f;
		if (elps > 100.f)
			MsgCB("# #PERF_WARN: bump create time = %.1f ms", elps);
		
		m_texture			= T_normal_2C;
		return				true;
	}
}

bool			CTextureContext::copy_rect(PDIRECT3DTEXTURE9 pTextureFrom, LPRECT r_source, LPRECT r_dest, DWORD filter)
{
	m_last_error = "dest texture not created!";
	if (!m_texture) return false;
	if (r_dest)
		SafeSetRect(&m_selection, *r_dest, DEBUG_INFO);
	// Copy surfaces & destroy temporary
	IDirect3DTexture9*  T_src = pTextureFrom;
	IDirect3DTexture9*  T_dst = m_texture;	

	int		L_src = T_src->GetLevelCount() - 1;
	int		L_dst = T_dst->GetLevelCount() - 1;
	while (L_src >= 0 && L_dst >= 0)
	{
		// Get surfaces
		IDirect3DSurface9		*S_src, *S_dst;
		R_CHK(T_src->GetSurfaceLevel(L_src, &S_src));
		R_CHK(T_dst->GetSurfaceLevel(L_dst, &S_dst)); // Copy
		HRESULT result = D3DXLoadSurfaceFromSurface (S_dst, NULL, r_dest, S_src, NULL, r_source, filter, 0); // 
		// Release surfaces
		_RELEASE(S_src);
		_RELEASE(S_dst);
		if (D3D_OK != result)
		{
			m_last_error.sprintf("D3DXLoadSurfaceFromSurface failed with error 0x%x, may be source in D3DPOOL_DEFAULT?", result);			
			return false;
		}
		L_src--; L_dst--;
	}

	m_last_error = "OK";
	return true;
}


bool			CTextureContext::stretch_rect(PDIRECT3DTEXTURE9 pTextureFrom, LPRECT r_source, LPRECT r_dest, D3DTEXTUREFILTERTYPE filter)
{
	m_last_error = "dest texture not created!";
	if (!m_texture) return false;
	if (r_dest)
		SafeSetRect(&m_selection, *r_dest, DEBUG_INFO);

	int		L_src			= pTextureFrom->GetLevelCount	() - 1;
	int		L_dst			= m_texture->GetLevelCount		() - 1;
	while (L_src >= 0 && L_dst >= 0)
	{
		// Get surfaces
		IDirect3DSurface9		*S_src, *S_dst;
		R_CHK(pTextureFrom->GetSurfaceLevel(L_src, &S_src));
		R_CHK(   m_texture->GetSurfaceLevel(L_dst, &S_dst)); // Copy
		HRESULT result = g_pDevice->StretchRect(S_src, r_source, S_dst, r_dest, filter);
		_RELEASE(S_src);
		_RELEASE(S_dst);
		if (D3D_OK != result)
		{
			m_last_error.sprintf("StretchRect failed with error 0x%x, may be source not in D3DPOOL_DEFAULT?", result);			
			return false;
		}
		L_src--; 
		L_dst--;
	}
	m_last_error = "OK";
	return true;

}


UINT			CTextureContext::mem_usage() const
{
	if (m_mem_usage > 0)
		return m_mem_usage;

	if (m_texture && m_info >= 0)
	{
		UINT size = m_texture->GetLevelCount() * m_desc.Width * m_desc.Height * 4;
		if (packed		== m_desc.Format) size /= 4;
		if (D3DFMT_DXT1 == m_desc.Format) size /= 8;
		return size;
	}
	return 0;
}


void			CRectIntersection::calc_intersection(UINT Level, CTextureContext *src, CTextureContext *dst)
{

	auto *src_desc = src->load_desc(Level);
	auto *dst_desc = dst->load_desc(Level);
	if (NULL == src_desc || NULL == dst_desc) return; 

	RECT &rsrc = src->m_selection;
	RECT &rdst = dst->m_selection;



	if (SafeSetRect(&m_target, rdst,  DEBUG_INFO))
	{		
		m_target.left = __max(0, rdst.left);
		m_target.top  = __max(0, rdst.top);
		clamp<LONG>(m_target.left,   0, dst_desc->Width);
		clamp<LONG>(m_target.top,    0, dst_desc->Height);
		clamp<LONG>(m_target.right,  0, dst_desc->Width);
		clamp<LONG>(m_target.bottom, 0, dst_desc->Height);	
	}
	else	
		dst->m_error_count++;	

	if (SafeSetRect(&m_source, rsrc, DEBUG_INFO))
	{
		clamp<LONG>(m_source.left,   0, src_desc->Width);
		clamp<LONG>(m_source.top,    0, src_desc->Height);
		clamp<LONG>(m_source.right,  0, src_desc->Width);
		clamp<LONG>(m_source.bottom, 0, src_desc->Height);	

	}
	else	
		src->m_error_count++;

	// проверка отсечения части оверлея, если он ушел за левый и/или верхний край
	if (rdst.left < 0) m_source.left = __min(m_source.right,  -rdst.left);
	if (rdst.top < 0)  m_source.top  = __min(m_source.bottom, -rdst.top);


	// первичная оценка доступных размерностей
	m_avail_width  = __min(m_source.right  - m_source.left, m_target.right  - m_target.left);
	m_avail_height = __min(m_source.bottom - m_source.top,  m_target.bottom - m_target.top);		
}

XRTEXTURES_API void  SetActiveDevice(LPDIRECT3DDEVICE9 pDevice)
{
	if (g_pDevice == pDevice) return;
	if (g_pDevice) _RELEASE(g_pDevice);

	g_pDevice = pDevice;
	g_pDevice->AddRef();
}
XRTEXTURES_API void  ReleaseActiveDevice()
{
	if (NULL == g_pDevice) return;

	if (g_dp_textures.size()) 
	{
		MsgCB("! #LEAK_WARN: DEFAULT_POOL textures not unloaded:");

		xr_vector<PDIRECT3DTEXTURE9> ulist;
		for (auto it = g_dp_textures.begin(); it != g_dp_textures.end(); it++)
		{
			MsgCB("*   0x%p = %s", it->first, *it->second);
			ulist.push_back( (PDIRECT3DTEXTURE9)it->first );
		}
		for (size_t n = 0; n < ulist.size(); n++) 
		{
			PDIRECT3DTEXTURE9 t = ulist[n];
			while (t)
				TexRelease(t, FALSE);
		}
	}
	_RELEASE(g_pDevice);
}

CTextureContext*  texture_convert(CTextureContext *source, D3DFORMAT fmt = D3DFMT_DXT5, bool release_source = false, bool allow_bypass = true)
{     
   if (fmt == source->load_desc(0)->Format && source->pool () == D3DPOOL_MANAGED && allow_bypass)
	   return source;
   CTextureContext* result = xr_new<CTextureContext>();
   result->load_texture(source->get_texture(), fmt);	      
   SafeSetRect(&result->m_selection, source->m_selection, DEBUG_INFO);		 // copy selection
   if (release_source)
	   xr_delete(source);
   return result;
}

IC CTextureContext*	get_add_result(PTEXTURE_COMBINE_CONTEXT pCtx, UINT index)
{
	if (index < MAX_TEXTURES_COMBINE)
	{ 
		if (NULL == pCtx->results[index])			
			pCtx->results[index] = xr_new<CTextureContext>();
		return pCtx->results[index];
	}
	return NULL;
}



bool texture_alpha_blend(CTimer &perf, PTEXTURE_COMBINE_CONTEXT pContext)
{
	CTextureContext *T_u_result = get_add_result(pContext, 0);

	static int ops = 0;

	if (alpha_blend(0, 0, u32(-1)) != u32(-1))
	{
		T_u_result->m_last_error = "alpha_blend test calculation failed ";
		Msg("!#ERROR: %s", *T_u_result->m_last_error);
		return NULL; // проверка, должно возвращаться -1
	}
	
	
	auto *T_back	= pContext->source[0]; // исходник, возможно результат
	auto *T_overlay = pContext->source[1]; // исходник, нельзя повреждать!
	if (T_back && T_overlay && T_back->get_texture() && T_overlay->get_texture() && T_back->load_desc(0) && T_overlay->load_desc(0))
	{
		// ALL OK!
	}
	else
	{
		T_u_result->m_last_error = "texture_alpha_blend must be set 2 source loaded textures!";
		T_u_result->m_error_count ++;
		Msg("!#ERROR: %s", *T_u_result->m_last_error);
		return false;
	}


	const Irect &dst_rect = pContext->i_coords[0];  // область назначения в фоновой текстуре
	const Irect &src_rect = pContext->i_coords[1];  // выбор области в оверлее
			

	if (!SafeSetRect(&T_overlay->m_selection, src_rect, DEBUG_INFO))
	{		
		T_u_result->m_last_error = "invalid source rect";
		T_u_result->m_error_count++;
		return false;
	}
	// dst_rect.right= T_back->m_desc.Width;
	// dst_rect.bottom = T_back->m_desc.Height;
	if (!SafeSetRect(&T_back->m_selection, dst_rect, DEBUG_INFO))
	{		
		T_u_result->m_last_error = "invalid target rect";
		T_u_result->m_error_count++;
		return false;
	}

	LPCSTR op = *pContext->operation;
	CRectIntersection intersection;
	intersection.calc_intersection(0, T_overlay, T_back); 

	u32 mips = 1; // __min(T_back->GetLevelCount(), T_overlay->GetLevelCount());	
		
	D3DSURFACE_DESC	&desc = T_back->m_desc;	
	D3DSURFACE_DESC	&o_desc = T_overlay->m_desc;
	// клон - все данные включены в результат, должна вернуться копия - не оригинал!
	// полное копирование нужно, поскольку накладывание предполагается лишь региона (вероятно малой текстуры)
	// использовать фоновую текстуру можно, если она распакована и является результирующей одновременно	
	if (T_back != T_u_result || desc.Format != unpacked)
	{
		T_u_result->load_texture(T_back->get_texture(), unpacked);
		RECT &sr = T_u_result->m_selection;

		if (sr.right  != desc.Width ||
			sr.bottom != desc.Height)
			MsgCB("! #WARN(texture_alpha_blend): loaded texture selection rb = %d x %d, T_back size = %d x %d ",
			  sr.right, sr.bottom, desc.Width, desc.Height);

	}

	float elps0 = perf.GetElapsed_sec() * 1000.f;
	CTextureContext* T_u_overlay = texture_convert(T_overlay, D3DFMT_A8R8G8B8);
	float	elps1 = perf.GetElapsed_sec() * 1000.f;
	if (elps1 > 10.f)
		MsgCB("# #PERF_WARN: texture_blend prepare time = %.1f, %.1f ms", elps0, elps1);

	bool result = true;

	if (intersection.m_avail_width < 8 || intersection.m_avail_height < 1)
	{
		Msg("! #WARN(texture_alpha_blend): rect intersection size small = %d x %d ", intersection.m_avail_width, intersection.m_avail_height);
		Msg(" t_background level 0 size = %d x %d ", desc.Width, desc.Height);
		Msg(" t_overlay    level 0 size = %d x %d ", o_desc.Width, o_desc.Height);
		DumpIntRect("* #DBG: source rect = ", intersection.m_source);		
		DumpIntRect("* #DBG: target rect = ", intersection.m_target);
		DumpIntRect("* #DBG: i_coords[0] = ", pContext->i_coords[0]);
		DumpIntRect("* #DBG: i_coords[1] = ", pContext->i_coords[1]);

		T_overlay->m_error_count++;
		T_u_result->m_error_count++;
		T_u_result->m_last_error = "invalid source/target rect";
		result = false;
	}

	

	perf.Start();
	if (result && intersection.m_avail_width > 0 && intersection.m_avail_height > 0) 
	__try
	{

		if (strstr(op, ":mask"))
			TW_Iterate_2OP(0, *T_u_result, *T_u_result, *T_u_overlay, &intersection, alpha_blend_mask);
		else
			TW_Iterate_2OP(0, *T_u_result, *T_u_result, *T_u_overlay, &intersection, alpha_blend);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		T_u_result->m_last_error = "#EXCEPTION: catched in texture_alpha_blend";
		Msg("!%s", *T_u_result->m_last_error);
		RECT &rs = intersection.m_source;
		MsgCB("* #DBG: source rect =  %4d, %4d, %4d, %4d ", rs.left, rs.top, rs.right, rs.bottom);
		RECT &rt = intersection.m_target;
		MsgCB("* #DBG: target rect =  %4d, %4d, %4d, %4d ", rt.left, rt.top, rt.right, rt.bottom);
		result = false;
	}
	
	elps1 = perf.GetElapsed_sec() * 1000.f;
	if (elps1 > 10.f)
		MsgCB("# #PERF_WARN: texture_blend process time = %.1f ms", elps1);

	// _RELEASE(T_back);
	if (T_u_overlay != T_overlay) // TODO: здесь может быть утечка!
	{		
		T_u_overlay->set_owner();
		xr_delete(T_u_overlay);
	}
	

	if (!strstr(op, ":unpacked")) // TODO: здесь может быть утечка!
	{
		T_u_result->set_owner();
		// PDIRECT3DTEXTURE9 texture = 
		result = T_u_result->load_texture(NULL, packed);  // Compress				
	}
	if (result)
		T_u_result->m_last_error = "OK";
	return result;
}

XRTEXTURES_API BOOL	TextureCombine (PTEXTURE_COMBINE_CONTEXT pContext)
{
	CTimer perf;
	perf.Start();
	BOOL result = FALSE;
		

	if (!pContext->operation) return FALSE;

	if (strstr(*pContext->operation, "alpha:"))
	{
		bool success = texture_alpha_blend(perf, pContext); //  back, over, , (use_sel ? &src_rect : NULL) );
		CTextureContext *ctx = pContext->results[0];
		if (success && ctx)
		{			
			result = TRUE;				
			ctx->m_last_error = "OK";
			pContext->mem_usage[0] = ctx->mem_usage(); 			
		}

	} else
	if (strstr(*pContext->operation, "unpack:"))
	{	
		CTextureContext *ctx = get_add_result(pContext, 0);
		R_ASSERT(ctx);
		ctx->m_last_error = "source not specified";
		CTextureContext *src = pContext->source[0];
		if (!src) return FALSE;
		ctx->m_last_error = "source must be not locked";
		if (src->is_locked()) return FALSE;
		PDIRECT3DTEXTURE9 texture = src->get_texture();
		ctx->m_last_error = "source.texture not loaded";
		if (!texture) return FALSE;
		result = ctx->load_texture(texture, unpacked);
		if (result)
		{
			ctx->m_last_error = "OK";
			pContext->mem_usage[0] = ctx->mem_usage();
			result = TRUE;
		}		

	} else
	if (strstr(*pContext->operation, "alpha_replace:"))
	{
		CTextureContext &ctx = *pContext->results[0];
		UINT rep = (UINT) pContext->i_params[0];		
		auto *texture = ctx.get_texture();
		ctx.m_last_error = "source.texture not loaded";
		if (!texture) return FALSE;
		if (TW_Iterate_1OP(0, ctx, ctx, NULL, alpha_replace, rep))
		{
			ctx.m_last_error = "OK";
			result = TRUE;
		}
	}
	


	float	elps = perf.GetElapsed_sec() * 1000.f;
	if (elps > 40.f)
		MsgCB("# #PERF_WARN: texture_combine '%s' total time = %.1f ms", *pContext->operation, elps);

	return result;
}



XRTEXTURES_API void fix_texture_name(LPSTR fn)
{
	LPSTR _ext = strext(fn);
	if(  _ext					&&
	  (0==stricmp(_ext,".tga")	||
		0==stricmp(_ext,".dds")	||
		0==stricmp(_ext,".bmp")	||
		0==stricmp(_ext,".ogm")	) )
		*_ext = 0;
}


