#pragma once

#include <emmintrin.h>
#include "Texture.h"

#pragma warning(disable:4995)
#include <d3dx9.h>

#define NOT_SELECTED (UINT)-1
#define D3DFMT_DEFAULT  (D3DFORMAT)D3DX_DEFAULT
#define	D3DFMT_UNPACKED D3DFMT_A8R8G8B8
#define D3DFMT_PACKED   D3DFMT_DXT5

enum {	
	TXLOAD_MANAGED			= (1 << 0),  // загрузка в управляемый пул	
	TXLOAD_DEFAULT_POOL		= (1 << 1),	 // форсированная загрузка в дефолтный пул  
	TXLOAD_APPLY_LOD		= (1 << 2),  //	разрешение применять texture_lod
	TXLOAD_UNPACKED			= (1 << 3),	 // загрузка в формате A8R8G8B8
	TXLOAD_DEFAULT			= (1 << 31)	 // использовать предыдущие параметры, или заданные в конструкторе
};


// Класс контекста текстуры необходим для обработки её растровых данных одной или несколькими операциями
class  XRTEXTURES_API  CTextureContext
{
	friend class CTexture;

protected:
	UINT						m_locked;
	UINT						m_info;
	int							m_locks;
	bool						m_cropped;  // used m_selection
	bool						m_owner;    // отвечает за сохранность текстуры в памяти, т.к. увеличил счетчик ссылок
	PDIRECT3DTEXTURE9			m_texture;
	UINT						m_mem_usage;
	UINT						m_id;		// уникальный номер, который меняется при перезагрузках или обработке текстуры
	D3DPOOL						m_pool;
	xr_resource_named		   *m_container;
public:
	UINT						m_crc32;	// контрольная сумма для виртуальных текстур
	D3DLOCKED_RECT				m_rect;
	D3DSURFACE_DESC				m_desc;
	int							m_error_count;
	RECT						m_selection;
	shared_str					m_last_error;
	Flags32						m_load_flags;
	
	CTextureContext											();										
	virtual	~CTextureContext								();

	void							assign_texture			(PDIRECT3DTEXTURE9 pTexture, bool own = false);

	PDIRECT3DTEXTURE9				create_texture			(UINT Width, UINT Height, UINT Levels, D3DFORMAT Format, D3DPOOL Pool = D3DPOOL_MANAGED);	
	void							get_selection			(LPRECT pRect);
	PDIRECT3DTEXTURE9				get_texture				() const							{ return m_texture; }
	UINT							get_id					() const							{ return m_id; }
	bool							is_locked				() const							{ return (m_rect.pBits != NULL); }
	bool							load_texture			(PDIRECT3DTEXTURE9 pTextureFrom, D3DFORMAT&  DestFmt, int levels_2_skip = 0, SIZE *rect_size = NULL, int useDefaultPool = 1);
	bool							load_texture			(shared_str fRName, Flags32 load_flags);
	D3DSURFACE_DESC*				load_desc				(UINT Level);
	UINT							mem_usage				() const;
	D3DPOOL							pool					() const							{ return m_pool; }
	void							set_owner				(bool owner = true)					{ m_owner = owner;}	

public: 
	// rect and low-level accessing
	D3DLOCKED_RECT*					lock_rect				(UINT Level, LPRECT pRect = NULL, UINT Flags = 0);
	bool							copy_rect				(PDIRECT3DTEXTURE9 pTextureFrom, LPRECT r_source = NULL, LPRECT r_dest = NULL, DWORD filter = D3DX_FILTER_TRIANGLE);
	bool							stretch_rect			(PDIRECT3DTEXTURE9 pTextureFrom, LPRECT r_source = NULL, LPRECT r_dest = NULL, D3DTEXTUREFILTERTYPE filter = D3DTEXF_LINEAR);
	bool							unlock_rect				(bool Force = false);


};

class XRTEXTURES_API CRectIntersection
{
public:
	RECT					m_target;			// область блокировки в текстурах назначения
	RECT					m_source;			// область блокировки в текстурах наложения



	// размеры области пересечения
	UINT					m_avail_width;		
	UINT					m_avail_height;   
	CRectIntersection()
	{
		SetRect(&m_target, 0, 0, 0, 0);
		SetRect(&m_source, 0, 0, 0, 0);
		m_avail_width = m_avail_height = 0;
	}
	
	// обсчет наложения одной текстуры на другую, по заданному смещению
	void					calc_intersection(UINT Level, CTextureContext *src, CTextureContext *dst);
};

XRTEXTURES_API SIZE_T		AvailVideoMemory();
XRTEXTURES_API void			fix_texture_name(LPSTR fn);
XRTEXTURES_API void			SetActiveDevice(LPDIRECT3DDEVICE9 pDevice);
XRTEXTURES_API void			ReleaseActiveDevice();
XRTEXTURES_API BOOL			TextureCombine(PTEXTURE_COMBINE_CONTEXT pContext);


