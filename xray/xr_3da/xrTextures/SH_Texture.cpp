// SHARED TEXTURE CLASS

#include "stdafx.h"
#include "../api_defines.h"
#include "../defines.h"
#include "../render_headers.h"
#include "../R_Backend.h"
#include "../lua_tools.h"
#include "../xrGame/pch_script.h"
#include "../common/doug_lea_memory_allocator.h"

#pragma hdrstop



#include "../ResourceManager.h"

#ifndef _EDITOR
    #include "../render.h"
#endif
    
#include "../tntQAVI.h"
#include "../xrTheora_Surface.h"
#include "../device.h"

#pragma comment(lib,"xr_3da.lib")
#pragma comment(lib,"lua5.1.lib")

#define		PRIORITY_HIGH	12
#define		PRIORITY_NORMAL	8
#define		PRIORITY_LOW	4

typedef int (WINAPI *OBJECT_METHOD) (PVOID pThis);

OBJECT_METHOD XRTEXTURES_API TextureLoadCapture = NULL;
OBJECT_METHOD XRTEXTURES_API TextureUnloadCapture = NULL;

CTimer apply_perf;

typedef void (*EXPORT_CLASSES) (lua_State *L);
typedef void (*SET_LVM_NAME)   (lua_State *L, LPCSTR szName, bool add_index);


EXPORT_CLASSES export_classes = NULL;
SET_LVM_NAME   set_lvm_name   = NULL;

extern LPDIRECT3DDEVICE9 g_pDevice;
extern ULONG TexRelease(PDIRECT3DTEXTURE9 &tex, BOOL bWarn = TRUE);


lua_State *g_tex_vm = NULL;

// #define USE_DL_ALLOCATOR

extern "C"
{
	void dlfatal(char *file, int line, char *function)
	{
		Debug.fatal(file, line, function, "Doug lea fatal error!");
	}
};



#ifndef USE_DL_ALLOCATOR
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
#else // USE_DL_ALLOCATOR

u32 game_lua_memory_usage	()
{
	return					((u32)dlmallinfo().uordblks);
}

static void *lua_alloc	(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  __try
  {
	  if (nsize == 0)	{ dlfree(ptr);	 return	NULL; }
	  else				return dlrealloc(ptr, nsize);
  } 
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
	  Msg("! #EXCEPTION: in lua_alloc, ud = 0x%p, ptr = 0x%p, osize = %d, nsize = %d ", ud, ptr, osize, nsize);
	  Msg("*  allocated %d ", game_lua_memory_usage());
  }
  return NULL;
}


#endif // USE_DL_ALLOCATOR



lua_State* create_lua_vm()
{
	lua_State* m_virtual_machine = NULL;
	__try 
	{
		m_virtual_machine = lua_newstate(lua_alloc, NULL);
			// luaL_newstate();
		luaL_openlibs(m_virtual_machine);
#ifdef LUAICP_COMPAT		
		#include "../xrGame/luaicp_attach.inc"
#endif
		luabind::open(m_virtual_machine);

		if (!export_classes)
		{
			HMODULE hDll = GetModuleHandle("xrGame.dll");
			if (hDll)
			{
				export_classes = (EXPORT_CLASSES)GetProcAddress(hDll, "?export_base_classes@@YAXPAUlua_State@@@Z");
				set_lvm_name   =   (SET_LVM_NAME)GetProcAddress(hDll, "?set_lvm_name@@YAXPAUlua_State@@PBD_N@Z");
			}
		}

		MsgCB("$#CONTEXT: exporing base classes into new lua VM");

		if (export_classes)
			export_classes(m_virtual_machine);
		else
			Msg("!#ERROR: not found function export_base_classes in xrGame.dll");
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!#EXCEPTION: create_lua_vm");
	}
	return m_virtual_machine;
}

void save_vm_thread(lua_State *L, lua_State *LT) 
{
	static string64 vm_name;
	sprintf_s(vm_name, 64, "lvm%p", (void*)LT);
	if (L)	lua_setglobal(L, vm_name);	
}

