#include "stdafx.h"
#pragma hdrstop

#include	"xrsharedmem.h"
#include	"xrMemory_pure.h"
#include    "xrMemory_align.h"
#include	<malloc.h>
// #include    <vld.h>

#pragma		optimize("gyts", off)

xrMemory	Memory;
BOOL		mem_initialized	= FALSE;
bool		shared_str_initialized	= false;

size_t		mapping_mem_usage = 0;
size_t		vm_direct_alloc = 0;

// Processor specific implementations
extern		pso_MemCopy		xrMemCopy_MMX;
extern		pso_MemCopy		xrMemCopy_x86;
extern		pso_MemFill		xrMemFill_x86;
extern		pso_MemFill32	xrMemFill32_MMX;
extern		pso_MemFill32	xrMemFill32_x86;
extern		void			DumpIniFiles(bool);
extern		HANDLE			g_heaps[256];
extern		int				g_chk_heap_id;

#ifdef EXT_MM
extern MEM_ALLOC_CALLBACK		mem_alloc_cb;
extern MEM_REALLOC_CALLBACK		mem_realloc_cb;
extern MEM_FREE_CALLBACK		mem_free_cb;
#endif



#ifdef ANGRY_MM
extern void*	big_pool[1024];
#endif

XRCORE_API void*	XrVirtualAlloc(__in_opt LPVOID lpAddress,  __in SIZE_T dwSize, __in DWORD flAllocationType, __in     DWORD flProtect)
{
	if (flAllocationType & MEM_COMMIT)
		vm_direct_alloc += dwSize;
	return VirtualAlloc (lpAddress, dwSize, flAllocationType, flProtect);
}

XRCORE_API size_t	ext_vm_usage()
{
	return mapping_mem_usage + vm_direct_alloc;
}



DWORD WINAPI VerifyThreadProc(LPVOID lpParam)
{
	// SetThreadAffinityMask(GetCurrentThread(), 0x10);
	// SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#ifdef USE_MEMORY_MONITOR
	while (1 && mem_initialized)		
			__try
			{
				// mem_usage_impl(GetProcessHeap(), 0, 0); 
				for (g_chk_heap_id = 0; g_chk_heap_id < 256; g_chk_heap_id++)
				{
					HANDLE heap = g_heaps[g_chk_heap_id];
					if (!heap) continue;
					mem_usage_impl(heap, 0, 0); // alpet: check heaps, very slowdown		
				}
			}
			__except (SIMPLE_FILTER)
			{
				MsgCB("!#EXCEPTION: catched in mem_usage_impl, g_chk_heap_id = %d", g_chk_heap_id);
			}
#endif
	
	
	return 0;
}


#ifdef DEBUG_MEMORY_MANAGER
XRCORE_API void dump_phase		()
{
	if (!Memory.debug_mode)
		return;

	static int					phase_counter = 0;

	string256					temp;
	sprintf_s					(temp,sizeof(temp),"x:\\$phase$%d.dump",++phase_counter);
	Memory.mem_statistic		(temp);
}
#endif // DEBUG_MEMORY_MANAGER

xrMemory::xrMemory()
#ifdef DEBUG_MEMORY_MANAGER
#	ifdef PROFILE_CRITICAL_SECTIONS
		:debug_cs(MUTEX_PROFILE_ID(xrMemory))
#	endif // PROFILE_CRITICAL_SECTIONS
#endif // DEBUG_MEMORY_MANAGER
{
#ifdef DEBUG_MEMORY_MANAGER

	debug_mode	= FALSE;

#endif // DEBUG_MEMORY_MANAGER
	mem_copy	= xrMemCopy_x86;
	mem_fill	= xrMemFill_x86;
	mem_fill32	= xrMemFill32_x86;
	n_last_alloc = 0;
	mem_fill (&m_last_allocs, 0, sizeof(m_last_allocs));	
	// g_heaps[0]  = GetProcessHeap();

}

#ifdef DEBUG_MEMORY_MANAGER
	BOOL	g_bMEMO		= FALSE;
#endif // DEBUG_MEMORY_MANAGER

