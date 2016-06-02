#include "StdAfx.h"
#include ".\r__occlusion.h"
#include "../lua_tools.h"

#pragma optimize("gyts", off)

int g_occq_disabled = 0;

ENGINE_API float g_fTimeInteractive;

volatile ULONG g_issued = 0;

R_occlusion::R_occlusion(void)
{
	enabled			= strstr(Core.Params,"-no_occq")?FALSE:TRUE;
}
R_occlusion::~R_occlusion(void)
{
	occq_destroy	();
}
void	R_occlusion::occq_create	(u32	limit	)
{
	pool.reserve	(limit);
	used.reserve	(limit);
	fids.reserve	(limit);
	for (u32 it=0; it < limit; it++)	{
		_Q	q;	q.order	= it;
		if	(FAILED(HW.pDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION,&q.Q)))	break;
		pool.push_back	(q);
	}
	std::reverse	(pool.begin(), pool.end());
	g_issued = 0;
}
void	R_occlusion::occq_destroy	(				)
{
	while	(!used.empty())	{
		_RELEASE(used.back().Q);
		used.pop_back	();
	}
	while	(!pool.empty())	{
		_RELEASE(pool.back().Q);
		pool.pop_back	();
	}
	used.clear	();
	pool.clear	();
	fids.clear	();
}

bool	R_occlusion::occq_disabled (   )
{
	if (g_occq_disabled > 0) {
		g_occq_disabled--;
		return true;
	}
	return (g_fTimeInteractive < 5.f || !enabled);
}

float	R_occlusion::occq_elapsed	(	)
{
	return elapsed;
}

u32		R_occlusion::occq_begin		(u32&	ID		)
{
	if (occq_disabled())		return 0;

	RImplementation.stats.o_queries	++;
	if (!fids.empty())	{
		ID				= fids.back();	 
		fids.pop_back();
		// fids.erase	(fids.begin()); // для ротации брать с начала
		VERIFY				( pool.size() );
		used[ID]			= pool.back	();
	} else {
		ID					= used.size	();
		VERIFY				( pool.size() );
		used.push_back		( pool.back() );
	}
	pool.pop_back			();
	CHK_DX		   (used[ID].Q->Issue	(D3DISSUE_BEGIN));	
	used[ID].frame = Device.dwFrame;
	// Msg				("begin: [%2d] - %d", used[ID].order, ID);
	return			used[ID].order;
}
void	R_occlusion::occq_end		(u32&	ID		)
{
	if (occq_disabled() || ID >= used.size())		return;

	// Msg				("end  : [%2d] - %d", used[ID].order, ID);	
	CHK_DX			(used[ID].Q->Issue	(D3DISSUE_END));
	used[ID].frame = Device.dwFrame;
	InterlockedIncrement (&g_issued);	
}

#ifdef	RENDER_WAIT_UTILIZE
  typedef void	( * RENDER_WAIT_CALLBACK)	(u32 wait_sleep);
  RENDER_WAIT_CALLBACK render_wait_CB = NULL;
  _declspec(dllexport) void SetRenderWaitCB(RENDER_WAIT_CALLBACK cb) {  render_wait_CB = cb; }; // для установки из внешней DLL обработчика холостых циклов ожидания
#endif


