#include "stdafx.h"
#include "frustum.h"

#pragma warning(disable:4995)
// mmsystem.h
#define MMNOSOUND
#define MMNOMIDI
#define MMNOAUX
#define MMNOMIXER
#define MMNOJOY
#include <mmsystem.h>
// d3dx9.h
#include <d3dx9.h>
#pragma warning(default:4995)

#include "x_ray.h"
#include "render.h"
#include <DbgHelp.h>
#include "lua_tools.h"
#include "../../build_config_defines.h"

ENGINE_API CRenderDevice Device;
ENGINE_API BOOL g_bRendering = FALSE; 

extern ENGINE_API BOOL g_appLoaded;
	   ENGINE_API BOOL g_appActive = FALSE;
extern ENGINE_API bool g_bGameInteractive;
ENGINE_API		 float g_fTimeInteractive = 0;

BOOL		g_bLoaded = FALSE;


int			g_iTargetFPS    = 100;
ref_light	precache_light	= 0;
bool		warn_flag		= false;

#pragma optimize("gyts", off)

class CTimings
{
	
protected:
			int			m_lines [256];
			float		m_elapsed [256];
volatile	ULONG		m_count;
public:

	IC		void   AddPoint(int line)
			{
				if (g_fTimeInteractive < 5.f) return;
				ULONG index = InterlockedIncrement(&m_count) - 1;
				index &= 0xFF;
				m_lines[index] = line;
				m_elapsed[index] = Device.frame_elapsed_sec();
			}

			ULONG Count()  const 
			{
				return m_count;
			}

			void  Dump()
			{
				string1024 ltxt = { 0 };
				string1024 etxt = { 0 };

				for (ULONG i = 0; i < __min(256, m_count); i++)
				{
					sprintf_s(ltxt, 1024, "%s %8d",   ltxt, m_lines[i]);
					sprintf_s(etxt, 1024, "%s %8.1f", etxt, m_elapsed[i] * 1000.f);
				}
				MsgCB("#   %s ", ltxt);
				MsgCB("#   %s ", etxt);
			}

	IC		void  Reset() { InterlockedExchange(&m_count, 0); }


};


BOOL CRenderDevice::Begin	()
{
#ifndef DEDICATED_SERVER
	HW.Validate		();
	HRESULT	_hr		= HW.pDevice->TestCooperativeLevel();
    if (FAILED(_hr))
	{
		// If the device was lost, do not render until we get it back
		if		(D3DERR_DEVICELOST==_hr)		{
			Sleep	(33);
			return	FALSE;
		}

		// Check if the device is ready to be reset
		if		(D3DERR_DEVICENOTRESET==_hr)
		{
			Reset	();
		}
	}

	CHK_DX					(HW.pDevice->BeginScene());
	RCache.OnFrameBegin		();
	RCache.set_CullMode		(CULL_CW);
	RCache.set_CullMode		(CULL_CCW);
	if (HW.Caps.SceneMode)	overdrawBegin	();
	FPU::m24r	();
	g_bRendering = 	TRUE;
#endif
	Device.Statistic->fTargetMedian = 0.02f;

	return		TRUE;
}

void CRenderDevice::Clear	()
{
	CHK_DX(HW.pDevice->Clear(0,0,
		D3DCLEAR_ZBUFFER|
		(psDeviceFlags.test(rsClearBB)?D3DCLEAR_TARGET:0)|
		(HW.Caps.bStencil?D3DCLEAR_STENCIL:0),
		D3DCOLOR_XRGB(0,0,0),1,0
		));
}

extern void CheckPrivilegySlowdown();
#include "resourcemanager.h"

XRCORE_API BOOL GetFunctionInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp);

void CRenderDevice::End		(void)
{
#ifndef DEDICATED_SERVER

	VERIFY	(HW.pDevice);

	if (HW.Caps.SceneMode)	overdrawEnd		();

	// 
	if (dwPrecacheFrame)
	{
		::Sound->set_master_volume	(0.f);
		dwPrecacheFrame	--;
		pApp->load_draw_internal	();
		if (0==dwPrecacheFrame)
		{
			Gamma.Update		();			

			if(precache_light) precache_light->set_active	(false);
			if(precache_light) precache_light.destroy		();
			::Sound->set_master_volume						(1.f);
			pApp->destroy_loading_shaders					();
			Resources->DestroyNecessaryTextures				();
			Memory.mem_compact								();
			Msg												("* MEMORY USAGE: %d K",Memory.mem_usage()/1024);
			CheckPrivilegySlowdown							();
		}
	}

	g_bRendering		= FALSE;
	// end scene
	RCache.OnFrameEnd	();
	Memory.dbg_check		();
    CHK_DX				(HW.pDevice->EndScene());

	HRESULT _hr		= HW.pDevice->Present( NULL, NULL, NULL, NULL );
	if				(D3DERR_DEVICELOST==_hr)	return;			// we will handle this later
	//R_ASSERT2		(SUCCEEDED(_hr),	"Presentation failed. Driver upgrade needed?");
#endif
}