void	xrMemory::_initialize	(BOOL bDebug)
{
#ifdef DEBUG_MEMORY_MANAGER
	debug_mode				= bDebug;
	debug_info_update		= 0;
#endif // DEBUG_MEMORY_MANAGER

	stat_calls				= 0;
	stat_counter			= 0;

	u32	features		= CPU::ID.feature & CPU::ID.os_support;
	if (features & _CPU_FEATURE_MMX)
	{
		mem_copy	= xrMemCopy_MMX;
		mem_fill	= xrMemFill_x86;
		mem_fill32	= xrMemFill32_MMX;
	} else {
		mem_copy	= xrMemCopy_x86;
		mem_fill	= xrMemFill_x86;
		mem_fill32	= xrMemFill32_x86;
	}

#ifndef M_BORLAND
	if (!strstr(Core.Params,"-pure_alloc")) {
		// initialize POOLs
		u32	element		= mem_pools_ebase;
		u32 sector		= mem_pools_ebase * 4096;
		for (u32 pid=0; pid<mem_pools_count; pid++)
		{
			mem_pools[pid]._initialize(element, sector, BLOCKHDR);
			element		+=	mem_pools_ebase;
			sector		+= 1024;
			// void *tmp   = mem_pools[pid].create();
			// mem_pools[pid].destroy(tmp);
		}
	}
#endif // M_BORLAND

#ifdef DEBUG_MEMORY_MANAGER
	if (0==strstr(Core.Params,"-memo"))	mem_initialized				= TRUE;
	else								g_bMEMO						= TRUE;
#else // DEBUG_MEMORY_MANAGER
#ifdef EXT_MM
	HMODULE  hLib = GetModuleHandle("luaicp.dll");
	R_ASSERT2(hLib, "Library LUAICP.DLL was not loaded");
	mem_alloc_cb =		(MEM_ALLOC_CALLBACK)   GetProcAddress(hLib, "FastMemAlloc");
	mem_realloc_cb =	(MEM_REALLOC_CALLBACK) GetProcAddress(hLib, "FastMemReAlloc");
	mem_free_cb =		(MEM_FREE_CALLBACK)    GetProcAddress(hLib, "FastMemFree");
#endif

	mem_initialized				= TRUE;
	DWORD tid					= 0;
	CreateThread(NULL, 0x10000, &VerifyThreadProc, NULL, 0, &tid);

#endif // DEBUG_MEMORY_MANAGER

//	DUMP_PHASE;
	g_pStringContainer			= xr_new<str_container>		();
	shared_str_initialized		= true;
//	DUMP_PHASE;
	g_pSharedMemoryContainer	= xr_new<smem_container>	();
//	DUMP_PHASE;

#ifdef VLD_ENABLED
	HMODULE mod = GetModuleHandle("luabind.dll");
	if (!mod) 
		mod = LoadLibrary("luabind.dll");
	if (mod)
		VLDDisableModule(mod);
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(0, path, MAX_PATH);
	wchar_t *tok = wcsstr(path, L"bin\\");
	if (tok) *tok = 0;
	wcscat_s (path, sizeof(path), L"logs\\xray_memory_report.log");
	VLDSetReportOptions (VLD_OPT_REPORT_TO_FILE | VLD_OPT_AGGREGATE_DUPLICATES | VLD_OPT_SAFE_STACK_WALK, path);
#endif

#ifdef ANGRY_MM
	size_t _free, resv, comm;
	vminfo(&_free, &resv, &comm);
	MsgCB("##DEBUG: free_mem before big_pool allocation = %d MiB", _free / 1048576);
	if (_free < (size_t)3 * 1024 * 1024 * 1024) return; // 3GB
	// alpet: захавать гигабайт, чтобы обламались остальные потребители
	for (int i = 0; i < 1024; i++)
	{
		big_pool[i] = VirtualAlloc (NULL, 1048576, MEM_COMMIT, PAGE_READONLY);
	}
	vminfo(&_free, &resv, &comm);
	MsgCB("##DEBUG: free_mem after big_pool allocation = %d MiB", _free / 1048576);

#endif
	// VLDGetReportFilename(path);
}

#ifdef DEBUG_MEMORY_MANAGER
	extern void dbg_dump_leaks();
	extern void dbg_dump_str_leaks();
#endif // DEBUG_MEMORY_MANAGER

void	xrMemory::_destroy()
{
#ifdef DEBUG_MEMORY_MANAGER
	mem_alloc_gather_stats		(false);
	mem_alloc_show_stats		();
	mem_alloc_clear_stats		();
#endif // DEBUG

#ifdef DEBUG_MEMORY_MANAGER
	if (debug_mode)				dbg_dump_str_leaks	();
#endif // DEBUG_MEMORY_MANAGER
	DumpIniFiles		(true);

	extern str_container		verbosity_filters;
	verbosity_filters.clean();

	g_pStringContainer->verify();
	g_pStringContainer->clean();


	xr_delete					(g_pSharedMemoryContainer);
	xr_delete					(g_pStringContainer);

#ifndef M_BORLAND
#	ifdef DEBUG_MEMORY_MANAGER
		if (debug_mode)				dbg_dump_leaks	();
#	endif // DEBUG_MEMORY_MANAGER
#endif // M_BORLAND

	mem_initialized				= FALSE;
#ifdef DEBUG_MEMORY_MANAGER
	debug_mode					= FALSE;
#endif // DEBUG_MEMORY_MANAGER
}

void	xrMemory::mem_compact	()
{
	RegFlushKey						( HKEY_CLASSES_ROOT );
	RegFlushKey						( HKEY_CURRENT_USER );
	_heapmin						( );
	HeapCompact						(GetProcessHeap(),0);
	if (g_pStringContainer)			g_pStringContainer->clean		();
	if (g_pSharedMemoryContainer)	g_pSharedMemoryContainer->clean	();
	if (strstr(Core.Params,"-swap_on_compact"))
		SetProcessWorkingSetSize	(GetCurrentProcess(),size_t(-1),size_t(-1));
}