XRTEXTURES_API void lua_finalize()
{
	__try
	{
		if (Device.Resources)
		{
			u32 ucount = 0;
			auto &list = Device.Resources->textures();
			for (auto it = list.begin(); it != list.end(); it++)
			{
				auto *t = it->second;
				if (t && t->lua())
				{
					t->Unload();
					ucount++;
				}
			}
			if (ucount)
				Msg("# #PERF: unloaded %d script textures", ucount);
		}

		lua_State *L = g_tex_vm;
		g_tex_vm = NULL;
		if (L) lua_close(L);
	}
	__except (SIMPLE_FILTER) {
		Log("! #EXCEPTION: catched in lua_finalize()");
	}
}


void lua_zero_globals(lua_State *L, LPCSTR list)
{
	for (int i = 0; i < _GetItemCount(list); i++)
	{
		string256 temp;
		lua_pushnil(L);
		lua_setglobal(L, _GetItem(list, i, temp));
	}
}


ICF void rect2RECT(LPRECT pRect, const Irect &rect)
{
	pRect->left	  = rect.x1;
	pRect->top	  = rect.y1;
	pRect->right  = rect.x2;
	pRect->bottom = rect.y2;
}


void resptrcode_texture::create(LPCSTR _name)
{
	_set(Device.Resources->_CreateTexture(_name));
}

void lua_push_texture(lua_State *L, CTexture *T)
{
	using namespace luabind::detail;
	convert_to_lua<CTexture*>(L, T);  
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTexture::CTexture		()
{	
	pAVI				= NULL;
	pTheora				= NULL;
	pOwner				= NULL;
	desc_cache			= 0;
	seqMSPF				= 0;
	flags.MemoryUsage	= 0;
	flags.bLoaded		= false;
	flags.bUser			= false;
	flags.seqCycles		= FALSE;
	m_material			= 1.0f;
	m_skip_prefetch		= false;	
	bind				= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_load);
	m_count_apply		= 0;
	m_load_delay		= 0.f;
	m_time_apply		= Device.fTimeGlobal;
	m_need_now			= false;
	self_context.m_last_error = "just created";
	self_context.m_container = this;
	seqRECT.set			(0, 0, 0, 0);
	seqPOS.set			(0, 0);
	seqStepX			= 0;
	seqStepY			= 0;
	seqFrame			= 0;
	seqIndex			= 0;
	m_lua_state			= NULL;
}

#pragma optimize("gyts", off)

CTexture::~CTexture()
{
	m_time_apply = -1.f; // разрешить выгрузку "пользовательских" текстур
	LPCSTR szName = *cName;
	if (szName)
		MsgCB("$#CONTEXT: CTexture destructor called for %s ", szName);
	Unload();
	// release external reference
	Device.Resources->_DeleteTexture(this);	

}

void					CTexture::surface_set	(IDirect3DBaseTexture9* surf)
{	
	PDIRECT3DTEXTURE9 tex = PDIRECT3DTEXTURE9(surf);
	if (self_context.m_texture == tex && tex)
		tex->AddRef();
	else
		self_context.assign_texture(tex, true);
}

void					CTexture::surface_upd	(IDirect3DBaseTexture9* surf)
{	
	if (NULL == this)
	{
		Msg("!#ERROR: CTexture::surface_set this = NULL");
		return;
	}

	self_context.assign_texture(PDIRECT3DTEXTURE9(surf), false);
}


IDirect3DBaseTexture9*	CTexture::surface_get	()
{
	if (NULL == this)
	{
		Msg("!#ERROR: CTexture::surface_get this = NULL");
		return NULL;
	}

	if (pSurface())		
		pSurface()->AddRef	();
	return pSurface();
}

void CTexture::PostLoad	()
{
	desc_enshure();
	if (pTheora)				bind		= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_theora);
	else if (pAVI)				bind		= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_avi);
	else if (!seqDATA.empty())	bind		= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_seq);
	else if (m_lua_state)		bind		= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_script);
	else						bind		= fastdelegate::FastDelegate1<u32>(this,&CTexture::apply_normal);
}

void CTexture::SetName(LPCSTR _name)
{
	MsgV("5TEXTURES", "# texture name changing from %-30s to %s", cName.c_str(), _name);
	this->set_name(_name);
}


void CTexture::apply_load	(u32 dwStage)	{
	m_need_now = true;
	m_skip_prefetch = false;
	MsgCB("$#CONTEXT: apply_load texture %s", *cName);
	if (!flags.bLoaded)		Load			()	;
	else					PostLoad		()	;	
	bind					(dwStage)			;
};

