#ifndef STREAM_READER_INLINE_H
#define STREAM_READER_INLINE_H

extern size_t mapping_mem_usage;

IC	CStreamReader::CStreamReader						()
{
}

IC	CStreamReader::CStreamReader						(const CStreamReader &object) :
	m_start_offset	(object.m_start_offset),
	m_file_size		(object.m_file_size),
	m_archive_size	(object.m_archive_size),
	m_window_size	(object.m_window_size)
{
	// should be never called
}

IC	CStreamReader &CStreamReader::operator=				(const CStreamReader &)
{
	// should be never called
	return			(*this);
}

IC	const HANDLE &CStreamReader::file_mapping_handle	() const
{
	return			(m_file_mapping_handle);
}

IC	void CStreamReader::unmap							()
{
	UnmapViewOfFile	(m_current_map_view_of_file);
	mapping_mem_usage -= m_current_window_size;
}

IC	void CStreamReader::remap							(const u64 &new_offset)
{
	unmap			();
	map				(new_offset);
}

IC	u64 CStreamReader::elapsed64						()	const
{
	u64				offset_from_file_start = tell();
	VERIFY			(m_file_size >= offset_from_file_start);
	return			(m_file_size - offset_from_file_start);
}

IC	const u64 &CStreamReader::length64					() const
{
	return			(m_file_size);
}

IC	void CStreamReader::seek64							(const s64 &offset)
{
	advance64		(offset - tell());
}

IC	u64 CStreamReader::tell64								() const
{
	VERIFY			(m_current_pointer >= m_start_pointer);
	VERIFY			(u32(m_current_pointer - m_start_pointer) <= m_current_window_size);
	return			(m_current_offset_from_start + (m_current_pointer - m_start_pointer));
}

IC	void CStreamReader::close							()
{
	destroy			();
	CStreamReader	*self = this;
	xr_delete		(self);
}

#endif // STREAM_READER_INLINE_H