// FS.h: interface for the CFS class.
//
//////////////////////////////////////////////////////////////////////

#ifndef fsH
#define fsH

#define CFS_CompressMark	(1ul << 31ul)

XRCORE_API void VerifyPath	(LPCSTR path);

extern XRCORE_API PVOID fs_alloc(size_t cb);
extern XRCORE_API PVOID fs_realloc(PVOID data, size_t cb);
extern XRCORE_API void  fs_free(PVOID data);


#ifdef DEBUG
	XRCORE_API	extern	u32		g_file_mapped_memory;
	XRCORE_API	extern	u32		g_file_mapped_count;
	XRCORE_API			void	dump_file_mappings		();
				extern	void	register_file_mapping	(void *address, const u32 &size, LPCSTR file_name);
				extern	void	unregister_file_mapping	(void *address, const u32 &size);
#endif // DEBUG

IC int			limit32		(const s64 v) 
{ 
	return v <= int_max ? (int)v : int_max;  
}
IC u32			limit32u	(const u64 v) 
{ 
	return v <= type_max(u32) ? (int)v : type_max(u32);  
}

#pragma pack(push, 4)

class CResourceDesc
{
public:
	const void*			 owner_ref;	
	shared_str			 name_ref;
	CResourceDesc					(const void *owner) 		{ owner_ref = owner; }	
	virtual ~CResourceDesc			()							{ R_ASSERT(this); }
};

ICF f64 size_in_mib(const u64 size) {	return ((f64)size) / 1048576.l; }



class XRCORE_API IFileSystemResource
{

public:
    CResourceDesc  *m_resource_desc;

					IFileSystemResource	()				{ m_resource_desc = NULL; }
	virtual			~IFileSystemResource()				{ xr_delete(m_resource_desc); }	
};

extern XRCORE_API bool check_position(const IFileSystemResource *R, const u64 ptr, const u64 size, LPCSTR context);


XRCORE_API void* do_before_find (const IFileSystemResource *R, u32 ID);	// IReaderBase::find_chunk
XRCORE_API void   do_after_find (void *param);