volatile u32	mt_access = 0;
volatile u32	mt_Thread_marker		= 0x12345678;

CTimings		time_points;
u32				stack_check = 0x33333333;
CTimer			*mt_perf = NULL;

float	mt_elapsed(bool restart = true)
{
	float elps = 0.f;
	if (g_fTimeInteractive < 5.f) return 0.f;

	if (NULL == mt_perf)	
		mt_perf = xr_new<CTimer>();
	else
	{
		_control87(_PC_64, _MCW_PC);
		elps = mt_perf->GetElapsed_sec() * 1000.f;
		_control87(_PC_24, _MCW_PC);
	}
	float ms = (float) mt_perf->GetElapsed_ms();
	if (elps > ms)
		elps = ms;


	if (elps < -0.1f || elps > 3.6e6f)
	{
		MsgCB("!#STRANGE_ERROR: elapsed time = %.1f (%.0f) ms, timer object 0x%p was degradded.", elps, ms, (void*)mt_perf);				
		xr_delete(mt_perf);
		elps = 0.0f;
		mt_perf = xr_new<CTimer>();
		mt_perf->Start();
	}
	if (restart)
		mt_perf->Start();


	return elps;
}


void 			mt_Thread	(void *ptr)	{
	HANDLE ht = GetCurrentThread();	

#ifdef MT_OPT2
	
	HANDLE hp = GetCurrentProcess();
	DWORD af_mask = 0, af_sys_mask = 0;
	GetProcessAffinityMask(hp, &af_mask, &af_sys_mask);
	int cc = iFloor(log2((float)af_mask + 1) / 2);  // count of cores / 2
	af_mask <<= std::min (4, cc);   // на последние ядра
	SetThreadAffinityMask(ht, af_mask);
#endif

	// InitSymEng();
	
	HANDLE process = GetCurrentProcess();
	// Turn on line loading and deferred loading.
    // Force the invade process flag no matter what operating system
    // I'm on.
    HANDLE hPID = (HANDLE)GetCurrentProcessId ( ) ;

	SetThreadPriority(ht, THREAD_PRIORITY_ABOVE_NORMAL);
	SetThreadAffinityMask(ht, 0xFFFE);

	

	while (true) {
		// waiting for Device permission to execute
		
		Device.mt_csEnter.Enter	();
		

		if (Device.mt_bMustExit) {
			Device.mt_bMustExit = FALSE;				// Important!!!
			Device.mt_csEnter.Leave();					// Important!!!
			return;
		}
		// we has granted permission to execute
		mt_Thread_marker			= Device.dwFrame;
		static u32 pit = 0;
		mt_access					= GetCurrentThreadId();
		time_points.AddPoint(__LINE__);
		mt_elapsed();
		float elps = 0;		
		float elps_list[255];
		u32 size = Device.seqParallel.size();
		__try
		{						
			for (pit = 0; pit < size; pit++)
			{
				R_ASSERT(mt_Thread_marker == Device.dwFrame);
				string512 temp = { 0 };	 // alpet: stack protection				
				stack_check++;
				Device.seqParallel[pit]();
				stack_check--;
				if (pit < 128 && 0 == xr_strlen(temp))
				{
					elps_list[pit] = mt_elapsed();
					elps += elps_list[pit];
				}

			}
			if (Device.seqParallel.size() != size)
				Msg("!WARN: seqParallel.size() = %d, before execute = %d", Device.seqParallel.size(), size);

		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Msg("!Exception catched in secondary thread. pit = %d, mt_access = %d, current = %d", pit, mt_access, GetCurrentThreadId());
		}
				
		time_points.AddPoint(__LINE__);

		if (elps > 50.f && g_fTimeInteractive > 5.f)
		{
			static u32 r_size = Device.seqParallel.size();
			MsgCB("##PERF_WARN: mt_Thread cycle time = %.1f ms, seqParallel size = %d", elps, size);			
			// BSUSymInitialize((DWORD)hPID,  hPID, g_application_path, TRUE);
			if (r_size > 128) r_size = 128;			
			for (pit = 0; pit < r_size; pit++)
			{
				auto &m = Device.seqParallel[pit].GetMemento();
				void **dump = (void**)&m;
				float e = elps_list[pit];
				if (dump && e > 10)
				{
					DWORD disp = 0;					
					string4096 name;					
					SetLastError(0);
					if (GetFunctionInfo(dump[1], name, &disp))
						MsgCB("#	%.3d. this = 0x%08p, func = %s + 0x%x, elapsed = %.1f ms ", pit, dump[0], name, disp, e);
					else
						MsgCB("#	%.3d. this = 0x%08p, func = 0x%08p, elapsed = %.1f ms, LE = %d ", pit, dump[0], dump[1], e, GetLastError());
				}

			}
		}

		Device.seqParallel.clear_not_free	();
		mt_elapsed();
		size_t count = Device.seqFrameMT.R.size();
		/// Device.seqFrameMT.profiling = 1;
		
		if (elps < 50.f) // alpet: очень жестокий подход с пропуском работы по кадру при опоздании!
			Device.seqFrameMT.Process	(rp_Frame);		
		elps = mt_elapsed();
		time_points.AddPoint(__LINE__);
		if (elps > 30.f && Device.bWarnFreeze)
			MsgCB("##PERF_WARN: Device.seqFrameMT.Process	(rp_Frame) eats time %.1f ms, seqFrameMT.R.size() = %d ",
				   elps, count);


		// now we give control to device - signals that we are ended our work
		Device.mt_csEnter.Leave	();
		// waits for device signal to continue - to start again
		Device.mt_csLeave.Enter	();
		// returns sync signal to device
		Device.mt_csLeave.Leave	();
	}
	
}

