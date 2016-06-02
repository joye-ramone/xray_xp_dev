#include "stdafx.h"
#pragma hdrstop

#include <errno.h>
#include <malloc.h>
#include "xrMemory_align.h"

/***
*align.c - Aligned allocation, reallocation or freeing of memory in the heap
*
*       Copyright (c) 1989-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _aligned_malloc(),
*                   _aligned_realloc(),
*                   _aligned_offset_malloc(),
*                   _aligned_offset_realloc() and
*                   _aligned_free() functions.
*
*******************************************************************************/

#define IS_2_POW_N(X)   (((X)&(X-1)) == 0)
#define PTR_SZ          sizeof(void *)
// #pragma optimize("gyts", off)

#ifdef __BORLANDC__
	typedef _W64 unsigned int  uintptr_t;
#endif
/***
*
* |1|___6___|2|3|4|_________5__________|_6_|
*
* 1 -> Pointer to start of the block allocated by malloc.
* 2 -> Value of 1.
* 3 -> Gap used to get 1 aligned on sizeof(void *).
* 4 -> Pointer to the start of data block.
* 4+5 -> Data block.
* 6 -> Wasted memory at rear of data block.
* 6 -> Wasted memory.
*
*******************************************************************************/

/***
* void * xr_aligned_malloc(size_t size, size_t alignment)
*       - Get a block of aligned memory from the heap.
*
* Purpose:
*       Allocate of block of aligned memory aligned on the alignment of at least
*       size bytes from the heap and return a pointer to it.
*
* Entry:
*       size_t size - size of block requested
*       size_t alignment - alignment of memory
*
* Exit:
*       Sucess: Pointer to memory block
*       Faliure: Null
*******************************************************************************/

void * __stdcall xr_aligned_malloc( HANDLE heap,
									size_t size,
									size_t alignment
									)
{
	MM_REQUEST				rqs (heap, size, alignment);
	return xr_aligned_offset_malloc(&rqs);
}

/***
* void *xr_aligned_offset_malloc(size_t size, size_t alignment, int offset)
*       - Get a block of memory from the heap.
*
* Purpose:
*       Allocate a block of memory which is shifted by offset from alignment of
*       at least size bytes from the heap and return a pointer to it.
*
* Entry:
*       size_t size - size of block of memory
*       size_t alignment - alignment of memory
*       size_t offset - offset of memory from the alignment
*
* Exit:
*       Sucess: Pointer to memory block
*       Faliure: Null
*
*******************************************************************************/

#define HEAP_FLAGS  HEAP_GENERATE_EXCEPTIONS
#define PTR_MASK    (PTR_SZ-1)


IC uintptr_t stub_malloc(HANDLE heap, size_t size)
{	
	void *result = 0;
	R_ASSERT2(HeapLock(heap), "Cannot lock heap");
	__try {
		result = HeapAlloc(heap, HEAP_FLAGS, size);	}
	__finally {
		HeapUnlock(heap); }
	return (uintptr_t) result;
}

IC uintptr_t stub_realloc(HANDLE heap, void *p, size_t new_size)
{
	void *result = 0;
	R_ASSERT2(HeapLock(heap), "Cannot lock heap");
	__try {
		result = HeapReAlloc(heap, HEAP_FLAGS, p, new_size); }
	__finally {
		HeapUnlock(heap); }
	return (uintptr_t)result;
}

IC void stub_free(HANDLE heap, void *p)
{	
	R_ASSERT2(HeapLock(heap), "Cannot lock heap");
	__try {
		HeapFree(heap, HEAP_FLAGS, p); 	}
	__finally {
		HeapUnlock(heap); }	
}

IC size_t stub_msize(HANDLE heap, void *p)
{
	R_ASSERT2(HeapLock(heap), "Cannot lock heap");
	__try {
		size_t result = (size_t)HeapSize(heap, 0, p);
		return result;
	}
	__finally {
		HeapUnlock(heap); }
	return 0;
}



