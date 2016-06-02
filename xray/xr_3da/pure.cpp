#include "stdafx.h"
#pragma hdrstop

#include "pure.h"
#pragma optimize("gyts", off)

XRCORE_API BOOL GetFunctionInfo (PVOID pAddr, LPSTR name, LPDWORD pDisp);
XRCORE_API BOOL GetObjectInfo (PVOID pAddr, LPSTR name, LPDWORD pDisp);
XRCORE_API size_t DebugStackPush(int desc, void *ptr);
XRCORE_API void   DebugStackPop(size_t dest_sz);

extern float g_fTimeInteractive;
extern bool  warn_flag;
xr_map<DWORD, int> entrance;



ENGINE_API int	__cdecl	_REG_Compare(const void *e1, const void *e2)
{
	_REG_INFO *p1 = (_REG_INFO *)e1;
	_REG_INFO *p2 = (_REG_INFO *)e2;
	return (p2->Prio - p1->Prio);
}


LPCSTR RP_Dump(RP_FUNC *f, void *obj)
{
	DWORD disp, disp2;
	string1024 name;
	string1024 obj_info;
	GetFunctionInfo((void*)f, name, &disp);
	GetObjectInfo(obj, obj_info, &disp2); 
	static string2048 info;
	sprintf_s(info, 2048, "func = 0x%p (%s + 0x%x), obj = 0x%p (%s + 0x%x) ", (void*)f, name, disp, obj, obj_info, disp2);
	return info;
}


ENGINE_API bool RP_SafeCall(RP_FUNC *f, void *obj, int profiling)
{
	/* alpet исследование: 
	   В данной функции происходят вызовы основных апдейтеров игры, включая скриптовые (биндеры).
	   Она вызывается из главного и вспомогательного потока. 	
	*/

	if (!obj) return false;
	DWORD tid = GetCurrentThreadId();
	entrance[tid] = entrance[tid] + 1;
	if (entrance[tid] > 10)
		__asm int 3;

	u64 ticks = GetTickCount64();
	u64 tickz = ticks;
	u32 elps = Device.frame_elapsed();	

	size_t size = DebugStackPush(1, f); DebugStackPush(0, obj);
	bool result = true;	
	__try {
		__try {			
			f(obj);			
		}
		__except (SIMPLE_FILTER) {
			MsgCB("$#DUMP_CONTEXT");
			Msg("!#EXCEPTION: catched in RP_SafeCall for %s", RP_Dump(f, obj));
			result = false;
		}
	}
	__finally {
		ticks = GetTickCount64() - ticks;
		DebugStackPop(size);
		tickz = GetTickCount64() - tickz;
		entrance[tid] = entrance[tid] - 1;
	}

	if (profiling)
		MsgCB("$#CONTEXT: after call %s ", RP_Dump(f, obj));	

	if (warn_flag) return result;

	u32 felps = Device.frame_elapsed();
	elps = felps - elps; 	

	float avg_frame = __max (15.f, Device.fTimeDelta * 1000.f);
	if (elps > 500 || (elps > avg_frame * 2  && g_fTimeInteractive > 5.f && Device.bWarnFreeze && !Device.Paused()))
	{
		LPCSTR info = RP_Dump(f, obj);
		LPCSTR tag = (elps > 100 ? "!" : "#");
		MsgCB("%s#PERF_WARN: after %s frame elapsed = %d ms (+%d), ticks1 = %d, ticks2 = %d ", tag, info, felps, elps, u32(ticks), u32(tickz));	
		warn_flag = (elps >= 100);

	}
		
	return result;
}

/*

void __fastcall rp_Render(void *p) 
{ 
	pureRender *obj = (pureRender*)p;
	// MsgCB("$#CONTEXT: OnRender for 0x%p @ %s ", p, typeid(obj).name()); 

	if (obj)
		obj->OnRender();
	else
		__asm nop;
}
*/

DECLARE_RP(Frame);
DECLARE_RP(Render);
DECLARE_RP(AppActivate);
DECLARE_RP(AppDeactivate);
DECLARE_RP(AppStart);
DECLARE_RP(AppEnd);
DECLARE_RP(DeviceReset);