void CTexture::apply_theora	(u32 dwStage)	{
	if (pTheora->Update(m_play_time!=0xFFFFFFFF?m_play_time:Device.dwTimeContinual)){
		R_ASSERT(D3DRTYPE_TEXTURE == pSurface()->GetType());
		IDirect3DTexture9*	T2D		= (IDirect3DTexture9*)pSurface();
		D3DLOCKED_RECT		R;
		RECT rect;
		rect.left			= 0;
		rect.top			= 0;
		rect.right			= pTheora->Width(true);
		rect.bottom			= pTheora->Height(true);

		u32 _w				= pTheora->Width(false);

		R_CHK				(T2D->LockRect(0,&R,&rect,0));
		R_ASSERT			(R.Pitch == int(pTheora->Width(false)*4));
		int _pos			= 0;
		pTheora->DecompressFrame((u32*)R.pBits, _w - rect.right, _pos);
		VERIFY				(u32(_pos) == rect.bottom*_w);
		R_CHK				(T2D->UnlockRect(0));
	}
	CHK_DX(g_pDevice->SetTexture(dwStage,pSurface()));
	m_time_apply = Device.fTimeGlobal;
	m_count_apply++;
};
void CTexture::apply_avi	(u32 dwStage)	{
	if (pAVI->NeedUpdate()){
		R_ASSERT(D3DRTYPE_TEXTURE == pSurface()->GetType());
		IDirect3DTexture9*	T2D		= self_context.get_texture();
		// AVI
		D3DLOCKED_RECT R;
		R_CHK	(T2D->LockRect(0,&R,NULL,0));
		R_ASSERT(R.Pitch == int(pAVI->m_dwWidth*4));
		//		R_ASSERT(pAVI->DecompressFrame((u32*)(R.pBits)));
		BYTE* ptr; pAVI->GetFrame(&ptr);
		CopyMemory(R.pBits,ptr,pAVI->m_dwWidth*pAVI->m_dwHeight*4);
		//		R_ASSERT(pAVI->GetFrame((BYTE*)(&R.pBits)));

		R_CHK	(T2D->UnlockRect(0));
	}
	CHK_DX(g_pDevice->SetTexture(dwStage,pSurface()));

	m_time_apply = Device.fTimeGlobal;
	m_count_apply++;
};
void CTexture::apply_seq	(u32 dwStage)	{
	// SEQ
	u32	frame		= Device.dwTimeContinual / seqMSPF; //Device.dwTimeGlobal
	u32	frame_data	= seqDATA.size();
	PDIRECT3DTEXTURE9 tex = NULL;

	if (0 == seqStepX) 	{
		if (flags.seqCycles) {
			seqIndex = frame % (frame_data * 2);
			if (seqIndex >= frame_data)	seqIndex = (frame_data - 1) - (seqIndex % frame_data);
		}
		else 
			seqIndex = frame % frame_data;		
	}

	// простая последовательность, переключение текстур из списка
	if (seqRECT.right == 0 || seqRECT.bottom == 0)  {
		self_context.set_owner(false);
		tex = (PDIRECT3DTEXTURE9)seqDATA[seqIndex];		
		CHK_DX(g_pDevice->SetTexture(dwStage, tex));
		self_context.assign_texture(tex);
	}
	else  {
		tex = (PDIRECT3DTEXTURE9)seqDATA[seqIndex];		 // TODO: several frames?
		R_ASSERT(tex);
		D3DSURFACE_DESC	desc;
		tex->GetLevelDesc(0, &desc);		
		// нужна распакованная ?
		if (desc.Format != D3DFMT_A8R8G8B8) {
			PDIRECT3DTEXTURE9 trash = tex;
			D3DFORMAT need_fmt = D3DFMT_A8R8G8B8;
			self_context.load_texture (trash, need_fmt, 0, NULL, 0);
			tex = self_context.get_texture();
			R_ASSERT(tex);
			self_context.set_owner(false);
			self_context.assign_texture(NULL);
			seqDATA[seqIndex] = tex;
			while (trash)
				TexRelease(trash, FALSE);			
			tex->GetLevelDesc(0, &desc);	
		}



		R_ASSERT(seqRECT.width() > 0 && seqRECT.height() > 0);
		if (NULL == pSurface())
			self_context.create_texture(seqRECT.width(), seqRECT.height(), tex->GetLevelCount(), D3DFMT_A8R8G8B8, D3DPOOL_MANAGED);

		RECT r_src;
		SetRect(&r_src, seqPOS.x, seqPOS.y, seqPOS.x + seqRECT.width(), seqPOS.y + seqRECT.height());
		self_context.copy_rect(tex, &r_src);
		CHK_DX(g_pDevice->SetTexture(dwStage, pSurface()));

		// TODO: frame order mistmatch probability!
		// если сдвиг не задан, текстура - всего лишь фрагмент
		while (seqFrame < frame) {   // alpet: простой проход слева направо, сверху вниз
			seqPOS.x += seqStepX;
			if (seqPOS.x + seqRECT.width() > desc.Width) {
				seqPOS.x = 0;
				seqPOS.y += seqStepY;
				if (seqPOS.y + seqRECT.height() > desc.Height) {
					seqPOS.y = 0;
					seqIndex = (seqIndex + 1) % frame_data;
				}
			} // if
			seqFrame++;
		} // while
	}


	m_time_apply = Device.fTimeGlobal;
	m_count_apply++;

};


