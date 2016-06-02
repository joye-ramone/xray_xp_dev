#include "stdafx.h"
#pragma hdrstop

#include "xrMemory_align.h"
#include "xrMemory_pure.h"

#ifndef	__BORLANDC__

#ifndef DEBUG_MEMORY_MANAGER
#	define	debug_mode 0
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
	XRCORE_API void*	g_globalCheckAddr = NULL;
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
	extern void save_stack_trace	();
#endif // DEBUG_MEMORY_MANAGER

// #pragma optimize("gyts", off)

#ifdef EXT_MM
MEM_ALLOC_CALLBACK		mem_alloc_cb;
MEM_REALLOC_CALLBACK	mem_realloc_cb;
MEM_FREE_CALLBACK		mem_free_cb;
#endif


MEMPOOL		mem_pools			[mem_pools_count];
HANDLE		g_heaps				[256] = { 0 };

extern		pso_MemFill		xrMemFill_x86;

int		    g_chk_heap_id		= 0;
#define		HEAP_LFH			2	

#ifdef ANGRY_MM
void*		big_pool[1024]		= { 0 };   // alpet: дл€ вы€снени€ кто хавает пам€ть
size_t		free_mem			= 0;	
extern void vminfo(size_t *_free, size_t *reserved, size_t *committed);
#endif

#ifdef USE_MEMORY_MONITOR
string256	g_alloc_names[256] = { 0 };
#endif

// MSVC
ICF	u8*		acc_header			(void* P)	{	u8*		_P		= (u8*)P;	return	_P-1;	}
ICF u8*     hid_header			(void* P)	{	u8*		_P		= (u8*)P;	return	_P-2;	}
ICF	u32		get_header			(void* P)	{	return	(u32)*acc_header(P);				}
ICF	u32		get_pool			(size_t size)
{
	u32		pid					= u32(size/mem_pools_ebase);
	if (pid>=mem_pools_count)	return mem_generic;
	else						return pid;
}

#ifdef PURE_ALLOC
bool	g_use_pure_alloc		= false;
#endif // PURE_ALLOC