#include "igame_level.h"
void CRenderDevice::PreCache	(u32 amount)
{
	if (HW.Caps.bForceGPU_REF)	amount=0;
#ifdef DEDICATED_SERVER
	amount = 0;
#endif
	// Msg			("* PCACHE: start for %d...",amount);
	dwPrecacheFrame	= dwPrecacheTotal = amount;
	if(amount && !precache_light && g_pGameLevel)
	{
		precache_light					= ::Render->light_create();
		precache_light->set_shadow		(false);
		precache_light->set_position	(vCameraPosition);
		precache_light->set_color		(255,255,255);
		precache_light->set_range		(5.0f);
		precache_light->set_active		(true);
	}
}


int g_svDedicateServerUpdateReate = 100;

ENGINE_API xr_list<LOADING_EVENT>			g_loading_events;

u32     msg_set  = 0;
float   msg_time = 0.f;



extern bool IsMainMenuActive();

ENGINE_API u32 TargetRenderLoad ()
{
	u32 result = 100;
	if (IsMainMenuActive())
		result = 30;
	if (Device.Paused())
		result = 10;
	if (!Device.b_is_Active)
		result = 0;

	return result;
}


void CRenderDevice::Run			()
{
//	DUMP_PHASE;
	g_bLoaded		= FALSE;
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	MSG				msg;
    BOOL			bGotMsg;
	Log				("*Starting engine...");
	thread_name		("X-RAY Primary thread");

	// Startup timers and calculate timer delta
	dwTimeGlobal				= 0;
	Timer_MM_Delta				= 0;
	{
		u32 time_mm			= timeGetTime	();
		while (timeGetTime()==time_mm);			// wait for next tick
		u32 time_system		= timeGetTime	();
		u32 time_local		= TimerAsync	();
		Timer_MM_Delta		= time_system-time_local;
	}

	// Start all threads
//	InitializeCriticalSection	(&mt_csEnter);
//	InitializeCriticalSection	(&mt_csLeave);
	mt_csEnter.Enter			();
	mt_bMustExit				= FALSE;
	thread_spawn				(mt_Thread,"X-RAY Secondary thread",0,0);

#ifdef ECO_RENDER				
	CTimer						fps_timer;
	u32							fps_checks = 0;
	float avg_fps				= 50;	
	float time_accum			= 0.f;
	float target_fmt			= 0.f;
	float adjust_delay			= 0.f;
	float median_elapsed		= 0.f;
	u32  last_check_frame		= 0;
#endif

	// Message cycle
    PeekMessage					( &msg, NULL, 0U, 0U, PM_NOREMOVE );

	seqAppStart.Process			(rp_AppStart);

	CHK_DX(HW.pDevice->Clear(0,0,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1,0));
	
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		
	Log				("* Runing message loop...");
	if (psDeviceFlags.test(rsConstantFPS))
		Log("! #WARN: used constant fps mode");

	while( WM_QUIT != msg.message  )
    {		
		bGotMsg = 0;				

		u32 msg_every = (b_is_Ready ? 1 : 1);
		u32 msg_start = frame_timer.GetElapsed_ms();

		msg_set <<= 1;


		if (Device.dwFrame % msg_every == 0)   
            bGotMsg = PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE );
        if( bGotMsg )
        {
			msg_set |= 1;
            TranslateMessage	( &msg );
            DispatchMessage	( &msg );
			msg_time += ( frame_timer.GetElapsed_ms() - msg_start ) / 1000.f;
			if (msg_time >= 0.008f)
			{
				string64 smsg;
				string2048 info; 
				DWORD disp;
				itoa (msg.message, smsg, 10);

				if (WM_TIMER == msg.message && msg.lParam > 0x100000)
				{
					strcpy_s(smsg, 64, "WM_TIMER");
					GetFunctionInfo((PVOID)msg.lParam, info, &disp);
					LPSTR s = info + xr_strlen(info);
					if (disp) sprintf_s(s, 1024, "+ 0x%x", disp);
				}
				else
					sprintf_s(info, 2048, "0x%x", msg.lParam);

				if (!Paused() && b_is_Active)
					MsgCB("!#PERF_WARN: after message %s processing time = %.1f ms, lParam = %s, wParam = 0x%x", smsg, msg_time * 1000.f, info, msg.wParam);
			}

        }
        else
        {		
			if (Paused())
			{
				lua_State  *L = g_aux_lua;								
				if (b_is_Ready && L)
					LuaExecute(g_aux_lua, "paused_idle_load", "");
				else 
					Sleep(1);
			}

			if (b_is_Ready) {				
#ifdef DEDICATED_SERVER
				u32 FrameStartTime = TimerGlobal.GetElapsed_ms();
#endif

				// дефайн ECO_RENDER лучше определять в свойствах проектов, а не в build_config_defines



#ifdef ECO_RENDER				
				if(b_is_Active && g_fTimeInteractive > 5.f) {  // namespace isolation
					float optimal		  = 0.f;
					// bool time_elapsed = false;
					float felps = frame_timer.GetElapsed_sec();

					if (median_elapsed < 0.001f)
						median_elapsed = felps;
					else
						median_elapsed = median_elapsed * 0.99f + felps * 0.01f;

					if (Device.dwFrame - last_check_frame >= 25)
					{
						fps_checks++;																		
						float ftd = fps_timer.GetElapsed_sec();												
						if (fps_checks > 1) // после паузы игнор
							time_accum = ftd;
						else
						{   // что-то сбивает расчеты, начать накопление снова
							last_check_frame = Device.dwFrame;
							time_accum		 = 0;
							fps_timer.Start();
						}

						if (g_iTargetFPS <= 500 && time_accum > 0.5f && fps_checks >= 10) // && b_is_Active
						{
							// Если за 0.5 секунды, прошло 50 кадров, fps = (50 / 0.5) = 100
							float frames = (float)(Device.dwFrame - last_check_frame);
							float fps = frames / time_accum;
							avg_fps = avg_fps * 0.9f + fps * 0.1f;
							if (avg_fps > fps + 10) avg_fps = fps; // fast drop reaction

							// если целевая частота кадров ниже текущей, увеличить задержку
							if (g_iTargetFPS < avg_fps -  1 && adjust_delay < 5.f) adjust_delay += 0.1f; // аккуратно
							if (g_iTargetFPS < avg_fps -  5 && adjust_delay < 5.f) adjust_delay += 1.0f; // грубо
							// если целевая частота кадров выше текущей, уменьшить задержку
							if (g_iTargetFPS > avg_fps +  1 && adjust_delay > -5.f) adjust_delay -= 0.1f;
							if (g_iTargetFPS > avg_fps +  5 && adjust_delay > -5.f) adjust_delay -= 1.0f;


							target_fmt = 1.f / float(g_iTargetFPS); // сколько времени должен занимать идеальный средний кадр
							target_fmt += adjust_delay * 0.001f;    // добавить смещение в мс

							clamp<float>(target_fmt, 0.0f, 0.035f);
							Statistic->fTargetMedian = target_fmt;

							last_check_frame = Device.dwFrame;
							time_accum = 0;
							fps_timer.Start();
						}
					}

					if (g_iTargetFPS < 900)
						optimal = target_fmt; // целевая задержка на каждый кадр рендера
					// задержка осторожно добавляется в конце кадра
					while (fps_checks > 10 && felps < optimal)
					{
						SwitchToThread();
						felps = frame_timer.GetElapsed_sec();			// текущее время кадра  						
						if (felps + 0.020f <= optimal)   // если более 100 кадров в секунду, как в меню например
						{	
							if (g_aux_lua)
							{
								lua_State  *L = g_aux_lua;
								float max_avail = 1000.f * (optimal - felps);
								LuaExecute(g_aux_lua, "frame_idle_load", "fs", max_avail, "device");
								felps = frame_timer.GetElapsed_sec();
							}							
						}
						Statistic->RenderTOTAL.cycles++;		   	  // idle cycles count
						if (felps + 0.002f <= optimal)
							SleepEx(1, (optimal - felps) > 0.01f);	   // попытка обойти разно-платформные особенности	 
					}

				}
				else
				{
					fps_checks = 0;
					median_elapsed = 0.f;
				}

				// if (g_iTargetFPS < 900 && TargetRenderLoad() < 50 && time_elapsed) optimal = 5; // 200 fps limit
				
				
#else
				Sleep(0);
#endif // ECO_RENDER			

				time_points.AddPoint(__LINE__); // last value

				if (psDeviceFlags.test(rsStatistic))	g_bEnableStatGather	= TRUE;
				else									g_bEnableStatGather	= FALSE;
				if(g_loading_events.size())
				{					
					if( g_loading_events.front() () )
						g_loading_events.pop_front();
					
					pApp->LoadDraw				();
					continue;
				}
				else
				{
					FrameMove();   // здесь заканчивается обработка кадра, и начинается новый кадр
				}
								
				msg_time  = 0.f;				
				time_points.AddPoint(__LINE__);
				// Precache
				if (dwPrecacheFrame)
				{
					float factor					= float(dwPrecacheFrame)/float(dwPrecacheTotal);
					float angle						= PI_MUL_2 * factor;
					vCameraDirection.set			(_sin(angle),0,_cos(angle));	vCameraDirection.normalize	();
					vCameraTop.set					(0,1,0);
					vCameraRight.crossproduct		(vCameraTop,vCameraDirection);

					mView.build_camera_dir			(vCameraPosition,vCameraDirection,vCameraTop);
				}

				// Matrices
				mFullTransform.mul			( mProject,mView	);
				RCache.set_xform_view		( mView				);
				RCache.set_xform_project	( mProject			);
				D3DXMatrixInverse			( (D3DXMATRIX*)&mInvFullTransform, 0, (D3DXMATRIX*)&mFullTransform);
				// *** Resume threads
				// Capture end point - thread must run only ONE cycle
				// Release start point - allow thread to run
				mt_csLeave.Enter			();
				time_points.AddPoint(__LINE__);
				mt_csEnter.Leave			();

#ifndef DEDICATED_SERVER
				if (g_bEnableStatGather)
				{
					Statistic->RenderTOTAL_Real.FrameStart();
					Statistic->RenderTOTAL_Real.Begin();
				}

				if (b_is_Active)							{
					if (Begin())				{
						time_points.AddPoint(__LINE__);
						seqRender.Process						(rp_Render);
						time_points.AddPoint(__LINE__);
						if (psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || Statistic->errors.size())	
							Statistic->Show						();
						End										();
					}
				} 
				else Sleep(10);


				if (g_bEnableStatGather)
				{
					Statistic->RenderTOTAL_Real.End();
					Statistic->RenderTOTAL_Real.FrameEnd();
					Statistic->RenderTOTAL.accum = Statistic->RenderTOTAL_Real.accum;
				}
				
#endif
				// *** Suspend threads
				// Capture startup point
				// Release end point - allow thread to wait for startup point
				mt_csEnter.Enter						();
				time_points.AddPoint(__LINE__);
				mt_csLeave.Leave						();
				

				// Ensure, that second thread gets chance to execute anyway
				if (dwFrame!=mt_Thread_marker)			{
					mt_access = GetCurrentThreadId();
					for (u32 pit=0; pit<Device.seqParallel.size(); pit++)					
						Device.seqParallel[pit]			();
					Device.seqParallel.clear_not_free	();
					seqFrameMT.Process					(rp_Frame);
				}
				time_points.AddPoint(__LINE__);

#ifdef DEDICATED_SERVER
				u32 FrameEndTime = TimerGlobal.GetElapsed_ms();
				u32 FrameTime = (FrameEndTime - FrameStartTime);
				/*
				string1024 FPS_str = "";
				string64 tmp;
				strcat(FPS_str, "FPS Real - ");
				if (dwTimeDelta != 0)
					strcat(FPS_str, ltoa(1000/dwTimeDelta, tmp, 10));
				else
					strcat(FPS_str, "~~~");

				strcat(FPS_str, ", FPS Proj - ");
				if (FrameTime != 0)
					strcat(FPS_str, ltoa(1000/FrameTime, tmp, 10));
				else
					strcat(FPS_str, "~~~");
				
*/
				u32 DSUpdateDelta = 1000/g_svDedicateServerUpdateReate;
				if (FrameTime < DSUpdateDelta)
				{
					Sleep(DSUpdateDelta - FrameTime);
//					Msg("sleep for %d", DSUpdateDelta - FrameTime);
//					strcat(FPS_str, ", sleeped for ");
//					strcat(FPS_str, ltoa(DSUpdateDelta - FrameTime, tmp, 10));
				}

//				Msg(FPS_str);
#endif

			} else { 
				Sleep		(5); // if not ready
			};
        } // non-message cycle
    } // message loop
	seqAppEnd.Process		(rp_AppEnd);

	// Stop Balance-Thread
	mt_bMustExit			= TRUE;
	mt_csEnter.Leave		();
	while (mt_bMustExit)	Sleep(0);
