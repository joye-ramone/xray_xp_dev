#ifndef logH
#define logH

//+RvP
#include <Windows.h>
#include <fstream>
#include <ctime>
//-RvP

#define VPUSH(a)	a.x,a.y,a.z

extern  XRCORE_API  u32				verbosity_level;

void	XRCORE_API  __cdecl			InitVerbosity(const char *filters);

void 	XRCORE_API	__cdecl			Msg			(LPCSTR format, ...);
void	XRCORE_API	__cdecl			MsgV		(const char *verbosity, const char *format, ...);    // alpet: вывод фильтруемых сообщений
void 	XRCORE_API	__cdecl			MsgCB		(LPCSTR format, ...);	// alpet: вывод сообщений только в колбек (для отладки) или в контекстный буфер

void 	XRCORE_API		Log			(LPCSTR msg);
void 	XRCORE_API		Log			(LPCSTR msg);
void 	XRCORE_API		Log			(LPCSTR msg, LPCSTR			dop);
void 	XRCORE_API		Log			(LPCSTR msg, u32			dop);
void 	XRCORE_API		Log			(LPCSTR msg, int  			dop);
void 	XRCORE_API		Log			(LPCSTR msg, float			dop);
void 	XRCORE_API		Log			(LPCSTR msg, const Fvector& dop);
void 	XRCORE_API		Log			(LPCSTR msg, const Fmatrix& dop);
void 	XRCORE_API		LogWinErr	(LPCSTR msg, long 			err_code);

void	XRCORE_API		LogXrayOffset(LPCSTR key, LPVOID base, LPVOID pval);


typedef void	( * LogCallback)	(LPCSTR string);
void	XRCORE_API				SetCrashCB	(LogCallback cb);
void	XRCORE_API				SetLogCB	(LogCallback cb);

LogCallback XRCORE_API			GetCrashCB	();
LogCallback XRCORE_API			GetLogCB	();
void    XRCORE_API				HandleCrash (LPCSTR format, ...);

void 							CreateLog	(BOOL no_log=FALSE);
void 							InitLog		();
void 							CloseLog	();
void	XRCORE_API				FlushLog	();

extern 	XRCORE_API	xr_vector<shared_str>*		LogFile;
extern 	XRCORE_API	BOOL						LogExecCB;


XRCORE_API	u32   		SimpleExceptionFilter(PEXCEPTION_POINTERS pExPtrs);
#define SIMPLE_FILTER	SimpleExceptionFilter(GetExceptionInformation())

XRCORE_API BOOL GetFunctionInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp);
XRCORE_API BOOL GetObjectInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp);

// alpet: универсальный sprintf, возвращает ссылку на один из 16 глобальных буферов	
XRCORE_API const xr_string& __cdecl xr_sprintf (LPCSTR format, ...); 
extern "C"
{
	// alpet: использует те-же глобальные буферы, что и xr_sprintf, но удобна для экспорта в "чужие" DLL/EXE
	XRCORE_API LPCSTR	    __stdcall  xrx_sprintf (LPCSTR format, ...); 	
	
	// alpet: вариант MsgCB с экспортом в "Си"
	void 	XRCORE_API		__stdcall  xr_MsgCB      (LPCSTR format, ...); 
}


#endif