void * __stdcall xr_aligned_offset_malloc (PMM_REQUEST  rqs)
{
	uintptr_t ptr, retptr, gap;

	if (!IS_2_POW_N(rqs->align))
	{
		errno = EINVAL;
		return NULL;
	}
	if ( rqs->offset >= rqs->size && rqs->offset != 0)
		 rqs->size	= rqs->offset+1;

	size_t align = (rqs->align > PTR_SZ ? rqs->align : PTR_SZ) -1;

	/* gap = number of bytes needed to round up offset to align with PTR_SZ*/
	gap = (0 - rqs->offset) & PTR_MASK;

	size_t last_alloc = PTR_SZ + gap + align + rqs->size;	
	R_ASSERT (last_alloc > rqs->size);
	ptr = 0;
	try
	{
		ptr = stub_malloc(rqs->heap, last_alloc);
		if (ptr == (uintptr_t)NULL)
		{
			string256 msg;
			strcpy_s(msg, 255, " Failed allocate memory block, size = ");
			size_t l = strlen(msg);
			itoa(last_alloc, &msg[l], 10);
			OutputDebugString(msg);
			return NULL;
		}

	}
	catch (...)
	{
		Msg("!#EXCEPTION: attempt to allocate %d bytes ", last_alloc);
	}
	if (ptr < 0x10000)
	{		
		// Sleep(50);		
		mem_usage_impl(rqs->heap, 0, 0); // alpet: check heaps, very slowdown		
		Debug.fatal(DEBUG_INFO, "!#FATAL: bad pointer for allocated block = 0x%08p,  requested size = %d, heap = 0x%8x ", ptr, last_alloc, rqs->heap);
		return NULL;
	}

	retptr =((ptr +PTR_SZ +gap +align + rqs->offset ) & ~align)- rqs->offset;
	((uintptr_t *)(retptr - gap))[-1] = ptr;

	rqs->size = last_alloc;
	return (void *)retptr;
}

/***
*
* void * xr_aligned_realloc(void * memblock, size_t size, size_t alignment)
*       - Reallocate a block of aligned memory from the heap.
*
* Purpose:
*       Reallocates of block of aligned memory aligned on the alignment of at
*       least size bytes from the heap and return a pointer to it. Size can be
*       either greater or less than the original size of the block.
*       The reallocation may result in moving the block as well as changing the
*       size.
*
* Entry:
*       void *memblock - pointer to block in the heap previously allocated by
*               call to _aligned_malloc(), _aligned_offset_malloc(),
*               _aligned_realloc() or _aligned_offset_realloc().
*       size_t size - size of block requested
*       size_t alignment - alignment of memory
*
* Exit:
*       Sucess: Pointer to re-allocated memory block
*       Faliure: Null
*
*******************************************************************************/

void * __stdcall xr_aligned_realloc( HANDLE heap, 
									 void *memblock,
									 size_t size,
									 size_t alignment
									 )
{
	MM_REQUEST					rqs (heap, size, alignment, 0);
	rqs.memblock				= memblock;
	return xr_aligned_offset_realloc(&rqs);
}


/***
*
* void *xr_aligned_offset_realloc (void * memblock, size_t size,
*                                     size_t alignment, int offset)
*       - Reallocate a block of memory from the heap.
*
* Purpose:
*       Reallocates a block of memory which is shifted by offset from
*       alignment of at least size bytes from the heap and return a pointer
*       to it. Size can be either greater or less than the original size of the
*       block.
*
* Entry:
*       void *memblock - pointer to block in the heap previously allocated by
*               call to _aligned_malloc(), _aligned_offset_malloc(),
*               _aligned_realloc() or _aligned_offset_realloc().
*       size_t size - size of block of memory
*       size_t alignment - alignment of memory
*       size_t offset - offset of memory from the alignment
*
* Exit:
*       Sucess: Pointer to the re-allocated memory block
*       Faliure: Null
*
*******************************************************************************/

