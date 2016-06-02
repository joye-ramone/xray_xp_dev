#ifndef STREAM_READER_H
#define STREAM_READER_H

class XRCORE_API CStreamReader : public IReaderBase<CStreamReader> {
private:
	HANDLE		m_file_mapping_handle;
	file_ptr	m_start_offset;
	file_size	m_file_size;
	file_ptr	m_archive_size;
	u32			m_window_size;

private:
	file_ptr	m_current_offset_from_start;
	u32			m_current_window_size;
	u8			*m_current_map_view_of_file;
	u8			*m_start_pointer;
	u8			*m_current_pointer;

	s64          next_chunk_pos;

private:
			void			map					(const u64 &new_offset);
	IC		void			unmap				();
	IC		void			remap				(const u64 &new_offset);

private:
	// should not be called
	IC						CStreamReader		(const CStreamReader &object);
	IC		CStreamReader	&operator=			(const CStreamReader &);

public:
	IC						CStreamReader		();

public:
	virtual	void			construct			(
								const HANDLE &file_mapping_handle,
								const file_ptr	&start_offset,
								const file_size &fsize,
								const file_size	&archive_size,
								const u32 &window_size
							);
	virtual	void			destroy				();

public:
	IC		const HANDLE	&file_mapping_handle() const;
	IC		u32				elapsed				() const				{ return limit32u(elapsed64()); };
	IC		const u32		length				() const				{ return limit32u(length64());  };
	IC		void			seek				(const int &offset)     { seek64(offset); };   
	IC		u32				tell				() const                { return limit32u(tell64()); };
	IC		void			close				();

	IC		u64				elapsed64			() const;
	IC		const u64		&length64			() const;
	IC		void			seek64				(const s64 &offset);
	IC		u64				tell64				() const;
	
public:
			void			advance				(const int &offset);
			void			advance64			(const s64 &offset);
			void			r					(void *buffer, u32 buffer_size);
			bool			chunk_at			(u32 ID, s64 offset, u32 max_diff = 0);

	IC		s64				next_chunk()								{  return next_chunk_pos; };
	IC		void 			set_next_chunk(s64 pos)						{  InterlockedExchange64(&next_chunk_pos, pos); }

			// поиск XR Chunk'ов - возврат - размер или 0			
			void			remember_chunk		(const s64 offset);
			CStreamReader	*open_chunk			(const u32 &chunk_id, s64 pos = 0);
//.			CStreamReader*open_chunk_iterator(const u32 &chunk_id, CStreamReader *previous = 0);	// 0 means first

public:
//.			void			r_string			(char *dest, u32 tgt_sz);
//.			void			r_string			(xr_string& dest);
//.			void			skip_stringZ		();
//.			void			r_stringZ			(char *dest, u32 tgt_sz);
//.			void			r_stringZ			(shared_str& dest);
//.			void			r_stringZ			(xr_string& dest);
};

#include "stream_reader_inline.h"

#endif // STREAM_READER_H