LPCSTR module_name(lua_State *L)
{
	static string64 temp;
	sprintf_s(temp, "st%p", (PVOID)L);
	return temp;
}

bool activate_module(lua_State *L)
{
	bool result = false;
	LPCSTR vmname = module_name (L);
	lua_getglobal(L, "modules"); // list of environments
	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, vmname);
		if (lua_istable(L, -1))
		{
			if (0 == lua_setfenv(L, 1))
				result = true;
			else
				Log("! #FAIL: lua_setfenv unsuccessful. ");
		}
		else
			lua_pop(L, 1);
	}
	else
		Log("! #FAIL: script_textures not found/not table, so environment will be not set ");

	lua_pop(L,  1);
	return result;
}

void CTexture::apply_script(u32 dwStage)	{
	if (self_context.m_error_count) return; // до перезагрузки заблокирована

	m_sync.Enter();
	__try
	{
		lua_State *L = m_lua_state;
		int save_top = lua_gettop(L);
		activate_module(L);
		LPCSTR mname = module_name (L);


		/*
		int erridx	 = 0;
		lua_getglobal(L, "AtPanicHandler");

		if (lua_isfunction(L, -1))
			erridx = lua_gettop(L);
		else
			lua_pop(L, 1);
		*/
		lua_pushinteger (L, Device.dwTimeContinual);
		lua_setglobal   (L, "time_continual");		
		// lua_getglobal(L, "apply");
		// if (lua_isfunction(L, -1)) // LuaExecute не годиться, т.к. нужно объект пушить
		__try 
		{
			string64 qfn;
			sprintf_s (qfn, 64, "%s.apply", mname);
			PLUA_RESULT r = LuaExecute(L, qfn, "o", object_param(this));
			if (r->error)
			{
				self_context.m_last_error = r->c_str("(null)");
				self_context.m_error_count ++;
				Msg("! #ERROR: for script-texture '%s' LuaExecute failed with result %d '%s'", *cName, r->error, *self_context.m_last_error);
			}

			// lua_push_texture(L, this);
			// lua_pcall(L, 1, LUA_MULTRET, erridx);
			lua_settop(L, save_top);
			if (!self_context.m_error_count)
				CHK_DX(g_pDevice->SetTexture(dwStage, pSurface()));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Msg("! #EXCEPTION: catched in CTexture::apply_script");
		}
		
		m_time_apply = Device.fTimeGlobal;
		m_count_apply++;
		if (self_context.m_error_count > 4)
		{
			Msg("! #ERROR: script texture errors = %d, last = %s, unloading", self_context.m_error_count, *self_context.m_last_error);
			Unload();
		}
	}
	__finally
	{
		m_sync.Leave();
	}

}
void CTexture::apply_normal	(u32 dwStage)	{
	apply_perf.Start();
	m_sync.Enter();
	u32 elps = apply_perf.GetElapsed_sec();
	if (elps > 5 && elps < 1e5f)
		MsgCB("##PERF_WARN: CTexture::apply_normal texture locking time =~C0D %.1f ms", apply_perf.GetElapsed_sec() * 1000.f);
	__try
	{
		CHK_DX(g_pDevice->SetTexture(dwStage, pSurface()));
		m_time_apply = Device.fTimeGlobal;
		m_count_apply++;
	}
	__finally
	{
		m_sync.Leave();
	}
};

