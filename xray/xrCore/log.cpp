#include "stdafx.h"
#pragma hdrstop

#include <time.h>
#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
#include "log.h"
#include "../../build_config_defines.h"

BOOL						LogExecCB		= TRUE;
static string_path			logFName		= "engine.log";
static BOOL 				no_log			= TRUE;
extern BOOL					crash_flag;
CTimer						dump_timer;
LPCSTR						CONTEXT_TAG			= "$#CONTEXT";
LPCSTR						DUMP_CONTEXT_TAG	= "$#DUMP_CONTEXT";

str_container				verbosity_filters; // набор фильтров для вывода регулярных сообщений
 u32						verbosity_level = 3;

 typedef void	( * TIME_GETTER)	(SYSTEMTIME *dst);

 static TIME_GETTER TimeGet = NULL;

 XRCORE_API TIME_GETTER SetTimeGetter(TIME_GETTER tg)
 {
	 TIME_GETTER old = TimeGet;
	 TimeGet = tg;
	 return old;
 }

 XRCORE_API PSYSTEMTIME XrLocalTime() 
 {
	 static SYSTEMTIME result;
	 if (TimeGet)
		 TimeGet (&result);
	 else
		 GetLocalTime(&result);

	 return &result;
 }

 XRCORE_API __time64_t XrLocalTime64()
 {
	 PSYSTEMTIME pst = XrLocalTime();
	 struct tm _tm;
	 _tm.tm_mday = pst->wDay;
	 _tm.tm_mon  = pst->wMonth - 1;
	 _tm.tm_wday = pst->wDayOfWeek;
	 _tm.tm_year = pst->wYear - 1900;

	 _tm.tm_hour = pst->wHour;
	 _tm.tm_min  = pst->wMinute;
	 _tm.tm_sec  = pst->wSecond;
	 _tm.tm_isdst = -1;
	 
	 return _mktime64(&_tm) * 1000LL + pst->wMilliseconds;
 }


 typedef struct  _LOG_LINE
 {
	 __time64_t		ts;
	 char		    msg [4096 - sizeof(__time64_t)];
 } LOG_LINE; 


#define	LOG_TIME_PRECISE
bool __declspec(dllexport) force_flush_log = false;	// alpet: выставить в true если лог все-же записывается плохо при вылете. Слишком частая запись лога вредит SSD и снижает произволительность.

XRCORE_API LPCSTR format_time(LPCSTR format, __time64_t t, bool add_ms = true)
{
	if (!t)
		return "never";

	u32 msec = 0;
	if (t > 1000000000000LL)
	{
		msec = (t % 1000LL);
		t /= 1000LL;
	}


	static string64 buffers [8];
	static int last_buff = 0;
	char *buff = buffers[last_buff ++];
	last_buff &= 7;
	struct tm * lt = localtime(&t);
	strftime(buff, 63, format, lt);

	if (add_ms)
		sprintf_s(buff, 64, "%s.%03d", buff, msec);

	return buff;
}



#ifdef PROFILE_CRITICAL_SECTIONS
	static xrCriticalSection	logCS(MUTEX_PROFILE_ID(log));
#else // PROFILE_CRITICAL_SECTIONS
	static xrCriticalSection	logCS;
#endif // PROFILE_CRITICAL_SECTIONS
xr_vector<shared_str>*		LogFile			= NULL;
static LogCallback			CrashCB			= 0;
static LogCallback			LogCB			= 0;

IWriter *LogWriter;

size_t cached_log = 0;

void FlushLog			(LPCSTR file_name)
{
	if (LogWriter)
		LogWriter->flush();
	cached_log = 0;
}

void FlushLog			()
{
	FlushLog		(logFName);
}

#pragma optimize("gyt", off)

void __cdecl InitVerbosity (const char *filters)
{
	string4096 tmp;
	strcpy_s(tmp, 4095, filters);
	tmp[sizeof(tmp) - 1] = 0;
	char *t = strtok(tmp, ",");
	while (NULL != t)
	{
		if (xr_strlen(t) > 0)
			verbosity_filters.dock(t)->dwReference++;
		t = strtok(NULL, ",");
	}

}

extern bool shared_str_initialized;


