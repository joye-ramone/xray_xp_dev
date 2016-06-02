#ifndef SH_TEXTURE_H
#define SH_TEXTURE_H
#pragma once

#include "../xrCore/xr_resource.h"
#include "../api_defines.h"
#include "../xrCDB/xrCDB.h"


class XRTEXTURES_API CTextureContext;

class ENGINE_API CAviPlayerCustom;
class ENGINE_API CTheoraSurface;

struct lua_State;



class XRTEXTURES_API CTexture			: public xr_resource_named	{
public:
	struct 
	{
		u32					bLoaded		: 1;
		u32					bUser		: 1;
		u32					seqCycles	: 1;
		u32					MemoryUsage	: 28;

	}									flags;
	fastdelegate::FastDelegate1<u32>	bind;

	// IDirect3DBaseTexture9*				pSurface;
	CAviPlayerCustom*					pAVI;
	CTheoraSurface*						pTheora;
	CTexture*							pOwner;			    // alpet: владелец текстуры, например скриптовая текстура
	float								m_material;
	shared_str							m_bumpmap;
	float								m_time_apply;	    // alpet: время последнего использования текстуры в рендеринге		
	int									m_count_apply;		// alpet: сколько раз применялось за последнюю загрузку		
	bool								m_skip_prefetch;	// alpet: пропускать отложенную загрузку
	bool								m_need_now;			// alpet: флаг немедленной загрузки (apply_load)
	float								m_load_delay;		// alpet: сколько загружалась текстура, для оценки "тяжести"
	xrCriticalSection					m_sync;				// alpet: для обработки многопоточного доступа	
	union{
		u32								m_play_time;		// sync theora time
		u32								seqMSPF;			// Sequence data milliseconds per frame
	};

	// Sequence data
	xr_vector<IDirect3DBaseTexture9*>	seqDATA;
	Irect								seqRECT;			// alpet: копирование кусочка текстуры для каждого кадра последовательности
	Ivector2							seqPOS;				// alpet: текущая позиция, откуда копируется фрейм
	u32									seqStepX;			// alpet: шаг сдвига фрейма вправо копирования текстуры, при смене кадра		
	u32									seqStepY;			// alpet: шаг сдвига фрейма вниз, когда будет достигнут правый край исходника
	u32									seqFrame;			// alpet: текущий кадр последовательности
	u32									seqIndex;			// alpet: текущая текстура последовательности



	// Description
	IDirect3DBaseTexture9*				desc_cache;
	D3DSURFACE_DESC						desc;
	shared_str							reg_name;			// alpet: имя регистрации текстуры в списке (для заменяемых текстур)
	PTEXTURE_COMBINE_CONTEXT			combine_context;	// alpet: для выполнения асинхронной операции смешивания в эту текстуру 
	CTextureContext						self_context;		// alpet: для оптимизации доступа к растру текстуры
	


	IC BOOL								desc_valid		()		{ return pSurface() == desc_cache; }
	IC void								desc_enshure	()		{ if (!desc_valid()) desc_update(); }
	void								desc_update		();

	CTextureContext						&get_context	();		
	lua_State*							lua				()  const  { return m_lua_state; }
	IDirect3DBaseTexture9*				pSurface		()   const;
protected:
	lua_State*							m_lua_state;

	BOOL								LoadAVI			(LPSTR fn);	
	BOOL								LoadLUA			(LPCSTR fn);
	BOOL								LoadOGM			(LPCSTR fn);
	BOOL								LoadSEQ			(LPCSTR fn);

	BOOL								LoadSurface		(LPCSTR fn, Flags32 load_flags);
	
public:

										CTexture		();
	virtual							   ~CTexture		();


	void	__stdcall					apply_load		(u32	stage);
	void	__stdcall					apply_theora	(u32	stage);
	void	__stdcall					apply_avi		(u32	stage);
	void	__stdcall					apply_seq		(u32	stage);
	void	__stdcall					apply_script	(u32	stage);
	void	__stdcall					apply_normal	(u32	stage);


	void								Combine			(TEXTURE_COMBINE_CONTEXT &src_context);
	void								Preload			();
	void								Load			();
	void								LoadImpl		( Flags32 load_flags = { TXLOAD_DEFAULT } );
	BOOL								LockRect		(BOOL bLock = TRUE, UINT Level = 0);
	void								PostLoad		();
	void								SetName			(LPCSTR _name);
	void								Unload			(void);
	
//	void								Apply			(u32 dwStage);

	void								surface_set		(IDirect3DBaseTexture9* surf);
	void								surface_upd		(IDirect3DBaseTexture9* surf);
	IDirect3DBaseTexture9*				surface_get 	();

	IC BOOL								isUser			()		{ return flags.bUser;					}
	IC u32								get_Width		()		{ desc_enshure(); return desc.Width;	}
	IC u32								get_Height		()		{ desc_enshure(); return desc.Height;	}

	void								video_Sync		(u32 _time){m_play_time=_time;}
	void								video_Play		(BOOL looped, u32 _time=0xFFFFFFFF);
	void								video_Pause		(BOOL state);
	void								video_Stop		();
	BOOL								video_IsPlaying	();

};
struct XRTEXTURES_API 	resptrcode_texture	: public resptr_base<CTexture>
{
	void				create			(LPCSTR	_name);
	void				destroy			()					{ _set(NULL);					}
	shared_str			bump_get		()					{ return _get()->m_bumpmap;		}
	bool				bump_exist		()					{ return 0!=bump_get().size();	}
};
typedef	resptr_core<CTexture,resptrcode_texture >	
	ref_texture;

XRTEXTURES_API	void lua_finalize();
#endif