void*	xrMemory::mem_alloc		(size_t size
#	ifdef DEBUG_MEMORY_NAME
								 , const char* _name
#	endif // DEBUG_MEMORY_NAME
								 )
{
	stat_calls++;

	// if (n_last_alloc > 100000) mem_usage_impl(GetProcessHeap(), 0, 0); // alpet: check heap, very slowdown		

	_ASSERT (size > 0);
	R_ASSERT2 (size < 0x1fffFFFF, "To large allocation");
#ifdef ANGRY_MM
	if (size > 16384 && free_mem > 0 && free_mem < 10000000)
	{
		for (int i = 0; i < 1024; i++)
			if (big_pool[i])
			{
				VirtualFree (big_pool[i], 0, MEM_RELEASE);
				big_pool[i] = NULL;
				break;
			}
		OutputDebugString("#WARN_MEM_LOW: free_mem < 10MB ");
	}
	if (0 == (stat_calls & 0xfff))
	{
		size_t resv, comm;
		vminfo(&free_mem, &resv, &comm);		
	}
#endif

#ifdef PURE_ALLOC
	static bool g_use_pure_alloc_initialized = false;
	if (!g_use_pure_alloc_initialized) {
		g_use_pure_alloc_initialized	= true;
		g_use_pure_alloc				= 
#	ifdef XRCORE_STATIC
			true
#	else // XRCORE_STATIC
			!!strstr(GetCommandLine(),"-pure_alloc")
#	endif // XRCORE_STATIC
			;
	}

	if (g_use_pure_alloc) {
		void							*result = malloc(size);
#ifdef USE_MEMORY_MONITOR
		memory_monitor::monitor_alloc	(result,size,_name);
#endif // USE_MEMORY_MONITOR
		return							(result);
	}
#endif // PURE_ALLOC

#ifdef DEBUG_MEMORY_MANAGER
	if (mem_initialized)		debug_cs.Enter		();
#endif // DEBUG_MEMORY_MANAGER

	u32		_footer				=	debug_mode ? 4 : OVERHEAD;
	void*	_ptr				=	0;
	void*	_real				=	0;	
#if (BLOCKHDR > 1)
	size_t  pages				=  size / 65536 + 1;
	u8		id_heap				= (u8)pages;
	if (!pages) id_heap			= 1;
	if (pages > 255) id_heap	= 255;
	// s(size > 255 && pages <= 255 ? (u8)pages : 1);
#else
	u8		id_heap				= 0;
#endif
	size_t  _align				=  16;
	// if (size <= 4) _align = 4;

	u32		pool				=	mem_generic;
	HANDLE &heap				=   g_heaps[id_heap];
	
	if (!g_heaps[0])
	{
		// g_heaps[0] = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 1048576, 0);
		g_heaps[0] = GetProcessHeap();
		ULONG HeapInformation = HEAP_LFH;

#if (BLOCKHDR > 1)
		for (int i = 0; i < 256; i++)
		{
			if (!g_heaps[i]) 
				 g_heaps[i] = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 4096 * i, 0);

			R_ASSERT2(g_heaps[i], make_string("Not allocated heap %d, last error = %d", i, GetLastError()));
			MsgCB("##XRMM: g_heaps[%3d] = 0x%p ", i, g_heaps[i]);			
			HeapSetInformation(g_heaps[i],
                               HeapCompatibilityInformation,
                               &HeapInformation,
                               sizeof(HeapInformation));

		}
#endif
	}
	
	R_ASSERT2(heap, "no heap created");
	
	MM_REQUEST				rqs (heap, BLOCKHDR + size + _footer, _align, BLOCKHDR);
	//
	if (!mem_initialized)		
	{
		// generic
		//	Igor: Reserve 1 byte for xrMemory header		
		_real					=	xr_aligned_offset_malloc	(&rqs);
		//void*	_real			=	xr_aligned_offset_malloc	(heap, size + _footer, 16, 0x1);
	} else {
#ifdef DEBUG_MEMORY_MANAGER
		save_stack_trace		();
#endif // DEBUG
		//	accelerated
		//	Igor: Reserve 1 byte for xrMemory header
#ifndef SLOW_MM
		pool				= get_pool(BLOCKHDR + size + _footer);
#endif
		if (mem_generic==pool)	
		{			
			_real				=	xr_aligned_offset_malloc	(&rqs);		
		} else {
			// pooled
			//	Already reserved when getting pool id
#ifdef  EXT_MM
			_real				= mem_alloc_cb(rqs.size);
#else
			_real				= mem_pools[pool].create();
#endif
		}
	}

	_ptr				=	(void*)(((u8*)_real) + BLOCKHDR);
#if (BLOCKHDR >= 8)
	strcpy_s ( (LPSTR)_real, 8, "NBLCK");
	if (!strstr( (LPSTR)_real,  "NBLCK"))
		__asm int 3;
#endif
	*acc_header(_ptr)	=	(u8)pool;			
#if (BLOCKHDR > 1)
	*hid_header(_ptr)	=   id_heap;
#endif
	R_ASSERT (rqs.size >= size);

#ifdef DEBUG_MEMORY_MANAGER
	if		(debug_mode)		dbg_register		(_ptr,size,_name);
	if (mem_initialized)		debug_cs.Leave		();	
#endif // DEBUG_MEMORY_MANAGER
#ifdef USE_MEMORY_MONITOR
	memory_monitor::monitor_alloc	(_ptr,size,_name);
#endif // USE_MEMORY_MONITOR	
	
#if (OVERHEAD >= 2)
	char *tail = (char*)_ptr + size + _footer - 1;
	*tail = (char)0xEF;
	xrMemFill_x86 (_ptr, 0xEE, size + _footer - 1); // DEBUG FILL
#endif