void AddOne				(const char *split) 
{
	if(!LogFile)		
						return;

	logCS.Enter			();

#ifdef DEBUG
	OutputDebugString	(split);
	OutputDebugString	("\n");
#endif

	//exec CallBack
	if (LogExecCB && LogCB)
		LogCB(split);

//	DUMP_PHASE;
	{
		if (shared_str_initialized && !crash_flag)
		{
			shared_str			temp = shared_str(split);
			LogFile->push_back(temp); // для отображения в окне консоли

			if (LogFile->size() > 2000)
			{
				for (auto it = LogFile->begin(); it != LogFile->end(); it++)
					if (it->size() == 0 || it->c_str()[1] != '~' )
					{
						LogFile->erase(it);
						break;
					}
			}

		}

		//+RvP, alpet
		if (LogWriter)
		{				
			switch (*split)
			{
			case 0x21:
			case 0x23:
			case 0x25:
				split ++; // пропустить первый символ, т.к. это вероятно цветовой тег
				break;
			}

			string128 buf;
#ifdef	LOG_TIME_PRECISE 
			SYSTEMTIME &lt = *XrLocalTime();
			// GetLocalTime(&lt);
			
			sprintf_s(buf, 128, "[%02d.%02d.%02d %02d:%02d:%02d.%03d] ", lt.wDay, lt.wMonth, lt.wYear % 100, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);						
			LogWriter->w_printf("%s%s\r\n", buf, split);
			cached_log += xr_strlen(buf);
			cached_log += xr_strlen(split) + 2;
#else
			time_t t = time(NULL);
			tm* ti = localtime(&t);
			
			strftime(buf, 128, "[%x %X]\t", ti);

			LogWriter->wprintf("%s %s\r\n", buf, split);
#endif
			if (force_flush_log || cached_log >= 32768)
				FlushLog();
		}
		//-RvP
	}


	logCS.Leave				();
}

xr_string			g_sprintf_buffers[16];
xr_string			g_void_str;
volatile ULONG		g_sprintf_index = 0;




XRCORE_API const xr_string& __cdecl xr_sprintf(LPCSTR format, va_list &mark)
{
	char	buf[0x10000]; // 64KiB // alpet: размер буфера лучше сделать побольше, чтобы избежать вылетов invalid parameter handler при выводе стеков вызова
	int sz = _vsnprintf_s (buf, sizeof(buf) - 1, _TRUNCATE, format, mark); buf[sizeof(buf) - 1] = 0;
	if (sz < 0)
		return g_void_str.assign("");

	buf[sz] = 0;
	va_end(mark);
	ULONG n = InterlockedIncrement(&g_sprintf_index) & 0xF;
	if (!mem_initialized)
		Memory._initialize();
	
	
	return g_sprintf_buffers[n].assign(buf, sz);
}

XRCORE_API const xr_string& __cdecl xr_sprintf(LPCSTR format, ...) // uniform sprintf
{
	va_list mark;	
	va_start(mark, format);					
	return xr_sprintf(format, mark);							
}

extern "C"
{
	XRCORE_API LPCSTR __stdcall xrx_sprintf(LPCSTR format, ...)
	{		
		va_list mark;	
		va_start(mark, format);					
		return xr_sprintf(format, mark).c_str();
	}
};


void __cdecl LogVAList(const char *format, va_list &mark)
{
	Log(xr_sprintf(format, mark).c_str());	
}

void __cdecl Msg		( const char *format, ...)
{
	va_list mark;	
	va_start	(mark, format );
	LogVAList   (format, mark);
}

extern	bool g_bFPUInitialized;

