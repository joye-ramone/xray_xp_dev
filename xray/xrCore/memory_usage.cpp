#include "stdafx.h"
#include <malloc.h>
#include <errno.h>

#pragma optimize("gyts", off)

extern size_t mapping_mem_usage;
extern size_t vm_direct_alloc;
extern HANDLE g_fs_heap;

volatile ULONG   g_free_vm = 0; 	
volatile ULONG   g_used_vm = 0;

void VMInfoUpdater(void *param)
{
	size_t _free, reserved, committed;

	while (g_free_vm != (ULONG) -1)
	{		
		vminfo(&_free, &reserved, &committed);		
		InterlockedExchange(&g_free_vm, _free);
		InterlockedExchange(&g_used_vm, committed);
		Sleep(1000);
	}
	g_free_vm = 0;

}


XRCORE_API void vminfo (size_t *_free, size_t *reserved, size_t *committed) {
	if (!reserved)
	{
		if (0 == g_used_vm)
		{
			g_used_vm = 1;
			thread_spawn(VMInfoUpdater, "VMInfoUpdater", 0, 0);
			Sleep(50);
		}
		*_free = g_free_vm;
		*committed = g_used_vm;
		return;
	}
	if (!_free)
	{
		g_free_vm = (ULONG)-1;
		return;
	}


	MEMORY_BASIC_INFORMATION memory_info;
	memory_info.BaseAddress = 0;
	*_free = *reserved = *committed = 0;
	while (VirtualQuery (memory_info.BaseAddress, &memory_info, sizeof (memory_info))) {
		switch (memory_info.State) {
		case MEM_FREE:
			*_free		+= memory_info.RegionSize;
			break;
		case MEM_RESERVE:
			*reserved	+= memory_info.RegionSize;
			break;
		case MEM_COMMIT:
			*committed += memory_info.RegionSize;
			break;
		}
		memory_info.BaseAddress = (char *) memory_info.BaseAddress + memory_info.RegionSize;
	}
}

extern bool shared_str_initialized;

XRCORE_API void log_vminfo	()
{
	static size_t last_free = 0, last_reserved = 0, last_committed = 0, last_mapping = 0;

	size_t  w_free, w_reserved, w_committed;
	vminfo	(&w_free, &w_reserved, &w_committed);
	if (shared_str_initialized)
	{
		Msg("* [win32]:   free[%8d K], reserved[%8d K], committed[%8d K], mapped[%7d K], VM direct[%d K]",
			w_free / 1024, w_reserved / 1024, w_committed / 1024, mapping_mem_usage / 1024, vm_direct_alloc / 1024
			);
		Msg("* [changes]: free[%+8d K], reserved[%+8d K], committed[%+8d K], mapped[%+7d K]",
			int(w_free - last_free) / 1024,			    int(w_reserved - last_reserved) / 1024,
			int(w_committed - last_committed) / 1024, 	int(mapping_mem_usage - last_mapping) / 1024);				
	
	}
	else
	{
		MsgCB(
			"*#MEM_USAGE: [win32 VM]: free[%d K], reserved[%d K], committed[%d K], mapped[%d K], VM direct[%d K]",
			w_free / 1024, w_reserved / 1024, w_committed / 1024, mapping_mem_usage / 1024, vm_direct_alloc / 1024
			);
	}
	last_free		= w_free;
	last_reserved	= w_reserved;
	last_committed  = w_committed;
	last_mapping	= mapping_mem_usage;
}

int heap_walk (
	    HANDLE heap_handle,
        struct _heapinfo *_entry
        )
{
        PROCESS_HEAP_ENTRY Entry;
        DWORD errval;
        int errflag;
        int retval = _HEAPOK;

        Entry.wFlags = 0;
        Entry.iRegionIndex = 0;
		Entry.cbData = 0;
        if ( (Entry.lpData = _entry->_pentry) == NULL ) {
            if ( !HeapWalk( heap_handle, &Entry ) ) {
                if ( GetLastError() == ERROR_CALL_NOT_IMPLEMENTED ) {
                    _doserrno = ERROR_CALL_NOT_IMPLEMENTED;
                    errno = ENOSYS;
                    return _HEAPEND;
                }
                return _HEAPBADBEGIN;
            }
        }
        else {
            if ( _entry->_useflag == _USEDENTRY ) {
                if ( !HeapValidate( heap_handle, 0, _entry->_pentry ) )
                    return _HEAPBADNODE;
                Entry.wFlags = PROCESS_HEAP_ENTRY_BUSY;
            }
nextBlock:
            /*
             * Guard the HeapWalk call in case we were passed a bad pointer
             * to an allegedly free block.
             */
            __try {
                errflag = 0;
                if ( !HeapWalk( heap_handle, &Entry ) )
                    errflag = 1;
            }
            __except( EXCEPTION_EXECUTE_HANDLER ) {
                errflag = 2;
            }

            /*
             * Check errflag to see how HeapWalk fared...
             */
            if ( errflag == 1 ) {
                /*
                 * HeapWalk returned an error.
                 */
                if ( (errval = GetLastError()) == ERROR_NO_MORE_ITEMS ) {
                    return _HEAPEND;
                }
                else if ( errval == ERROR_CALL_NOT_IMPLEMENTED ) {
                    _doserrno = errval;
                    errno = ENOSYS;
                    return _HEAPEND;
                }
                return _HEAPBADNODE;
            }
            else if ( errflag == 2 ) {
                /*
                 * Exception occurred during the HeapWalk!
                 */
                return _HEAPBADNODE;
            }
        }

        if ( Entry.wFlags & (PROCESS_HEAP_REGION |
             PROCESS_HEAP_UNCOMMITTED_RANGE) )
        {
            goto nextBlock;
        }

        _entry->_pentry = (int*)Entry.lpData;
        _entry->_size = Entry.cbData;
        if ( Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY ) {
            _entry->_useflag = _USEDENTRY;
        }
        else {
            _entry->_useflag = _FREEENTRY;
        }

        return( retval );
}