#ifdef USE_MEMORY_MONITOR
	u8 na = n_last_alloc & 0xFF;
	m_last_allocs[na]._dummy = size;
	m_last_allocs[na]._p	 = _real;
	m_last_allocs[na]._name  = (char*)_ptr;
	m_last_allocs[na]._size  = rqs.size;

	size_t len = xr_strlen(_name) + 1;
	strncpy(g_alloc_names[na], _name, len > 256 ? 256 : len);
#endif
	n_last_alloc++;
	// Sleep(10);
	return	_ptr;
}

void	xrMemory::mem_free		(void* P)
{
	stat_calls++;

#ifdef PURE_ALLOC
	if (g_use_pure_alloc) {
		free					(P);
		return;
	}
#endif // PURE_ALLOC

#ifdef DEBUG_MEMORY_MANAGER
	if(g_globalCheckAddr==P)
		__asm int 3;
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
	if (mem_initialized)		debug_cs.Enter		();
#endif // DEBUG_MEMORY_MANAGER
	if		(debug_mode)		dbg_unregister	(P);
	u32	pool					= get_header	(P);
#if (BLOCKHDR > 1)
	u8  id_heap					= *hid_header	(P);
#else
	u8  id_heap					= 0;
#endif

	HANDLE heap					= g_heaps[id_heap];	

	void* _real					= (void*)(((u8*)P) - BLOCKHDR);
	MM_REQUEST					rqs(heap, 0);
	rqs.memblock				= _real;

	if (mem_generic==pool)		
	{
		// generic
		if (!heap)
			Debug.fatal (DEBUG_INFO, "heap %d unassigned, trying free 0x%p ", id_heap, P);
#if (BLOCKHDR >= 8)
		strcpy_s ( (char*)_real, 8, "FBLCK");
#endif		
		xr_aligned_free			(&rqs);
	} else {
		// pooled
		R_ASSERT2 	(pool < mem_pools_count,  make_string("Memory corruption, pool = %d", pool));		
#ifdef  EXT_MM
		mem_free_cb(_real);
#else
		mem_pools[pool].destroy	(_real);
#endif
	}
#ifdef USE_MEMORY_MONITOR
	memory_monitor::monitor_free(P);
#endif // USE_MEMORY_MONITOR

	
#ifdef DEBUG_MEMORY_MANAGER
	if (mem_initialized)		debug_cs.Leave	();
#endif // DEBUG_MEMORY_MANAGER
}

extern BOOL	g_bDbgFillMemory	;