CTextureContext&	 CTexture::get_context()
{	
	return self_context;
}

IDirect3DBaseTexture9*	CTexture::pSurface()   const
{
	if (NULL == this)
	{
		Msg("!#ERROR: CTexture::pSurface this = NULL");
		return NULL;
	}
	return self_context.get_texture();
}


void perform_combine (void *param)
{	
	CTexture &t =  *(CTexture*)param;
	TEXTURE_COMBINE_CONTEXT &context = *t.combine_context;
	BOOL result = FALSE;
	__try
	{
		// MsgCB("$#CONTEXT: perform_blend %s ", *context.operation);
		context.results[0] = &t.get_context();	
		if (t.self_context.m_error_count > 0)
		{
			MsgCB("* #DBG_SCRIPT_TEXTURE: m_error_count = %d", t.self_context.m_error_count);
			for (int i = 0; i < MAX_TEXTURES_COMBINE; i++)
			{
				Irect &r = context.i_coords[i];
				if (r.width() || r.height())
					MsgCB("#	i_coords[%d] = { %4d, %4d, %4d, %4d } ", i, r.x1, r.y1, r.x2, r.y2);
				Frect &f = context.f_coords[i];
				if (f.width() != 0.f || f.height() != 0.f)
					MsgCB("#	f_coords[%d] = { %4.2f, %4.2f, %4.2f, %4.2f } ", i, f.x1, f.y1, f.x2, f.y2);

			}
			t.self_context.m_error_count--;
		}


		t.m_sync.Enter();
		__try
		{
			result = TextureCombine(t.combine_context);
		}
		__finally
		{
			t.m_sync.Leave();
		}
	}
	__except (SIMPLE_FILTER)
	{
		result = FALSE;
	}
	t.flags.MemoryUsage = 0;		
	if (result)
	{		
		t.flags.bLoaded = (!t.self_context.m_error_count);
		t.flags.MemoryUsage = context.mem_usage[0]; 
		t.PostLoad	();
		t.m_time_apply = Device.fTimeGlobal;
		t.m_skip_prefetch = true; // порожденая текстура
	}	
	context.operation = NULL;
	xr_delete(t.combine_context);
}


void CTexture::Combine (TEXTURE_COMBINE_CONTEXT &src_context)
{
	combine_context = xr_new<TEXTURE_COMBINE_CONTEXT>();
	TEXTURE_COMBINE_CONTEXT &context = *combine_context;
	// ZeroMemory(combine_context, sizeof(TEXTURE_COMBINE_CONTEXT));
	context = src_context;
	context.operation = src_context.operation;
	/*
	for (int i = 0; i < MAX_TEXTURES_COMBINE; i++)
	{
		context.source[i]		= src_context.source[i];
		context.i_coords[i]		= src_context.i_coords[i];
		context.f_coords[i]		= src_context.f_coords[i];
		context.i_params[i]		= src_context.i_params[i];
		context.f_params[i]		= src_context.f_params[i];
	}
	*/
	
	if (strstr(*context.operation, ":async"))
		thread_spawn(perform_combine, "async_combine", 0, this);
	else
		perform_combine(this);
}


void CTexture::Preload	()
{
	// Material
/*
	if (Device.Resources->m_description->line_exist("specification",*cName))	{
//		if (strstr(*cName,"ston_stena"))	__asm int 3;
		LPCSTR		descr			=	Device.Resources->m_description->r_string("specification",*cName);
		string256	bmode;
		sscanf		(descr,"bump_mode[%[^]]], material[%f]",bmode,&m_material);
		if ((bmode[0]=='u')&&(bmode[1]=='s')&&(bmode[2]=='e')&&(bmode[3]==':'))
		{
			// bump-map specified
			m_bumpmap		=	bmode+4;
		}
//		Msg	("mid[%f] : %s",m_material,*cName);
	}
*/
	m_bumpmap = Device.Resources->m_textures_description.GetBumpName(cName);
	m_material = Device.Resources->m_textures_description.GetMaterial(cName);
}

void CTexture::Load()
{
	int result = 0;
	if (TextureLoadCapture)
		result = TextureLoadCapture ((PVOID)this);
		
	if (result <= 0)
		LoadImpl ();
}