XRCORE_API u32	mem_usage_impl	(HANDLE heap_handle, u32* pBlocksUsed, u32* pBlocksFree)
{
#ifdef USE_MEMORY_MONITOR
	extern string256	g_alloc_names[256];
#endif
	extern HANDLE g_heaps[256];
	size_t	total = 0;
	size_t heap_size = 0;
	
	if ( (u32)heap_handle < 16)
	{
		if (0 == (u32)heap_handle)
		for (int i = 1; i < 256; i++)
		  if (g_heaps[i])				
			{ 
				size_t heap_size = mem_usage_impl(g_heaps[i], 0, 0);
				total += heap_size;
				if (heap_size > 64 * 1048576)
					MsgCB("##MEM_USAGE: g_heap[%d] usage = %.3f MiB", i, (float)heap_size / 1048576.f);

			}

		if (2 == (u32)heap_handle)
		{
			if (g_fs_heap)
				return mem_usage_impl(g_fs_heap, 0, 0);
			else
				return 0;
		}


		HANDLE all_heaps[512];
		DWORD cnt = GetProcessHeaps(512, &all_heaps[0]);

		if (1 == (u32)heap_handle)
			for (DWORD h = 0; h < cnt; h++)
			{
				bool found = (all_heaps[h] == g_fs_heap);
				if (!found)
				for (int i = 0; i < 256; i++)
					if (all_heaps[h] == g_heaps[i])
					{
						found = true; break;
					}
				if (!found)
				{
					HANDLE heap = all_heaps[h];
					size_t heap_size = mem_usage_impl(heap, 0, 0);
					total += heap_size;
					if (heap_size > 8 * 1048576)
					{   // обычно обнаруживаются кучи Direct3D для текстур и прочего
						MsgCB("##MEM_USAGE: unregistered heap 0x%x found, usage = %.3f MiB ", heap, (float)heap_size / 1048576.f);
						// HeapDestroy(heap_handle); // варварство!
					}
				}

			}

		return total;
	}

	
	BOOL locked = HeapLock (heap_handle);
	R_ASSERT(locked);

	__try
	{

		extern		int				g_chk_heap_id;

		_HEAPINFO		hinfo;
		int				heapstatus;
		hinfo._pentry = NULL;		
		u32	blocks_free = 0;
		u32	blocks_used = 0;
		void *prev_entry = NULL;
		void *curr_entry = NULL;

		while ((heapstatus = heap_walk(heap_handle, &hinfo)) == _HEAPOK)
		{
			if (hinfo._useflag == _USEDENTRY)	{
				total += hinfo._size;
				blocks_used += 1;
			}
			else {
				blocks_free += 1;
			}
			prev_entry = curr_entry;
			curr_entry = hinfo._pentry;
		}
		if (pBlocksFree)	*pBlocksFree = 1024 * (u32)blocks_free;
		if (pBlocksUsed)	*pBlocksUsed = 1024 * (u32)blocks_used;

		switch (heapstatus)
		{
		case _HEAPEMPTY:
			break;
		case _HEAPEND:
			break;
		case _HEAPBADPTR:
			MsgCB("!#FATAL: bad pointer to heap");
			break;
		case _HEAPBADBEGIN:
			// FATAL			("bad start of heap");
			MsgCB("!#FATAL: bad start of heap, blocks_free = %d, blocks_used = %d", blocks_free, blocks_used);
			break;
		case _HEAPBADNODE:
			MsgCB("!#FATAL: bad node in heap %d, n_last_alloc = %d, pentry = 0x%p, prev = 0x%p, blocks_free = %d, blocks_used = %d ",
				g_chk_heap_id, Memory.n_last_alloc, hinfo._pentry, prev_entry, blocks_free, blocks_used);
			size_t sz_curr = _msize(curr_entry);
			size_t sz_prev = _msize(prev_entry);
			MsgCB(" curr_size = %d, prev_size = %d", sz_curr, sz_prev);
			for (int i = 0; i < 256; i++)
			{
				auto &la = Memory.m_last_allocs[i];
#ifdef USE_MEMORY_MONITOR
				MsgCB(" m_last_allocs[%.3d] = 0x%.8p, 0x%.8p, %7d, %5d, %s ", i, la._p, la._name, la._size, la._dummy, g_alloc_names[i]);
#else
				MsgCB(" m_last_allocs[%.3d] = 0x%.8p, 0x%.8p, %7d, %d ", i, la._p, la._name, la._size, la._dummy);
#endif
			}

			Sleep(1500);
			locked = FALSE;
			HeapUnlock(heap_handle);
			FATAL("bad node in heap");
			break;
		}

	}
	__finally
	{
		if (locked) HeapUnlock(heap_handle);
	}
	return (u32) total;
}

u32		xrMemory::mem_usage		(u32* pBlocksUsed, u32* pBlocksFree)
{
	size_t  w_free, w_reserved, w_committed;
	vminfo	(&w_free, &w_reserved, &w_committed);
	if (pBlocksUsed || pBlocksFree)
		return				(mem_usage_impl((HANDLE)_get_heap_handle(), pBlocksUsed, pBlocksFree));
	else
		return w_committed;
}