void * __stdcall xr_aligned_offset_realloc(PMM_REQUEST rqs)
{
	uintptr_t ptr, retptr, gap, stptr, diff;
	uintptr_t movsz, reqsz;
	int bFree = 0;

	if (rqs->memblock == NULL)
	{
		return xr_aligned_offset_malloc(rqs);
	}
	if (rqs->size == 0)
	{
		xr_aligned_free(rqs);
		return NULL;
	}
	if ( rqs->offset >= rqs->size && rqs->offset != 0)
	{
		errno = EINVAL;
		return NULL;
	}

	stptr = (uintptr_t)rqs->memblock;

	/* ptr points to the pointer to starting of the memory block */
	stptr = (stptr & ~PTR_MASK) - PTR_SZ;

	/* ptr is the pointer to the start of memory block*/
	stptr = *((uintptr_t *)stptr);

	size_t align = rqs->align;

	if (!IS_2_POW_N(align))
	{
		errno = EINVAL;
		return NULL;
	}

	align = (align > PTR_SZ ? align : PTR_SZ) -1;
	/* gap = number of bytes needed to round up offset to align with PTR_SZ*/
	gap = (0 - rqs->offset) & PTR_MASK;

	diff = (uintptr_t)rqs->memblock - stptr;
	/* Mov size is min of the size of data available and sizw requested.
	*/
	movsz = stub_msize(rqs->heap, (void *)stptr) - ((uintptr_t)rqs->memblock - stptr);
	movsz = (movsz > rqs->size) ? rqs->size : movsz;
	reqsz = PTR_SZ +gap +align + rqs->size;

	/* First check if we can expand(reducing or expanding using expand) data
	* safely, ie no data is lost. eg, reducing alignment and keeping size
	* same might result in loss of data at the tail of data block while
	* expanding.
	*
	* If no, use malloc to allocate the new data and move data.
	*
	* If yes, expand and then check if we need to move the data.
	*/
	if ((stptr +align +PTR_SZ +gap)<(uintptr_t)rqs->memblock)
	{
		ptr = stub_malloc(rqs->heap, reqsz);
		if (ptr == (uintptr_t) NULL)
			return NULL;
		rqs->size = reqsz;
		bFree = 1;
	}
	else
	{
		// ptr = (uintptr_t)_expand((void *)stptr, reqsz);
		ptr = stub_realloc(rqs->heap, (void*)stptr, reqsz);


		if (ptr == (uintptr_t)NULL)
		{
			ptr = stub_malloc(rqs->heap, reqsz);
			if (ptr == (uintptr_t) NULL)
				return NULL;
			bFree = 1;
			rqs->size = reqsz;
		}
		else
			stptr = ptr;
	}


	if ( ptr == ((uintptr_t)rqs->memblock - diff)
		&& !( ((size_t)rqs->memblock + gap +rqs->offset) & ~(align) ))
	{
		return rqs->memblock;
	}

	retptr =((ptr +PTR_SZ + gap + align + rqs->offset) & ~align) - rqs->offset;
	memmove((void *)retptr, (void *)(stptr + diff), movsz);
	if ( bFree)
		stub_free (rqs->heap, (void *)stptr);

	((uintptr_t *)(retptr - gap))[-1] = ptr;
	return (void *)retptr;
}


/***
*
* void *xr_aligned_free(void *memblock)
*       - Free the memory which was allocated using _aligned_malloc or
*       _aligned_offset_memory
*
* Purpose:
*       Frees the aligned memory block which was allocated using _aligned_malloc
*       or _aligned_memory.
*
* Entry:
*       void * memblock - pointer to the block of memory
*
*******************************************************************************/

void __stdcall xr_aligned_free(PMM_REQUEST rqs)
{
	uintptr_t ptr;

	if (rqs->memblock == NULL)
		return;

	ptr = (uintptr_t)rqs->memblock;

	/* ptr points to the pointer to starting of the memory block */
	ptr = (ptr & ~PTR_MASK) - PTR_SZ;

	/* ptr is the pointer to the start of memory block*/
	ptr = *((uintptr_t *)ptr);
	try
	{
		stub_free(rqs->heap, (void *)ptr);
	}
	catch (...)
	{
		MsgCB("!#EXCEPTION: catched in xr_aligned_free, ptr = 0x%p ", ptr);
	}
}

u32 __stdcall xr_aligned_msize(HANDLE heap, void *memblock)
{
	uintptr_t ptr;

	if (memblock == NULL)
		return	0;

	ptr = (uintptr_t)memblock;

	/* ptr points to the pointer to starting of the memory block */
	ptr = (ptr & ~PTR_MASK) - PTR_SZ;

	/* ptr is the pointer to the start of memory block*/
	ptr = *((uintptr_t *)ptr);
	return	(u32)	stub_msize	(heap, (void *)ptr);
}