void*	xrMemory::mem_realloc	(void* P, size_t size
#ifdef DEBUG_MEMORY_NAME
								 , const char* _name
#endif // DEBUG_MEMORY_NAME
								 )
{
	stat_calls++;
#ifdef PURE_ALLOC
	if (g_use_pure_alloc) {
		void							*result = realloc(P,size);
#	ifdef USE_MEMORY_MONITOR
		memory_monitor::monitor_free	(P);
		memory_monitor::monitor_alloc	(result,size,_name);
#	endif // USE_MEMORY_MONITOR
		return							(result);
	}
#endif // PURE_ALLOC
	if (0==P) {
		return mem_alloc	(size
#	ifdef DEBUG_MEMORY_NAME
		,_name
#	endif // DEBUG_MEMORY_NAME
		);
	}

#ifdef DEBUG_MEMORY_MANAGER
	if(g_globalCheckAddr==P)
		__asm int 3;
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
	if (mem_initialized)		debug_cs.Enter		();
#endif // DEBUG_MEMORY_MANAGER
	u32		p_current			= get_header(P);
	//	Igor: Reserve 1 byte for xrMemory header	
#ifdef SLOW_MM
	u32	p_new				= mem_generic; 
#else	
	u32		p_new				= get_pool	(BLOCKHDR + size + (debug_mode?4:OVERHEAD));
#endif	
	u32		p_mode				;
	if (mem_generic==p_current)	{
		if (p_new<p_current)		p_mode	= 2	;
		else						p_mode	= 0	;
	} else 							p_mode	= 1	;

	void*	_real				= (void*)(((u8*)P) - BLOCKHDR);
	void*	_ptr				= NULL;
	u32		_footer				= debug_mode ? 4 : OVERHEAD;
#if (BLOCKHDR > 1)
	u8  id_heap					= *hid_header	(P);
#else
	u8  id_heap					= 0;
#endif

	HANDLE heap					= g_heaps[id_heap];
	if (!heap)
	     Debug.fatal (DEBUG_INFO, "heap %d unassigned, realloc 0x%p to size %d", id_heap, P, size);


	MM_REQUEST					rqs (heap, BLOCKHDR + size + _footer, 16, BLOCKHDR);
	rqs.memblock				= _real;

	if		(0==p_mode)
	{
		
#ifdef DEBUG_MEMORY_MANAGER
		if		(debug_mode)	{
			g_bDbgFillMemory	= false;
			dbg_unregister		(P);
			g_bDbgFillMemory	= true;
		}
#endif // DEBUG_MEMORY_MANAGER
		//	Igor: Reserve 1 byte for xrMemory header
		void*	_real2			=	xr_aligned_offset_realloc	(&rqs);
		//void*	_real2			=	xr_aligned_offset_realloc	(_real,size+_footer,16,0x1);
#if (BLOCKHDR >= 8)
		strcpy_s ( (char*)_real2, 8, "RBLCK");
#endif		
		_ptr					= (void*)(((u8*)_real2) + BLOCKHDR);
		*acc_header(_ptr)		= mem_generic;
		*hid_header(_ptr)		= id_heap;
#ifdef DEBUG_MEMORY_MANAGER
		if		(debug_mode)	dbg_register	(_ptr,size,_name);
#endif // DEBUG_MEMORY_MANAGER
#ifdef USE_MEMORY_MONITOR
		memory_monitor::monitor_free	(P);
		memory_monitor::monitor_alloc	(_ptr,size,_name);
#endif // USE_MEMORY_MONITOR
	} else if (1==p_mode)		{
		// pooled realloc
		R_ASSERT2				(p_current<mem_pools_count,"Memory corruption");
		u32		s_current		= mem_pools[p_current].get_element();
		u32		s_dest			= (u32)size;
		void*	p_old			= P;
		void*	p_new			= mem_alloc(size
#ifdef DEBUG_MEMORY_NAME
			,_name
#endif // DEBUG_MEMORY_NAME
		);
		R_ASSERT2(p_new, make_string("cannot realloc block %p to size %d", p_old, size));

		//	Igor: Reserve 1 byte for xrMemory header
		//	Don't bother in this case?
		mem_copy				(p_new,p_old,_min(s_current - BLOCKHDR, s_dest));
		//mem_copy				(p_new,p_old,_min(s_current,s_dest));
		mem_free				(p_old);
		_ptr					= p_new;
	} else if (2==p_mode)		{
		// relocate into another mmgr(pooled) from real
		void*	p_old			= P;
		void*   p_new = 0;
		try
		{			
			p_new			= mem_alloc(size
#	ifdef DEBUG_MEMORY_NAME
				,_name
#	endif // DEBUG_MEMORY_NAME
		);
			// R_ASSERT2(p_new, make_string("cannot realloc block %p to size %d", p_old, size));
		
			mem_copy(p_new, p_old, (u32)size);
			mem_free(p_old);
			_ptr					= p_new;
		}
		catch(...)
		{
			Msg("!#EXCEPTION: mem_realloc p_old = %p, p_new = %p, size = %d ", p_old, p_new, size);
		}
		
	}

#ifdef DEBUG_MEMORY_MANAGER
	if (mem_initialized)		debug_cs.Leave	();

	if(g_globalCheckAddr==_ptr)
		__asm int 3;
#endif // DEBUG_MEMORY_MANAGER

	return	_ptr;
}

#endif // __BORLANDC__