BOOL CTexture::LoadOGM(LPCSTR fn) {
	// AVI
	pTheora		= xr_new<CTheoraSurface>();
	m_play_time	= 0xFFFFFFFF;

	if (!pTheora->Load(fn)) {
		xr_delete(pTheora);
		FATAL				("Can't open video stream");
	} else {
		flags.MemoryUsage	= pTheora->Width(true)*pTheora->Height(true)*4;
		pTheora->Play		(TRUE,Device.dwTimeContinual);

		// Now create texture
		IDirect3DTexture9*	pTexture = 0;
		u32 _w = pTheora->Width(false);
		u32 _h = pTheora->Height(false);

		HRESULT hrr = g_pDevice->CreateTexture(
			_w, _h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, NULL );
						
		if (FAILED(hrr))
		{
			FATAL		("Invalid video stream");
			R_CHK		(hrr);
			xr_delete	(pTheora);
			self_context.assign_texture(NULL);
		}
		else
		{
			self_context.assign_texture(pTexture);
			return TRUE;
		}
	}
	return FALSE;
} 

BOOL CTexture::LoadAVI(LPSTR fn)  {
	// AVI
	pAVI = xr_new<CAviPlayerCustom>();

	if (!pAVI->Load(fn)) {
		xr_delete(pAVI);
		FATAL				("Can't open video stream");
	} else {
		flags.MemoryUsage	= pAVI->m_dwWidth * pAVI->m_dwHeight*4;

		// Now create texture
		IDirect3DTexture9*	pTexture = 0;
		HRESULT hrr = g_pDevice->CreateTexture(
			pAVI->m_dwWidth, pAVI->m_dwHeight, 1, 0, D3DFMT_A8R8G8B8,D3DPOOL_MANAGED, &pTexture, NULL );
			
		if (FAILED(hrr))
		{
			FATAL		("Invalid video stream");
			R_CHK		(hrr);
			xr_delete	(pAVI);
			self_context.assign_texture(NULL);
		}
		else
		{
			self_context.assign_texture(pTexture, true);
			return TRUE;
		}
	}
	return  FALSE;
} 

BOOL CTexture::LoadLUA (LPCSTR fn) {
	// alpet: высокоуровневая обработка - скриптовая текстура! 
	IReader* _fs		= FS.r_open(fn);
	int size = _fs->length();
	R_ASSERT(size > 10);
	char *content = xr_alloc<char>(size + 1);
	_fs->r(content, size);
	content[size] = 0;		
	if (!g_tex_vm)
	{
		Log("##DBG: creating Lua VM for script textures.");
		g_tex_vm = create_lua_vm();
		lua_zero_globals(g_tex_vm, "alife,get_actor_obj,get_torch_obj,level,main_menu,object_by_id");
		lua_newtable(g_tex_vm);
		lua_setglobal(g_tex_vm, "modules");  
	}
	
	lua_State *L = lua_newthread(g_tex_vm);
	save_vm_thread (g_tex_vm, L);
	LPCSTR mname = module_name(L);
	int erridx = 0;
	int save_top = lua_gettop(L);
	lua_getglobal(L, "AtPanicHandler");
	if (lua_isfunction(L, -1))
		erridx = lua_gettop(L);
	else
		lua_pop(L, 1);
	if (set_lvm_name)
		set_lvm_name(L, mname, true);

	xr_string code;
	code = "local this = {} ";
	code += xr_sprintf("modules.%s = this ", mname);
	code += "setmetatable(this, {__index = _G}) ";
	code = (code + mname) + " = this "; // global namespace name
	code += "setfenv(1, this) ";	
	code += content;

	int err = luaL_loadbuffer(L, code.c_str(), code.size(), fn); // simple do-file 
	if (0 == err && lua_isfunction(L, -1))
	{		
		lua_pcall(L, 0, LUA_MULTRET, erridx);
		m_lua_state = L;
		lua_settop(L, save_top);
	}
	else
	{
		if (lua_isstring(L, -1))
			Msg("!#ERROR: loadbuffer for %s returned %d:\n*\t %s", fn, err, lua_tostring(L, -1));
		else
			Msg("!#ERROR: loadbuffer for %s returned %d", fn, err);

		lua_pushnil(g_tex_vm);
		save_vm_thread (g_tex_vm, L);
	}

	xr_free(content);
	FS.r_close(_fs);	
	return !!m_lua_state;
} 
BOOL CTexture::LoadSEQ (LPCSTR fn) {
	// Sequence
	string256 buffer;
	IReader* _fs		= FS.r_open(fn);

	flags.seqCycles	= FALSE;
	_fs->r_string	(buffer,sizeof(buffer));
	if (0==stricmp	(buffer,"cycled"))
	{
		flags.seqCycles	= TRUE;
		_fs->r_string	(buffer,sizeof(buffer));
	}

	int vals = _GetItemCount(buffer);
	string32	temp;
	float params[16] = { 0.f };
	for (int i = 0; i < __min(16, vals); i++)
		params[i] = (float)atof(_GetItem(buffer, i, temp));

	Flags32 load_flags = self_context.m_load_flags;


	float fps = params[0]; 
	seqMSPF	= iFloor(1000.0f / fps);
	seqIndex  = 0;
	seqPOS.set(0, 0);
	if (vals >= 5)
	{
		seqRECT.lt.set(0, 0);
		seqRECT.set_width  ((int)params[1]);
		seqRECT.set_height ((int)params[2]);
		seqStepX		  = (int)params[3];
		seqStepY		  = (int)params[4];			
		load_flags.set(TXLOAD_UNPACKED, TRUE);
	}

	while (!_fs->eof())
	{
		_fs->r_string(buffer,sizeof(buffer));
		_Trim		(buffer);
		if (buffer[0])	
		{
			// Load another texture				
			get_context().load_texture (buffer, load_flags); // всякий раз становиться владельцем
			get_context().set_owner(false);
			if (pSurface())	
			{					
				// pSurface->SetPriority	(PRIORITY_LOW);
				seqDATA.push_back		(pSurface());
				flags.MemoryUsage		+= self_context.mem_usage();
			}
		}
	}
	get_context().assign_texture(NULL);
	FS.r_close	(_fs);

	return seqDATA.size();
} 
BOOL CTexture::LoadSurface (LPCSTR fn, Flags32 load_flags) { // or TGA
	// Normal texture
	u32	mem  = 0;
	get_context().load_texture(fn, load_flags);
	// Calc memory usage and preload into vid-mem
	if (pSurface()) {
		// pSurface->SetPriority	(PRIORITY_NORMAL);
		flags.MemoryUsage		+=	self_context.mem_usage();
		return TRUE;
	}
	return FALSE;
}