u32		R_occlusion::occq_get		(u32&	ID)
{
	elapsed = 0.f;

	if (occq_disabled()  || ID >= used.size() )	return OCCQ_BREAK; // всегда делать видимым

	DWORD	fragments	= 3;
	HRESULT hr;
	// CHK_DX		(used[ID].Q->GetData(&fragments,sizeof(fragments),D3DGETDATA_FLUSH));
	// Msg			("get  : [%2d] - %d => %d", used[ID].order, ID, fragments);	
		

	static float r_wait_med = 1.f;
	static float r_wait_sum = 0.f;
	static u32   r_wait_cnt = 0;
	static u32   debug_id   = 0;

	if (ID == debug_id)
		__asm nop;



	_Q &OQ = used[ID];	

	BOOL frame_switch = (OQ.frame != Device.dwFrame);	
	


	Device.Statistic->RenderDUMP_Wait.Begin	();
	T.Start	();	
	IDirect3DQuery9 *query = OQ.Q;
	bool wait_ok = true;

	if (NULL == query)
	{
		ID = (u32)-1;
		return OCCQ_BREAK;
	}
	

	while	( (hr = query->GetData (&fragments, sizeof(fragments), D3DGETDATA_FLUSH)) == S_FALSE ) {		
		
		if (frame_switch)	{
			MsgCB("! #WARN: occlusion query to late, was issued in frame %d, but now frame %d", OQ.frame, Device.dwFrame);
			if (!fragments) fragments = OCCQ_ERROR + 2;			
			break;
		}

#ifdef	RENDER_WAIT_UTILIZE
		float elps = T.GetElapsed_sec();
		float avail_ms = r_wait_med - (elps + r_wait_sum) * 1000.f;

		if (render_wait_CB)
			render_wait_CB( (u32)avail_ms );			
		if (g_render_lua && (avail_ms > 1.f))
		{				
			// alpet: данная функция в скриптах может утилизировать задержку 1-20ms, но не должна никак касаться графической части				
			// LuaExecute(g_render_lua, "render.wait", "fd", avail_ms, ps_r2_wait_sleep);
		}
			
#endif
		Device.Statistic->RenderDUMP_Wait.cycles++;				

		if (Device.frame_elapsed_sec() >= 1.1f && elps > 0.75f)	{			
			Msg("!#PERF_RENDER: occlusion query breaked due frame timeout reached, compsumption = %.3f sec. Frame = %d, query_id = %d", elps, Device.dwFrame, ID);			
			wait_ok = false;
			g_occq_disabled += 10000;
			break;
		}
		

		if (!SwitchToThread())
			SleepEx(ps_r2_wait_sleep, TRUE);
				
		if (frame_switch)
		{			
			fragments = OCCQ_BREAK;
			break;
		}
	}

	if (wait_ok)		
		InterlockedDecrement(&g_issued);	

	Device.Statistic->RenderDUMP_Wait.End	();
	elapsed		= T.GetElapsed_sec();
	if (elapsed >= 0.25f && !frame_switch)
	{
		MsgCB("!#PERF_RENDER: wait occlusion query time = %.3f sec, id = %d, quieries issued = %d, interactive = %.1f s ", elapsed, ID, g_issued, g_fTimeInteractive);
	}

	static u32	last_frame = 0;
	u32 frame_inc = (last_frame != Device.dwFrame);
	last_frame = Device.dwFrame;

	r_wait_sum += elapsed;
	r_wait_cnt += frame_inc;
	if (r_wait_cnt >= 1000)
	{		
		r_wait_med = r_wait_sum;
		MsgCB("##PERF_RENDER: median dump wait = %.1f ms", r_wait_med);
		r_wait_cnt = 0;
		r_wait_sum = 0.f;
	}
	
	if	(hr == D3DERR_DEVICELOST)	
		 fragments = OCCQ_BREAK;
	else
	  if (!wait_ok) 
		 fragments = OCCQ_ERROR + 0xE;

	if (0 == fragments)	RImplementation.stats.o_culled	++;

	// insert into pool (sorting in decreasing order)
	_Q&		Q			= used[ID];
	if (pool.empty())	
		pool.push_back(Q);
	else	{
		// вставка таким образом, чтобы порядок не нарушался
		int		it		= int( pool.size() ) - 1;
		while	( (it>=0) && (pool[it].order < Q.order) )	it--;
		pool.insert		(pool.begin() + it + 1 ,Q);
	}

	// remove from used and shrink as nescessary
	used[ID].Q			= 0;
	fids.push_back		(ID);
	ID					= (u32)-1;
	return	fragments;
}