//	DeleteCriticalSection	(&mt_csEnter);
//	DeleteCriticalSection	(&mt_csLeave);
}

void ProcessLoading(RP_FUNC *f);
void CRenderDevice::FrameMove()
{
	dwFrame			++;
	dwTimeContinual	= TimerMM.GetElapsed_ms	();
	if (psDeviceFlags.test(rsConstantFPS))	{
		// 20ms = 50fps
		fTimeDelta		=	0.0205f;			
		fTimeGlobal		+=	0.020f;
		dwTimeDelta		=	20;
		dwTimeGlobal	+=	20;
	} else {
		// Timer
		float fPreviousFrameTime = Timer.GetElapsed_sec(); 
		if (fPreviousFrameTime > 0.08f && b_is_Ready && b_is_Active && !g_pauseMngr.Paused() && g_fTimeInteractive > 5.f )
		{
			MsgCB("!#PERF_WARN: frame %d (mt=%d), rendered while %.1f ms, target_time = %.1f ms,  msg_time = %.1f ms, interactive = %.1f sec ",
					 dwFrame, mt_Thread_marker, fPreviousFrameTime * 1000.f, Statistic->fTargetMedian * 1000.f,  
					 msg_time * 1000.f, g_fTimeInteractive);

			time_points.Dump();			
			if (fPreviousFrameTime > 0.5f)
			{
				Msg("!#WARN_HEAVY_FREEZE");
				MsgCB("$#DUMP_CONTEXT");
			}
		}
		Timer.Start();	// previous frame
		warn_flag = false;
		fTimeDelta = 0.1f * fTimeDelta + 0.9f * fPreviousFrameTime;			// smooth random system activity - worst case ~7% error
		if (fTimeDelta > .3f) fTimeDelta= .3f;							   // limit 300ms

		// if(Paused())		fTimeDelta = 0.0f;

//		u64	qTime		= TimerGlobal.GetElapsed_clk();
		fTimeGlobal		= TimerGlobal.GetElapsed_sec(); //float(qTime)*CPU::cycles2seconds;
		u32	_old_global	= dwTimeGlobal;
		dwTimeGlobal	= TimerGlobal.GetElapsed_ms	();	//u32((qTime*u64(1000))/CPU::cycles_per_second);
		dwTimeDelta		= dwTimeGlobal-_old_global;
	}

	if (g_bGameInteractive && g_bLoaded)
		g_fTimeInteractive += (b_is_Active && !Paused() ? fTimeDelta : 0.f);
	else
		g_fTimeInteractive = 0.f;	

	frame_timer.Start();
	time_points.Reset();

	// Frame move
	Statistic->EngineTOTAL.Begin	();
	if (!g_bLoaded)
	{
		ProcessLoading(rp_Frame);	
	}
	else
		seqFrame.Process			(rp_Frame);
	Statistic->EngineTOTAL.End	();
}