void CTexture::LoadImpl		(Flags32 load_flags)
{
	flags.bLoaded					= true;
	desc_cache						= 0;	
	if (load_flags.test(TXLOAD_DEFAULT))
		load_flags = self_context.m_load_flags;
	else
		self_context.m_load_flags = load_flags;
		
	CTimer	perf;
	perf.Start();
	LPCSTR szName = *cName;
	if (!szName)
	{
		Msg("!#ERROR: texture name = NULL for CTexture %p ", (void*)this);
		return;
	}
	if (strstr(szName, "~vmt\\")) return;  // virtual textures cannot be loaded
	flags.bUser						= false;
	flags.MemoryUsage				= 0;
	if ( 0== stricmp(szName, "$null") )	return;
	if ( strstr(szName,  "$user$") )	{
		flags.bUser	= true;
		return;
	}

	auto surf = pSurface();
	if (surf)
	{
		self_context.load_desc(0);
		// если пулы совпадают, пропустить перезагрузку
		if ( (load_flags.test(TXLOAD_DEFAULT_POOL) && D3DPOOL_DEFAULT == self_context.m_desc.Pool) ||
			 (load_flags.test(TXLOAD_MANAGED)	   && D3DPOOL_MANAGED == self_context.m_desc.Pool) ) return;
		Unload();
	}
	
	static volatile ULONG load_call = 0;
	ULONG last = InterlockedIncrement(&load_call);
	if (last % 500 == 0)
		MsgCB ("##PERF: loading %d texture... ", last);

	if (!m_need_now)
		MsgCB("$#CONTEXT: loading non-urgent texture %s", szName);

	// MsgV ("7LOAD_TEX", " loading texture %s ", szName);
	//  MsgCB("$#CONTEXT: loading texture %s ", szName);
	Preload							();
#ifndef		DEDICATED_SERVER
	// Check for OGM
	string_path			fn;
	string_path			name;
	strcpy_s(name, sizeof(name), *cName);
	// alpet: для унификации имен клонов ресурсов
	char *sub = strstr(name, "|");
	if (sub)
		sub[0] = 0;

	if (FS.exist(fn, "$game_textures$", name, ".ogm")) LoadOGM(fn);	else
		if (FS.exist(fn, "$game_textures$", name, ".ogm")) LoadAVI(fn); else
			if (FS.exist(fn, "$game_textures$", name, ".lua")) LoadLUA(fn); else
				if (FS.exist(fn, "$game_textures$", name, ".seq")) LoadSEQ(fn); else
					LoadSurface(name, load_flags); // искать в разных папках, с разными расширениями и т.п.

#endif
	PostLoad	();
	if (flags.bLoaded)
		m_time_apply = Device.fTimeGlobal;

	float elps = perf.GetElapsed_sec() * 1000.f;
	if (elps > m_load_delay)
		m_load_delay = elps;

	if (elps >= 500.f)
		MsgCB("##PERF_WARN: Texture load time = %7.1f ms, size = %5dx%-5d ( %5.1f MiB ), name = '%s' ", 
					elps, get_Width(), get_Height(), float(flags.MemoryUsage) / 1048576.f, szName);

}