XRCORE_API void __cdecl ExecLogCB (LPCSTR msg) // alpet: вывод сообщений только в колбек (для отладки и передачи данных в перехватчик)
{

#define RING_SIZE 64
	static CTimer ctx_timer;
	static LOG_LINE ctx_ring[RING_SIZE];   // кольцевой буфер для сохранения данных контекста выполнения (выводится при сбое, или по необходимости)
	static u32 ctx_index		= 0;
	static u32 last_dump_index	= 0;
	if (strstr(msg, "$#DEBUG:"))
	{
		if (!IsDebuggerPresent()) return;
	}


	// функция двойного назначения: может использоваться для вотчинга произвольных переменных в местах потенциальных сбоев

	LPCSTR start = strstr(msg, CONTEXT_TAG);
	if (start && xr_strlen(start) >= 16)
	{
		start += 2;

		// SYSTEMTIME lt;
		// GetLocalTime(&lt);
		float elps = 0;
		if (ctx_index > 0 && g_bFPUInitialized)
			elps = ctx_timer.GetElapsed_sec() * 1000.f;
		else
			dump_timer.Start();

		LOG_LINE &dest = ctx_ring[ctx_index & (RING_SIZE - 1)];
		dest.ts = XrLocalTime64();
		if (dest.ts < 1000000000000LL)
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			dest.ts *= 1000LL;
			dest.ts += (__time64_t)st.wMilliseconds;
		}
		ctx_index++;
		// copy-paste forever	
		//  lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds, 
		//  %02d:%02d:%02d.%03d
		
		sprintf_s(dest.msg, sizeof(dest.msg) - 1, "[%5.1f]. %s", elps, start);						
		ctx_timer.Start();
		return;
	}
	if (strstr(msg, DUMP_CONTEXT_TAG))
	{
		if (last_dump_index == ctx_index) return;

		last_dump_index = ctx_index;
		dump_timer.Start();
		__time64_t now;
		_time64(&now);
		// время в микросекундах, т.е. надо делить на миллион
#define  ONE_SEC64 1000000

		MsgCB("<DEBUG_CONTEXT_DUMP>"); // рекурсия!
		for (u32 i = RING_SIZE - 1; i > 0; i--)
		{
			LOG_LINE &line = ctx_ring[(ctx_index + i) & (RING_SIZE - 1)];
			if (line.ts + 5 * ONE_SEC64 >= now) // 5 seconds limit
			{
				LPCSTR tm = format_time("%H:%M:%S", line.ts, true);
				if (!strstr(line.msg, DUMP_CONTEXT_TAG))
					 MsgCB("# [%s]%s", tm, line.msg);
				line.ts = 0; // чтобы больше не выводить 
			}
			
		}
		MsgCB("<DEBUG_CONTEXT_DUMP/>");
		return;
	}

	if (NULL != LogCB && msg[0]) LogCB(msg);
}

void 	XRCORE_API	__cdecl  MsgCB(LPCSTR format, ...)
{
	// на этапе вызова этой функции, ещё не инициализированн xrMemory объект
	va_list mark;
	va_start(mark, format);
	char	buf[0x4000]; // 64KiB // alpet: размер буфера лучше сделать побольше, чтобы избежать вылетов invalid parameter handler при выводе стеков вызова
	int sz = _vsnprintf(buf, sizeof(buf) - 1, format, mark); buf[sizeof(buf) - 1] = 0;
	buf[sz] = 0;
	va_end(mark);
	ExecLogCB (buf);
}

extern "C"
{
	XRCORE_API void __stdcall  xr_MsgCB(LPCSTR format, ...) // alternate MsgCB version
	{
		va_list mark;
		va_start(mark, format);
		ExecLogCB (xr_sprintf(format, mark).c_str());
	}

	XRCORE_API LPCSTR __stdcall GetFunctionInfo(LPVOID p)
	{
		DWORD disp = 0;
		static string4096 buff;
		sprintf_s(buff, 4095, "0x%p", p);
		if (GetFunctionInfo(p, buff, &disp))
		{
			if (disp)
				sprintf_s(buff + xr_strlen(buff), 1024, "+ 0x%x", disp);
		}
		return buff;
	}
}

#ifdef LUAICP_COMPAT
__declspec(dllexport) void DumpContext() // for use from external DLL
{
	MsgCB(DUMP_CONTEXT_TAG);
}
#endif

void __cdecl MsgV (const char *verbosity, const char *format, ...)
{
	if (!verbosity) return;	
	bool b_show = (verbosity[0] != '!');
	if (!b_show)
		verbosity++;

	if (verbosity[0] >= '0' && verbosity[0] <= '9')
	{
		u32 msg_level = verbosity[0] - '0';
		if (msg_level > verbosity_level) return;
		verbosity++;
	}
	str_value *f = verbosity_filters.dock(verbosity);
	if ( (f->dwReference > 0) == b_show )
	{		
		va_list mark;
		va_start(mark, format);		
		LogVAList (format, mark);
	}
	// f->dwReference--;
}

void Log				(const char *s) 
{
	int		i,j;
	string4096	split;
	if (strstr(s, CONTEXT_TAG) || strstr(s, DUMP_CONTEXT_TAG))
	{
		ExecLogCB(s);
		return;
	}

	if (strstr(s, "#ERROR"))
	{
		string_path snd;
		if (FS.update_path(snd, "$fs_root$", "sounds\\error.wav"))
			PlaySound (snd, 0, SND_FILENAME | SND_NOWAIT );
	}
		

	for (i=0,j=0; s[i]!=0; i++) {
		if (s[i]=='\n') {
			split[j]=0;	// end of line
			if (split[0]==0) { split[0]=' '; split[1]=0; }
			AddOne(split);
			j=0;
		} else {
			split[j++]=s[i];
		}
	}
	split[j]=0;
	AddOne(split);
}