void ProcessLoading				(RP_FUNC *f)
{
	Device.seqFrame.Process				(rp_Frame);
	g_bLoaded							= TRUE;
}

ENGINE_API BOOL bShowPauseString = TRUE;
#include "IGame_Persistent.h"

void CRenderDevice::Pause(BOOL bOn, BOOL bTimer, BOOL bSound, LPCSTR reason)
{
	static int snd_emitters_ = -1;

	if (g_bBenchmark)	return;


#ifdef DEBUG
	Msg("pause [%s] timer=[%s] sound=[%s] reason=%s",bOn?"ON":"OFF", bTimer?"ON":"OFF", bSound?"ON":"OFF", reason);
#endif // DEBUG

#ifndef DEDICATED_SERVER	

	if(bOn)
	{
		if(!Paused())						
			bShowPauseString				= TRUE;

		if( bTimer && g_pGamePersistent->CanBePaused() )
			g_pauseMngr.Pause				(TRUE);
	
		if(bSound){
			snd_emitters_ =					::Sound->pause_emitters(true);
#ifdef DEBUG
			Log("snd_emitters_[true]",snd_emitters_);
#endif // DEBUG
		}
	}else
	{
		if( bTimer && /*g_pGamePersistent->CanBePaused() &&*/ g_pauseMngr.Paused() )
			g_pauseMngr.Pause				(FALSE);
		
		if(bSound)
		{
			if(snd_emitters_>0) //avoid crash
			{
				snd_emitters_ =				::Sound->pause_emitters(false);
#ifdef DEBUG
				Log("snd_emitters_[false]",snd_emitters_);
#endif // DEBUG
			}else {
#ifdef DEBUG
				Log("Sound->pause_emitters underflow");
#endif // DEBUG
			}
		}
	}

#endif

}