void CTexture::Unload	()
{
#ifdef DEBUG
	string_path				msg_buff;
	sprintf_s				(msg_buff,sizeof(msg_buff),"* Unloading texture [%s] pSurface RefCount=",cName.c_str());
#endif // DEBUG
	if (strstr(*this->cName, "$user$") && m_time_apply >= 0.f)
	{
		MsgCB("!#WARN: ignored unload texture %s - this allowed only while destruction!", *this->cName);
		return;
	}


	if (TextureUnloadCapture)
		TextureUnloadCapture((PVOID)this);


//.	if (flags.bLoaded)		Msg		("* Unloaded: %s",cName.c_str());
	m_need_now				= false;
	flags.bLoaded			= FALSE;
	if (!seqDATA.empty())	{
		for (u32 I = 0; I < seqDATA.size(); I++)
		{
			PDIRECT3DTEXTURE9 tex = (PDIRECT3DTEXTURE9) seqDATA[I];
			while (tex)	TexRelease(tex, FALSE);
		}
		seqDATA.clear();
	}

	if (m_lua_state && g_tex_vm)
	__try {		
		lua_State *L = m_lua_state;
		m_lua_state = NULL;
		LuaExecute(L, "unload", "o", object_param(this));
		lua_pushnil (g_tex_vm);
		save_vm_thread (g_tex_vm, L);		
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		Msg("!#EXCEPTION: while releasing lua VM thread in CTexture::Unload ");
	}



#ifdef DEBUG
	_SHOW_REF		(msg_buff, pSurface);
#endif // DEBUG
	self_context.assign_texture(NULL);

	xr_delete		(pAVI);
	xr_delete		(pTheora);

	self_context.m_error_count = 0;
	flags.MemoryUsage = 0;

	bind			= fastdelegate::FastDelegate1<u32>(this,&CTexture::
		apply_load);
}

BOOL CTexture::LockRect(BOOL bLock, UINT Level)
{
  	get_context();
	if (!pSurface()) return FALSE;

	BOOL result;

	if (bLock)
		result = (NULL != self_context.lock_rect(Level));
	else
	{
		self_context.unlock_rect();
		result = (0 == self_context.m_rect.Pitch);
	}
		
	Msg("##DBG: CTexture::LockRect for~C0A %s~C0B, bLock =~C0D %d~C0B, success =~C0D %d~C07", *cName, bLock, result);	
	return TRUE;
}

void CTexture::desc_update	()
{
	desc_cache	= pSurface();
	if (pSurface() && (D3DRTYPE_TEXTURE == pSurface()->GetType()))
	{
		IDirect3DTexture9*	T	= (IDirect3DTexture9*)pSurface();
		R_CHK					(T->GetLevelDesc(0,&desc));
	}
}

void CTexture::video_Play		(BOOL looped, u32 _time)	
{ 
	if (pTheora) pTheora->Play	(looped,(_time!=0xFFFFFFFF)?(m_play_time=_time):Device.dwTimeContinual); 
}

void CTexture::video_Pause		(BOOL state)
{
	if (pTheora) pTheora->Pause	(state); 
}

void CTexture::video_Stop			()				
{ 
	if (pTheora) pTheora->Stop(); 
}

BOOL CTexture::video_IsPlaying	()				
{ 
	return (pTheora)?pTheora->IsPlaying():FALSE; 
}
