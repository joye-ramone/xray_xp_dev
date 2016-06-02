#ifndef xrMemory_alignH
#define xrMemory_alignH
#pragma once

#define     BLOCKHDR			2
#define		OVERHEAD			2

typedef struct _MM_REQUEST
{
	HANDLE		heap;
	size_t		size;
	size_t		align;
	size_t		offset;
	void*		memblock;

	ICF _MM_REQUEST(HANDLE _heap, size_t _size, size_t _align = 16, size_t _offset = 0)
	{
		heap = _heap; size = _size; align = _align; offset = _offset;
	}

} MM_REQUEST, *PMM_REQUEST;


u32		__stdcall xr_aligned_msize(HANDLE, void *);
void    __stdcall xr_aligned_free(PMM_REQUEST);
// void *  __stdcall xr_aligned_malloc(HANDLE, size_t, size_t, size_t&);
void *  __stdcall xr_aligned_offset_malloc(PMM_REQUEST);
// void *  __stdcall xr_aligned_realloc(HANDLE, void *, size_t, size_t, size_t&);
void *  __stdcall xr_aligned_offset_realloc(PMM_REQUEST);

#endif