void Log				(const char *msg, const char *dop) {
	char buf[1024];

	if (dop)	sprintf_s(buf,sizeof(buf),"%s %s",msg,dop);
	else		sprintf_s(buf,sizeof(buf),"%s",msg);

	Log		(buf);
}

void Log				(const char *msg, u32 dop) {
	char buf[1024];

	sprintf_s	(buf,sizeof(buf),"%s %d",msg,dop);
	Log			(buf);
}

void Log				(const char *msg, int dop) {
	char buf[1024];

	sprintf_s	(buf, sizeof(buf),"%s %d",msg,dop);
	Log		(buf);
}

void Log				(const char *msg, float dop) {
	char buf[1024];

	sprintf_s	(buf, sizeof(buf),"%s %f",msg,dop);
	Log		(buf);
}

void Log				(const char *msg, const Fvector &dop) {
	char buf[1024];

	sprintf_s	(buf,sizeof(buf),"%s (%f,%f,%f)",msg,dop.x,dop.y,dop.z);
	Log		(buf);
}

void Log				(const char *msg, const Fmatrix &dop)	{
	char	buf	[1024];

	sprintf_s	(buf,sizeof(buf),"%s:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",msg,dop.i.x,dop.i.y,dop.i.z,dop._14_
																				,dop.j.x,dop.j.y,dop.j.z,dop._24_
																				,dop.k.x,dop.k.y,dop.k.z,dop._34_
																				,dop.c.x,dop.c.y,dop.c.z,dop._44_);
	Log		(buf);
}

void LogWinErr			(const char *msg, long err_code)	{
	Msg					("%s: %s",msg,Debug.error2string(err_code)	);
}



typedef void (WINAPI *OFFSET_UPDATER)(LPCSTR key, u32 ofs);


void	LogXrayOffset(LPCSTR key, LPVOID base, LPVOID pval)
{
#ifdef LUAICP_COMPAT
	u32 ofs = (u32)pval - (u32)base;
	MsgV	  ("XRAY_OFFSET", "XRAY_OFFSET: %30s = 0x%x base = 0x%p, pval = 0x%p ", key, ofs, base, pval);
	static OFFSET_UPDATER cbUpdater = NULL;
	HMODULE hDLL = GetModuleHandle("luaicp.dll");
	if (!cbUpdater && hDLL)
		cbUpdater = (OFFSET_UPDATER) GetProcAddress(hDLL, "UpdateXrayOffset");

	if (cbUpdater)
		cbUpdater(key, ofs);
#endif
}

void SetCrashCB			(LogCallback cb)
{
	CrashCB				= cb;
}

void SetLogCB			(LogCallback cb)
{
	LogCB				= cb;
}

LogCallback  GetCrashCB()
{
	return CrashCB;
}


LogCallback  GetLogCB()
{
	return LogCB;
}

void    XRCORE_API	HandleCrash(LPCSTR format, ...) // вывод информации по сбою через колбек
{
	if (!CrashCB) return;

	va_list mark;
	string4096	buf;
	va_start(mark, format);
	int sz = _vsnprintf(buf, sizeof(buf)-1, format, mark); buf[sizeof(buf)-1] = 0;
	va_end(mark);	
	CrashCB(buf);
}


LPCSTR log_name			()
{
	return				(logFName);
}

void InitLog()
{
	R_ASSERT			(LogFile==NULL);
	LogFile				= xr_new< xr_vector<shared_str> >();
}

void CreateLog			(BOOL nl)
{
    no_log				= nl;
	strconcat			(sizeof(logFName), logFName, "~", Core.ApplicationName,"_",Core.UserName,".baselog");
	if (FS.path_exist("$logs$"))
		FS.update_path	(logFName,"$logs$",logFName);
	if (!no_log) {
		LogWriter = FS.w_open	(logFName);
        if (LogWriter == NULL){
        	MessageBox	(NULL,"Can't create log file.","Error",MB_ICONERROR);
        	abort();
        }		
	
		__time64_t t = _time64(NULL);
		struct tm* ti = _localtime64(&t);
		string64 buf;
		strftime(buf, 63, "[%H:%M:%S]. #INIT: ", ti);
		// strftime(buf, 128, "[%x %X]\t", ti);

        for (u32 it=0; it < LogFile->size(); it++)	{
			LPCSTR		s	= *((*LogFile)[it]);
			LogWriter->w_printf("%s%s\n", buf, s?s:"");
		}
		LogWriter->flush();
    }
	LogFile->reserve	(128);
}

void CloseLog(void)
{
	if(LogWriter){
		FS.w_close(LogWriter);
	}

	FlushLog		();
 	LogFile->clear	();
	xr_delete		(LogFile);
}