//------------------------------------------------------------------------------------
// Write
//------------------------------------------------------------------------------------
class XRCORE_API IWriter:
	public IFileSystemResource
{
private:
	xr_stack<u32>		chunk_pos;
public:
	shared_str			fName;
public:
	IWriter	()
	{
	}
	virtual	~IWriter	()
	{
        R_ASSERT3	(chunk_pos.empty(),"Opened chunk not closed.",*fName);
	}

	// kernel
	virtual void	seek	(u32 pos)						= 0;
	virtual u32		tell	()								= 0;

	virtual void	w		(const void* ptr, u32 count)	= 0;

	// generalized writing functions
	IC void			w_u64	(u64 d)					{	w(&d,sizeof(u64));	}
	IC void			w_u32	(u32 d)					{	w(&d,sizeof(u32));	}
	IC void			w_u16	(u16 d)					{	w(&d,sizeof(u16));	}
	IC void			w_u8	(u8 d)					{	w(&d,sizeof(u8));	}
	IC void			w_s64	(s64 d)					{	w(&d,sizeof(s64));	}
	IC void			w_s32	(s32 d)					{	w(&d,sizeof(s32));	}
	IC void			w_s16	(s16 d)					{	w(&d,sizeof(s16));	}
	IC void			w_s8	(s8 d)					{	w(&d,sizeof(s8));	}
	IC void			w_float	(float d)				{	w(&d,sizeof(float));}
	IC void			w_string(const char *p)			{	w(p,(u32)xr_strlen(p));w_u8(13);w_u8(10);	}
	IC void			w_stringZ(const char *p)		{	w(p,(u32)xr_strlen(p)+1);					}
	IC void			w_stringZ(const shared_str& p) 	{	w(*p?*p:"",p.size());w_u8(0);		}
	IC void			w_stringZ(shared_str& p)		{	w(*p?*p:"",p.size());w_u8(0);		}
	IC void			w_stringZ(const xr_string& p)	{	w(p.c_str()?p.c_str():"",(u32)p.size());w_u8(0);	}
	IC void			w_fcolor(const Fcolor &v)		{	w(&v,sizeof(Fcolor));	}
	IC void			w_fvector4(const Fvector4 &v)	{	w(&v,sizeof(Fvector4));	}
	IC void			w_fvector3(const Fvector3 &v)	{	w(&v,sizeof(Fvector3));	}
	IC void			w_fvector2(const Fvector2 &v)	{	w(&v,sizeof(Fvector2));	}
	IC void			w_ivector4(const Ivector4 &v)	{	w(&v,sizeof(Ivector4));	}
	IC void			w_ivector3(const Ivector3 &v)	{	w(&v,sizeof(Ivector3));	}
	IC void			w_ivector2(const Ivector2 &v)	{	w(&v,sizeof(Ivector2));	}

    // quant writing functions
	IC void 		w_float_q16	(float a, float min, float max)
	{
		VERIFY		(a>=min && a<=max);
		float q		= (a-min)/(max-min);
		w_u16		(u16(iFloor(q*65535.f+.5f)));
	}
	IC void 		w_float_q8	(float a, float min, float max)
	{
		VERIFY		(a>=min && a<=max);
		float q		= (a-min)/(max-min);
		w_u8		(u8(iFloor(q*255.f+.5f)));
	}
	IC void 		w_angle16	(float a)		    {	w_float_q16	(angle_normalize(a),0,PI_MUL_2);}
	IC void 		w_angle8	(float a)		    {	w_float_q8	(angle_normalize(a),0,PI_MUL_2);}
	IC void 		w_dir		(const Fvector& D) 	{	w_u16(pvCompress(D));	}
	void 			w_sdir		(const Fvector& D);
	void	__cdecl	w_printf	(const char* format, ...);

	// generalized chunking
	u32				align		();
	void			open_chunk	(u32 type);
	void			close_chunk	();
	u32				chunk_size	();					// returns size of currently opened chunk, 0 otherwise
	void			w_compressed(void* ptr, u32 count);
	void			w_chunk		(u32 type, void* data, u32 size);
	virtual bool	valid		()									{return true;}
	virtual int		flush		()	{return 0;}	//RvP
};

class XRCORE_API CMemoryWriter : public IWriter
{
	u8*				data;
	u32				position;
	u32				mem_size;
	u32				file_size;
public:
	CMemoryWriter() {
		data		= 0;
		position	= 0;
		mem_size	= 0;
		file_size	= 0;
	}
	virtual	~CMemoryWriter();

	// kernel
	virtual void	w			(const void* ptr, u32 count);

	virtual void	seek		(u32 pos)	{	position = pos;				}
	virtual u32		tell		() 			{	return position;			}

	// specific
	IC u8*			pointer		()			{	return data;				}
	IC u32			size		() const 	{	return file_size;			}
	IC void			clear		()			{	file_size=0; position=0;	}
#pragma warning(push)
#pragma warning(disable:4995)
	IC void			free		()			{	file_size=0; position=0; mem_size=0; fs_free(data);	}
#pragma warning(pop)
	bool			save_to		(LPCSTR fn);
};

