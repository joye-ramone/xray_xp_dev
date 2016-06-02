#include "stdafx.h"
#pragma hdrstop

#include "xrMemory_POOL.h"
#include "xrMemory_align.h"

#pragma optimize("gyts", off)

extern HANDLE			g_heaps[256];

MEMPOOL::~MEMPOOL()
{
	MM_REQUEST				rqs (g_heaps[4], 0);
	rqs.memblock = list;
	if (rqs.memblock)
		xr_aligned_free(&rqs);
}

void	MEMPOOL::block_create	()
{
	// Allocate
	R_ASSERT				(0==list);	
	MM_REQUEST				rqs (g_heaps[4], s_sector, 16, 0);

	list					= (u8*)		xr_aligned_offset_malloc	(&rqs);
	MsgCB("##MM_OPT: block_create allocated %7d bytes for elements with size %3d, block count = %d ", rqs.size, s_element, block_count);

	// Partition
	// каждые s_element байт прописывается указатель на следующий блок в пуле
	for (u32 it=0; it<(s_count-1); it++)
	{
		u8*	E				= list + it * s_element;
		*access(E)			= E + s_element;
	}
	list_end = list + (s_count - 1) * s_element;
	*access(list_end) = NULL;
	block_count				++;
}

void	MEMPOOL::_initialize	(u32 _element, u32 _sector, u32 _header)
{
	R_ASSERT		(_element < _sector/2);
	s_sector		= _sector;
	s_element		= _element;
	s_count			= s_sector/s_element;
	s_offset		= _header;
	list			= NULL;
	block_count		= 0;
}

void   MEMPOOL::release(void *P)
{
	*access(P)		= list;
	list			= (u8*)P; // last freed now list entry
	// == list_end ? NULL : list + s_element;
}