BOOL CRenderDevice::Paused()
{
	return g_pauseMngr.Paused();
};

void CRenderDevice::OnWM_Activate(WPARAM wParam, LPARAM lParam)
{
	u16 fActive						= LOWORD(wParam);
	BOOL fMinimized					= (BOOL) HIWORD(wParam);
	BOOL bActive					= ((fActive!=WA_INACTIVE) && (!fMinimized))?TRUE:FALSE;

	if (bActive!=Device.b_is_Active)
	{
		Device.b_is_Active				= bActive;
		g_appActive						= bActive;

		if (Device.b_is_Active)	
		{
			Device.seqAppActivate.Process(rp_AppActivate);
#ifndef DEDICATED_SERVER
				ShowCursor			(FALSE);
#endif
		}else	
		{
			Device.seqAppDeactivate.Process(rp_AppDeactivate);
			ShowCursor				(TRUE);
		}
	}
}

ENGINE_API bool busy_warn(LPCSTR file, u32 line, LPCSTR func, u32 timeout)
{
	if (warn_flag) return true;

	if (Device.dwFrame < 1000 || g_fTimeInteractive < 5.f) return false;

	float average = Device.fTimeDelta > 0.01f ? Device.fTimeDelta * 1000.f : 10.f; // 100 fps эталон

	if (timeout < 10)	{		
		timeout = (u32)round(average * (float)timeout); // сколько средних дельт считать пороговым значением
		timeout = __min(timeout, 70);
	}
	
	u32 elps = Device.frame_elapsed();
	if (elps >= timeout && Device.bWarnFreeze)
	{
		MsgCB("%s#BUSY_WARN:~C07 %35s:%d in function '%-25s' frame_elapsed =~C0D %d~C07, frame_average =~C0D %.1f~C07", 
				 (elps > 100 ? "!" : "#"), file, line, func, elps, average);
		warn_flag = true;
		return true;
	}
	return false;
}