#ifdef DEBUG_MEMORY_MANAGER
ICF	u8*		acc_header			(void* P)	{	u8*		_P		= (u8*)P;	return	_P-1;	}
ICF	u32		get_header			(void* P)	{	return	(u32)*acc_header(P);				}
void	xrMemory::mem_statistic	(LPCSTR fn)
{
	if (!debug_mode)	return	;
	mem_compact				()	;

	debug_cs.Enter			()	;
	debug_mode				= FALSE;

	FILE*		Fa			= fopen		(fn,"w");
	fprintf					(Fa,"$BEGIN CHUNK #0\n");
	fprintf					(Fa,"POOL: %d %dKb\n",mem_pools_count,mem_pools_ebase);

	fprintf					(Fa,"$BEGIN CHUNK #1\n");
	for (u32 k=0; k<mem_pools_count; ++k)
		fprintf				(Fa,"%2d: %d %db\n",k,mem_pools[k].get_block_count(),(k+1)*16);
	
	fprintf					(Fa,"$BEGIN CHUNK #2\n");
	for (u32 it=0; it<debug_info.size(); it++)
	{
		if (0==debug_info[it]._p)	continue	;

		u32 p_current		= get_header(debug_info[it]._p);
		int pool_id			= (mem_generic==p_current)?-1:p_current;

		fprintf				(Fa,"0x%08X[%2d]: %8d %s\n",*(u32*)(&debug_info[it]._p),pool_id,debug_info[it]._size,debug_info[it]._name);
	}

	{
		for (u32 k=0; k<mem_pools_count; ++k) {
			MEMPOOL			&pool = mem_pools[k];
			u8				*list = pool.list;
			while (list) {
				pool.cs.Enter	();
				u32				temp = *(u32*)(&list);
				if (!temp)
					break;
				fprintf			(Fa,"0x%08X[%2d]: %8d mempool\n",temp,k,pool.s_element);
				list			= (u8*)*pool.access(list);
				pool.cs.Leave	();
			}
		}
	}

	/*
	fprintf					(Fa,"$BEGIN CHUNK #3\n");
	for (u32 it=0; it<debug_info.size(); it++)
	{
		if (0==debug_info[it]._p)	continue	;
		try{
			if (0==strcmp(debug_info[it]._name,"storage: sstring"))
				fprintf		(Fa,"0x%08X: %8d %s %s\n",*(u32*)(&debug_info[it]._p),debug_info[it]._size,debug_info[it]._name,((str_value*)(*(u32*)(&debug_info[it]._p)))->value);
		}catch(...){
		}
	}
	*/

	fclose		(Fa)		;

	// leave
	debug_mode				= TRUE;
	debug_cs.Leave			();

	/*
	mem_compact				();
	LPCSTR					fn	= "$memstat$.tmp";
	xr_map<u32,u32>			stats;

	if (g_pStringContainer)			Msg	("memstat: shared_str: economy: %d bytes",g_pStringContainer->stat_economy());
	if (g_pSharedMemoryContainer)	Msg	("memstat: shared_mem: economy: %d bytes",g_pSharedMemoryContainer->stat_economy());

	// Dump memory stats into file to avoid reallocation while traversing
	{
		IWriter*	F		= FS.w_open(fn);
		F->w_u32			(0);
		_HEAPINFO			hinfo;
		int					heapstatus;
		hinfo._pentry		= NULL;
		while( ( heapstatus = _heapwalk( &hinfo ) ) == _HEAPOK )
			if (hinfo._useflag == _USEDENTRY)	F->w_u32	(u32(hinfo._size));
		FS.w_close			(F);
	}

	// Read back and perform sorting
	{
		IReader*	F		= FS.r_open	(fn);
		u32 size			= F->r_u32	();
		while (!F->eof())
		{
			size						= F->r_u32	();
			xr_map<u32,u32>::iterator I	= stats.find(size);
			if (I!=stats.end())			I->second += 1;
			else						stats.insert(mk_pair(size,1));
		}
		FS.r_close			(F);
		FS.file_delete		(fn);
	}

	// Output to log
	{
		xr_map<u32,u32>::iterator I		= stats.begin();
		xr_map<u32,u32>::iterator E		= stats.end();
		for (; I!=E; I++)	Msg			("%8d : %-4d [%d]",I->first,I->second,I->first*I->second);
	}
	*/
}
#endif // DEBUG_MEMORY_MANAGER

// xr_strdup
char*			xr_strdup		(const char* string)
{	
	VERIFY	(string);
	u32		len			= u32(xr_strlen(string))+1	;
	char *	memory		= (char*)	Memory.mem_alloc( len
#ifdef DEBUG_MEMORY_NAME
		, "strdup"
#endif // DEBUG_MEMORY_NAME
	);
	CopyMemory		(memory,string,len);
	return	memory;
}

XRCORE_API		BOOL			is_stack_ptr		( void* _ptr)
{
	int			local_value		= 0;
	void*		ptr_refsound	= _ptr;
	void*		ptr_local		= &local_value;
	ptrdiff_t	difference		= (ptrdiff_t)_abs(s64(ptrdiff_t(ptr_local) - ptrdiff_t(ptr_refsound)));
	return		(difference < (512*1024));
}
