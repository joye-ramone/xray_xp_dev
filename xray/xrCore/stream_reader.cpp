#include "stdafx.h"
#include "FS64.h"
#include "stream_reader.h"

extern size_t		mapping_mem_usage;

void CStreamReader::construct				(
		const HANDLE &file_mapping_handle,
		const file_ptr  &start_offset,
		const file_size &fsize,
		const file_size &archive_size,
		const u32		&window_size
	)
{
	m_file_mapping_handle		= file_mapping_handle;
	m_start_offset				= start_offset;
	m_file_size					= fsize;
	m_archive_size				= archive_size;
	m_window_size				= _max(window_size,FS.dwAllocGranularity);

	map							(0);
}

void CStreamReader::destroy					()
{
	unmap						();
}

void CStreamReader::map						(const u64 &new_offset)
{
	VERIFY						(new_offset <= m_file_size);
	m_current_offset_from_start	= new_offset;

	u64							granularity = FS.dwAllocGranularity;
	u64							start_offset = m_start_offset + new_offset;
	u64							pure_start_offset = start_offset;
	start_offset				= (start_offset/granularity)*granularity;

	VERIFY						(pure_start_offset >= start_offset);
	u64							pure_end_offset = m_window_size + pure_start_offset;
	u64							end_offset = pure_end_offset/granularity;
	if (pure_end_offset%granularity)
		++end_offset;

	end_offset					*= granularity;
	if (end_offset > m_archive_size)
		end_offset				= m_archive_size;
	
	m_current_window_size		= (u32) (end_offset - start_offset);	
	m_current_map_view_of_file	= (u8*)
		MapViewOfFile(
			m_file_mapping_handle,
			FILE_MAP_READ,
			HIDWORD(start_offset),
			LODWORD(start_offset),
			m_current_window_size
		);
	m_current_pointer			= m_current_map_view_of_file;
	if (m_current_pointer)
		mapping_mem_usage += m_current_window_size;

	u32							difference = (u32)(pure_start_offset - start_offset);
	m_current_window_size		-= difference;
	m_current_pointer			+= difference;
	m_start_pointer				= m_current_pointer;
}

void CStreamReader::advance(const s32 &offset)
{
	advance64(offset);
}

void CStreamReader::advance64				(const s64 &offset)
{
	VERIFY						(m_current_pointer >= m_start_pointer);
	VERIFY						(u32(m_current_pointer - m_start_pointer) <= m_current_window_size);
	int							offset_inside_window = int(m_current_pointer - m_start_pointer);
	if (offset_inside_window + offset >= (int)m_current_window_size) {
		remap					(m_current_offset_from_start + offset_inside_window + offset);
		return;
	}

	if (offset_inside_window + offset < 0) {
		remap					(m_current_offset_from_start + offset_inside_window + offset);
		return;
	}

	m_current_pointer			+= offset;
}

void CStreamReader::r						(void *_buffer, u32 buffer_size)
{
	VERIFY						(m_current_pointer >= m_start_pointer);
	VERIFY						(u32(m_current_pointer - m_start_pointer) <= m_current_window_size);

	int							offset_inside_window = int(m_current_pointer - m_start_pointer);
	if (offset_inside_window + buffer_size < m_current_window_size) {
		Memory.mem_copy			(_buffer,m_current_pointer,buffer_size);
		m_current_pointer		+= buffer_size;
		return;
	}

	u8							*buffer = (u8*)_buffer;
	u32							elapsed_in_window = m_current_window_size - (m_current_pointer - m_start_pointer);

	do {
		Memory.mem_copy			(buffer,m_current_pointer,elapsed_in_window);
		buffer					+= elapsed_in_window;
		buffer_size				-= elapsed_in_window;
		advance64				(elapsed_in_window);

		elapsed_in_window		= m_current_window_size;
	}
	while (m_current_window_size < buffer_size);

	Memory.mem_copy				(buffer,m_current_pointer,buffer_size);
	advance64					(buffer_size);
}

bool CStreamReader::chunk_at(u32 ID, s64 offset, u32 max_diff)
{
	if (offset < 0)
		offset = next_chunk_pos;

	if (offset < 0 || offset + 8 >= (s64) length64()) return false;

	seek64(offset);
	while (elapsed64() > 8)
	{
		u32 dwType = r_u32() & (~CFS_CompressMark);
		u32 dwSize = r_u32();
		if (dwSize && dwType == ID )
		{
			advance64(-8);
			return true;
		}			
		if (!max_diff--) break;			
	}
	return false;
}

void CStreamReader::remember_chunk(const s64 offset)
{
	// nothing here, but need?
}


CStreamReader *CStreamReader::open_chunk	(const u32 &chunk_id, s64 pos)
{
	if (pos)
		set_next_chunk(pos);
	if (next_chunk_pos < 0 || next_chunk_pos + 8 > length64())
		set_next_chunk(0);

	BOOL						compressed;
	u32							size = find_chunk(chunk_id,&compressed);
	if (!size)
		return					(0);

	R_ASSERT2					(!compressed,"cannot use CStreamReader on compressed chunks");
	CStreamReader				*result = xr_new<CStreamReader>();
	result->construct			(file_mapping_handle(), m_start_offset + tell64(), size, m_archive_size, m_window_size);
	return						(result);
}