//------------------------------------------------------------------------------------
// Read
//------------------------------------------------------------------------------------
template <typename implementation_type>
class IReaderBase:
	public IFileSystemResource 
{
public:


	IC implementation_type&impl	()				{return *(implementation_type*)this;}
	IC const implementation_type&impl() const	{return *(implementation_type*)this;}

	IC BOOL			eof			()	const		{return impl().elapsed64()<=0;	};
	
	IC void			r			(void *p, int cnt) {impl().r(p,cnt);}

	IC Fvector		r_vec3		()			{Fvector tmp;r(&tmp,3*sizeof(float));return tmp;	};
	IC Fvector4		r_vec4		()			{Fvector4 tmp;r(&tmp,4*sizeof(float));return tmp;	};
	IC u64			r_u64		()			{	u64 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC u32			r_u32		()			{	u32 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC u16			r_u16		()			{	u16 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC u8			r_u8		()			{	u8 tmp;		r(&tmp,sizeof(tmp)); return tmp;	};
	IC s64			r_s64		()			{	s64 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC s32			r_s32		()			{	s32 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC s16			r_s16		()			{	s16 tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC s8			r_s8		()			{	s8 tmp;		r(&tmp,sizeof(tmp)); return tmp;	};
	IC float		r_float		()			{	float tmp;	r(&tmp,sizeof(tmp)); return tmp;	};
	IC void			r_fvector4	(Fvector4 &v){	r(&v,sizeof(Fvector4));	}
	IC void			r_fvector3	(Fvector3 &v){	r(&v,sizeof(Fvector3));	}
	IC void			r_fvector2	(Fvector2 &v){	r(&v,sizeof(Fvector2));	}
	IC void			r_ivector4	(Ivector4 &v){	r(&v,sizeof(Ivector4));	}
	IC void			r_ivector4	(Ivector3 &v){	r(&v,sizeof(Ivector3));	}
	IC void			r_ivector4	(Ivector2 &v){	r(&v,sizeof(Ivector2));	}
	IC void			r_fcolor	(Fcolor &v)	{	r(&v,sizeof(Fcolor));	}
	
	IC float		r_float_q16	(float min, float max)
	{
		u16	val 	= r_u16();
		float A		= (float(val)*(max-min))/65535.f + min;		// floating-point-error possible
		VERIFY		((A >= min-EPS_S) && (A <= max+EPS_S));
        return A;
	}
	IC float		r_float_q8	(float min, float max)
	{
		u8 val		= r_u8();
		float	A	= (float(val)/255.0001f) *(max-min) + min;	// floating-point-error possible
		VERIFY		((A >= min) && (A <= max));
        return	A;
	}
	IC float		r_angle16	()			{ return r_float_q16(0,PI_MUL_2);	}
	IC float		r_angle8	()			{ return r_float_q8	(0,PI_MUL_2);	}
	IC void			r_dir		(Fvector& A){ u16 t=r_u16(); pvDecompress(A,t); }
	IC void			r_sdir		(Fvector& A)
	{
		u16	t		= r_u16();
		float s		= r_float();
		pvDecompress(A,t);
		A.mul		(s);
	}
	// Set file pointer to start of chunk data (0 for root chunk)
	IC	void		rewind		()			{	impl().seek(0); }
		
	
	IC	u32 		find_chunk	(u32 ID, BOOL* bCompressed = 0)	
	{
		u32	dwSize = 0, dwType = 0, result = 0;
		void *tmp = do_before_find(this, ID);
		
		__try
		{	
			
			if (!impl().chunk_at(ID, -1, 7)) // базова€ оптимизаци€ дл€ последовательного однопоточного доступа. ћожно начинать с чанка -7
				rewind();

			while (impl().elapsed64 () > 8) {							
				dwType = r_u32();
				dwSize = r_u32();			
				if (dwSize > 0)
					impl().remember_chunk( impl().tell64() - 8 );

				const u32 test = dwType & (~CFS_CompressMark);

				if ( test == ID ) {				
					const s64 _next = impl().tell64() + (s64) dwSize;	
					FORCE_VERIFY(_next <= (s64) impl().length64());

					if (bCompressed) 
						*bCompressed = dwType & CFS_CompressMark;

					if (dwSize > 0)
						impl().set_next_chunk(_next);
					result = dwSize;
					break;
				}
				if ( impl().elapsed64() < dwSize ) break;				
				impl().advance64(dwSize);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			u32 *self = (u32*) this;
			Msg("!#EXCEPTION: IReaderBase::find_chunk(%d) this = 0x%p, dwSize = %d, chunk size = %.5lf MiB ",
						ID, this, dwSize, size_in_mib (impl().length64()));
			if (!IsBadReadPtr(self, 16))
			    Msg(" IReader contents: 0x%X, 0x%X, 0x%X, 0x%X ", self[0], self[1], self[2], self[2]);
		}

		do_after_find(tmp);
		return result;
	}
	
	IC	BOOL		r_chunk		(u32 ID, void *dest)	// чтение XR Chunk'ов (4b-ID,4b-size,??b-data)
	{
		u32	dwSize = find_chunk(ID);
		if (dwSize!=0) {
			r(dest,dwSize);
			return TRUE;
		} else return FALSE;
	}
	
	IC	BOOL		r_chunk_safe(u32 ID, void *dest, u32 dest_size)	// чтение XR Chunk'ов (4b-ID,4b-size,??b-data)
	{
		u32	dwSize = find_chunk(ID);
		if (dwSize!=0) {
			R_ASSERT(dwSize==dest_size);
			r(dest,dwSize);
			return TRUE;
		} else return FALSE;
	}
};

#define MAX_LAST_CHUNKS 8

// alpet: подготовка к 64-битной сборке
class XRCORE_API IReader : public IReaderBase<IReader> {
protected:	
	s64				Pos		;
	s64				Size	;
	s64				iterpos	;
	char *			data	;
	s64				last_chunks[MAX_LAST_CHUNKS];
	volatile ULONG	  last_chunk_idx;
	volatile LONGLONG next_chunk_pos;	
	
	IC void			ZeroInit()
	{
	 	next_chunk_pos = Pos = iterpos = Size = 0;
		data		   = NULL;
		last_chunk_idx = 0;
		memset(last_chunks, 0, sizeof(last_chunks));
	}

public:
	IC				IReader			()
	{
		ZeroInit();
	}

	IC				IReader			(void *_data, s64 _size, s64 _iterpos = 0)
	{
		ZeroInit();
		data		= (char *)_data	;
		Size		= _size			;
		Pos			= 0				;
		iterpos		= _iterpos		;
	}

	IC s64			next_chunk()						{  return next_chunk_pos; };
	IC void 		set_next_chunk(s64 pos)				{  InterlockedExchange64(&next_chunk_pos, pos); }
	IC s64			header_offset()						{  return iterpos - Size - 8;  }

protected:
	IC u32			correction					(u32 p)
	{
		if (p%16) {
			return ((p%16)+1)*16 - p;
		} return 0;
	}
	
	s32				advance_term_string			();	
public:
	IC int			elapsed		()	const		{	return limit32 (elapsed64());		};
	IC int			tell		()	const		{	return limit32 (tell64());				};
	IC void			seek		(int ptr)		{	seek64(ptr); };
	IC int			length		()	const		{	return limit32 (Size);				};
	IC void*		pointer		()	const		{	return &(data[(size_t)Pos]);	};
	IC void			advance		(int cnt)		{	advance64(cnt); };	
	IC s64			elapsed64	()	const		{	return Size >= Pos ? Size-Pos : 0;	};
	IC s64			tell64		()	const		{	check_position(this, Pos, Size, "tell64"); return Pos; };
	IC f64			tell_mib	()  const       {   return size_in_mib (Pos); } 
	IC void			seek64		(s64 ptr)		{	Pos=ptr; check_position(this, Pos, Size, "seek64"); };
	IC s64			length64	()	const		{	return Size;			};
	IC f64			length_mib  ()  const		{   return size_in_mib (Size); } 
	IC void			advance64	(s64 cnt)		{	Pos+=cnt; check_position(this, Pos, Size, "advance64"); };

public:
	void			r			(void *p, __int64 cnt);

	void			r_string	(char *dest, u32 tgt_sz);
	void			r_string	(xr_string& dest);

	void			skip_stringZ();

	void			r_stringZ	(char *dest, u32 tgt_sz);
	void			r_stringZ	(shared_str& dest);
	void			r_stringZ	(xr_string& dest);

public:
	void			close		();

public:
	
	bool			chunk_at (u32 ID, s64 pos, u32 max_diff = 0);
	// поиск XR Chunk'ов - возврат - размер или 0
	IReader*		open_chunk	(u32 ID, const s64 pos = 0);

	// iterators
	IReader*		open_chunk_iterator		(u32& ID, IReader* previous=NULL);	// NULL=first	
	void			remember_chunk (const s64 pos);
};



class XRCORE_API CVirtualFileRW : public IReader
{
private:
	void	*hSrcFile, *hSrcMap;
public:
			CVirtualFileRW		(const char *cFileName);
	virtual ~CVirtualFileRW		();
};

#pragma pack(pop)
#endif // fsH
