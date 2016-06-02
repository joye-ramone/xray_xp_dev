#include "stdafx.h"
#pragma hdrstop

#include "fs_internal.h"

#pragma warning(disable:4995)
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <Shlwapi.h>
#pragma warning(default:4995)
#pragma optimize("gyts", off)

typedef void DUMMY_STUFF (const void*,const u32&,void*);
XRCORE_API DUMMY_STUFF	*g_dummy_stuff = 0;
extern size_t mapping_mem_usage;

float   find_time  = 0;	// IReaderBase::find_chunk
int     find_count = 0;

volatile LONG g_last_block = 0;

typedef struct _FIND_PERF_BLOCK
{
	const IFileSystemResource	*R;
	u32							ID;
	CTimer				     timer;
} FIND_PERF_BLOCK, *PFIND_PERF_BLOCK;


FIND_PERF_BLOCK g_find_data[16];
HANDLE g_fs_heap = 0;

LPCSTR  auto_size(u64 size)
{
	static string64 buff;
	if (size < 0xffff)
		sprintf_s(buff, 64, "%llu", size);
	else
		sprintf_s(buff, 64, "%f MiB", size_in_mib(size));
	return buff;
}


#define USE_FS_MM

#ifdef USE_FS_MM

XRCORE_API PVOID fs_alloc(size_t cb)
{
	if (NULL == g_fs_heap)
		g_fs_heap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 1048576, 0); 
	PVOID result = NULL;
	HeapLock(g_fs_heap);
	__try
	{
		result =  HeapAlloc (g_fs_heap, HEAP_GENERATE_EXCEPTIONS, cb);
	}
	__finally
	{
		HeapUnlock(g_fs_heap);
	};
	return result;
}

XRCORE_API PVOID fs_realloc(PVOID data, size_t cb)
{
	if (NULL == g_fs_heap)
		g_fs_heap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 1048576, 0); 
	PVOID result = NULL;
	HeapLock(g_fs_heap);
	__try
	{
		result = HeapReAlloc(g_fs_heap, HEAP_GENERATE_EXCEPTIONS, data, cb);
	}
	__finally
	{
		HeapUnlock(g_fs_heap);
	};
	return result;
}


XRCORE_API void fs_free(PVOID data)
{
	if (!g_fs_heap)
	{
		xr_free(data);
		return;
	}


	HeapLock(g_fs_heap);
	__try
	{
		HeapFree(g_fs_heap, HEAP_GENERATE_EXCEPTIONS, data);
	}
	__finally
	{
		HeapUnlock(g_fs_heap);
	};
}
#else
void *fs_alloc(size_t cb)
{
	return Memory.mem_alloc	(cb);
}


#endif




void *do_before_find(const IFileSystemResource *R, u32 ID)
{
#ifdef PROFILE_FS
	LONG n = InterlockedIncrement(&g_last_block) & 0x0F;
	FIND_PERF_BLOCK &pb = g_find_data[n];
	pb.ID = ID;
	pb.R   = R;
	pb.timer.Start();
	return &pb;
#else
	return NULL;
#endif
}

void do_after_find(void *param)
{
#ifdef PROFILE_FS
	PFIND_PERF_BLOCK pb = (PFIND_PERF_BLOCK)param;
	find_time += pb->timer.GetElapsed_sec();
	if ((++find_count) % 100000 == 0)
		MsgCB("##PERF: IReaderBase::find_chunk called %d times, find chunk time = %.3f seconds", find_count, find_time);
#endif
}


#ifdef M_BORLAND
#	define O_SEQUENTIAL 0
#endif // M_BORLAND

#ifdef DEBUG
	XRCORE_API	u32								g_file_mapped_memory = 0;
	u32								g_file_mapped_count	= 0;
	typedef std::map<u32,std::pair<u32,shared_str> >	FILE_MAPPINGS;
	FILE_MAPPINGS					g_file_mappings;

void register_file_mapping			(void *address, const u32 &size, LPCSTR file_name)
{
	FILE_MAPPINGS::const_iterator	I = g_file_mappings.find(*(u32*)&address);
	VERIFY							(I == g_file_mappings.end());
	g_file_mappings.insert			(std::make_pair(*(u32*)&address,std::make_pair(size,shared_str(file_name))));

	g_file_mapped_memory			+= size;
	++g_file_mapped_count;
#ifdef USE_MEMORY_MONITOR
//	memory_monitor::monitor_alloc	(addres,size,"file mapping");
	string512						temp;
	sprintf_s						(temp, sizeof(temp),"file mapping: %s",file_name);
	memory_monitor::monitor_alloc	(address,size,temp);
#endif // USE_MEMORY_MONITOR
}

void unregister_file_mapping		(void *address, const u32 &size)
{
	FILE_MAPPINGS::iterator			I = g_file_mappings.find(*(u32*)&address);
	VERIFY							(I != g_file_mappings.end());
//	VERIFY2							((*I).second.first == size,make_string("file mapping sizes are different: %d -> %d",(*I).second.first,size));
	g_file_mapped_memory			-= (*I).second.first;
	--g_file_mapped_count;

	g_file_mappings.erase			(I);

#ifdef USE_MEMORY_MONITOR
	memory_monitor::monitor_free	(address);
#endif // USE_MEMORY_MONITOR
}

XRCORE_API void dump_file_mappings	()
{
	Msg								("* active file mappings (%d):",g_file_mappings.size());

	FILE_MAPPINGS::const_iterator	I = g_file_mappings.begin();
	FILE_MAPPINGS::const_iterator	E = g_file_mappings.end();
	for ( ; I != E; ++I)
		Msg							(
			"* [0x%08x][%d][%s]",
			(*I).first,
			(*I).second.first,
			(*I).second.second.c_str()
		);
}
#endif // DEBUG
//////////////////////////////////////////////////////////////////////
// Tools
//////////////////////////////////////////////////////////////////////
//---------------------------------------------------
void VerifyPath(LPCSTR path)
{
	string1024 tmp;
	for(int i=0;path[i];i++){
		if( path[i]!='\\' || i==0 )
			continue;
		CopyMemory( tmp, path, i );
		tmp[i] = 0;
        _mkdir(tmp);
	}
}
void*  FileDownload(LPCSTR fn, file_size* pdwSize)
{
	int		hFile;
	u32		size;
	void*	buf;	
	/*
	string_path fname;
	if (xr_strlen(fn) > 2 && fn[1] != ':')
	{
		FS.update_path(fname, "$fs_root$", fn);
		fn = fname;
	}
	*/

	if (!PathFileExists(fn))
	{
		Debug.fatal(DEBUG_INFO, "FileDownload: Файл не существует %s", fn);
		return NULL;
	}

#ifdef _EDITOR
	hFile	= _open(fn,O_RDONLY|O_BINARY|O_SEQUENTIAL);
#else
	hFile	= _open(fn,O_RDONLY|O_BINARY|O_SEQUENTIAL,_S_IREAD);
#endif
	if (hFile<=0)	{
		Sleep	(1);
#ifdef _EDITOR
		hFile	= _open(fn,O_RDONLY|O_BINARY|O_SEQUENTIAL);
#else
		hFile	= _open(fn,O_RDONLY|O_BINARY|O_SEQUENTIAL,_S_IREAD);
#endif
	}
	R_ASSERT2(hFile>0,fn);
#ifdef _EDITOR
	size	= filelength(hFile);
#else
	size	= _filelength(hFile);
#endif

	buf		= fs_alloc	(size
#ifdef DEBUG_MEMORY_NAME
		,"FILE in memory"
#endif // DEBUG_MEMORY_NAME
		);
	int r_bytes	= _read	(hFile,buf,size);
	R_ASSERT3(r_bytes==(int)size,"Can't read file data:",fn);
	_close	(hFile);
	if (pdwSize) *pdwSize = size;
	return buf;
}

typedef char MARK[9];
IC void mk_mark(MARK& M, const char* S)
{	strncpy(M,S,8); }

void  FileCompress	(const char *fn, const char* sign, void* data, u32 size)
{
	MARK M; mk_mark(M,sign);

	int H	= open(fn,O_BINARY|O_CREAT|O_WRONLY|O_TRUNC,S_IREAD|S_IWRITE);
	R_ASSERT2(H>0,fn);
	_write	(H,&M,8);
	_writeLZ(H,data,size);
	_close	(H);
}

void*  FileDecompress	(const char *fn, const char* sign, u32* size)
{
	MARK M,F; mk_mark(M,sign);

	int	H = open	(fn,O_BINARY|O_RDONLY);
	R_ASSERT2(H>0,fn);
	_read	(H,&F,8);
	if (strncmp(M,F,8)!=0)		{
		F[8]=0;		Msg("FATAL: signatures doesn't match, file(%s) / requested(%s)",F,sign);
	}
    R_ASSERT(strncmp(M,F,8)==0);

	void* ptr = 0; u32 SZ;
	SZ = _readLZ (H, ptr, filelength(H)-8);
	_close	(H);
	if (size) *size = SZ;
	return ptr;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
//---------------------------------------------------
// memory
CMemoryWriter::~CMemoryWriter() 
{	
	fs_free(data);
}

void CMemoryWriter::w	(const void* ptr, u32 count)
{
	if (position+count > mem_size) {
		// reallocate
		if (mem_size==0)	mem_size=128;
		while (mem_size <= (position+count)) mem_size*=2;
		if (0==data)		data = (BYTE*)	fs_alloc	(mem_size
#ifdef DEBUG_MEMORY_NAME
			,		"CMemoryWriter - storage"
#endif // DEBUG_MEMORY_NAME
			);
		else				data = (BYTE*)	fs_realloc	( data, mem_size
#ifdef DEBUG_MEMORY_NAME
			,	"CMemoryWriter - storage"
#endif // DEBUG_MEMORY_NAME
			);
	}


	CopyMemory	(data+position,ptr,count);
	position		+=count;
	if (position > file_size) file_size = position;
}

//static const u32 mb_sz = 0x1000000;
bool CMemoryWriter::save_to	(LPCSTR fn)
{
	IWriter* F 		= FS.w_open(fn);
    if (F){
	    F->w		(pointer(),size());
    	FS.w_close	(F);
        return 		true;
    }
    return false;
}


void	IWriter::open_chunk	(u32 type)
{
	w_u32(type);
	chunk_pos.push(tell());
	w_u32(0);	// the place for 'size'
}
void	IWriter::close_chunk	()
{
	VERIFY(!chunk_pos.empty());

	int pos			= tell();
	seek			(chunk_pos.top());
	w_u32			(pos-chunk_pos.top()-4);
	seek			(pos);
	chunk_pos.pop	();
}
u32	IWriter::chunk_size	()					// returns size of currently opened chunk, 0 otherwise
{
	if (chunk_pos.empty())	return 0;
	return tell() - chunk_pos.top()-4;
}

void	IWriter::w_compressed(void* ptr, u32 count)
{
	BYTE*		dest	= 0;
	unsigned	dest_sz	= 0;
	_compressLZ	(&dest,&dest_sz,ptr,count);
	
	if (g_dummy_stuff)
		g_dummy_stuff	(dest,dest_sz,dest);

	if (dest && dest_sz)
		w(dest,dest_sz);
	fs_free	(dest);
}

void	IWriter::w_chunk(u32 type, void* data, u32 size)
{
	open_chunk	(type);
	if (type & CFS_CompressMark)	w_compressed(data,size);
	else							w			(data,size);
	close_chunk	();
}
void 	IWriter::w_sdir	(const Fvector& D) 
{
	Fvector C;
	float mag		= D.magnitude();
	if (mag>EPS_S)	{
		C.div		(D,mag);
	} else {
		C.set		(0,0,1);
		mag			= 0;
	}
	w_dir	(C);
	w_float (mag);
}
void	IWriter::w_printf(const char* format, ...)
{
	va_list mark;
	char buf[1024];
	va_start( mark, format );
	vsprintf( buf, format, mark );
	w		( buf, xr_strlen(buf) );
}

//---------------------------------------------------
// base stream

bool IReader::chunk_at(u32 ID, s64 pos, u32 max_diff) 
{ 
	if (pos < 0)
		pos = next_chunk_pos;

	if (pos < 0 || pos + 8 >= (s64) length64()) return false;

	seek64(pos);
	while (elapsed64() > 8)
	{
		u32 dwType = r_u32() & (~CFS_CompressMark);
		u32 dwSize = r_u32();
		// если чанк найден, прерывается цикл
		if (dwSize && dwType == ID )
		{
			advance64(-8);
			return true;
		}
			
		if (!max_diff-- || elapsed64() <= dwSize) break;		
		advance64(dwSize);
	}
	return false;
}

void IReader::remember_chunk(const s64 pos)
{
	u32 dest = InterlockedIncrement(&last_chunk_idx) & (MAX_LAST_CHUNKS - 1);	
	last_chunks[dest] = pos;
}



IReader*	IReader::open_chunk(u32 ID, const s64 pos)
{
	static int opens_count = 0;




	if (pos != 0)
		set_next_chunk(pos);

	if (next_chunk_pos < 0 || next_chunk_pos > Size)
		set_next_chunk (iterpos);

	BOOL	bCompressed;
	// оптимизация для многопоточного доступа - выбор среди последних чанков, могут быть пропущенные
	if (!chunk_at(ID, next_chunk()))
	{
		for (u32 i = 0; i < MAX_LAST_CHUNKS; i++)
		{			
			const s64 _pos = last_chunks[i];
			if (_pos > 0 && _pos + 8 < length64() && chunk_at(ID, _pos))
			{
				set_next_chunk(_pos);
				break;
			}
		}
	}
	
	
	u32	dwSize = find_chunk(ID,&bCompressed);
	

	IReader *result = NULL;

	if (dwSize!=0) {
		// remember_chunk ( next_chunk() );
		if (bCompressed) {
			BYTE*		dest;
			unsigned	dest_sz = rtc_check_fastpack(pointer(), dwSize);
			if (dest_sz)
			{				
				dest = (BYTE*) fs_alloc(dest_sz);
				dest_sz = rtc_decompress(dest, dest_sz, pointer(), dwSize);
			}
			else
				_decompressLZ(&dest,&dest_sz,pointer(),dwSize);
		    result = xr_new<CTempReader>	(dest,		dest_sz,		tell64() + dwSize);
		} else 
			result = xr_new<IReader>		(pointer(),	dwSize,			tell64() + dwSize);
				
		return result;
	} else return 0;
};
void	IReader::close()
{	xr_delete((IReader*)this); }

IReader*	IReader::open_chunk_iterator	(u32& ID, IReader* _prev)
{
	if (0==_prev)	{
		// first
		rewind		();
	} else {
		// next
		seek64		(_prev->iterpos);
		_prev->close();
	}

	//	open
	if			(elapsed()<8)	return		NULL;
	ID			= r_u32	()		;
	u32 _size	= r_u32	()		;
	if ( ID & CFS_CompressMark )
	{
		// compressed
		u8*				dest	;
		unsigned		dest_sz	= rtc_check_fastpack(pointer(), _size);
		if (dest_sz)
		{				
			dest = (BYTE*) fs_alloc(dest_sz);
			dest_sz = rtc_decompress(dest, dest_sz, pointer(), _size);
		}
		else
			_decompressLZ	(&dest,&dest_sz,pointer(),_size);
		return xr_new<CTempReader>	(dest,		dest_sz,	tell()+_size);
	} else {
		// normal
		return xr_new<IReader>		(pointer(),	_size,		tell()+_size);
	}
}




void	IReader::r	(void *p, s64 cnt)
{
	VERIFY				(Pos+cnt<=Size);
	if (Pos < 0 || Pos + cnt > Size)
		Debug.fatal(DEBUG_INFO, "IReader::r invalid Pos + cnt = %s, Size = %s ", auto_size(Pos + cnt), auto_size(Size));
	if (IsBadReadPtr(pointer(), limit32(cnt)))
  		Debug.fatal(DEBUG_INFO, "IReader::r bad read pointer %p, Memory = %p, Pos = %d ", pointer(), data, Pos);
	if (IsBadWritePtr(p, limit32(cnt)))
		Debug.fatal(DEBUG_INFO, "IReader::r bad write pointer %p", p);

	CopyMemory		(p,pointer(), limit32(cnt));
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', Pos, limit32(cnt));
#endif
	advance64			(cnt);	
#ifdef DEBUG
	BOOL	bShow		= FALSE		;
	if (dynamic_cast<CFileReader*>(this))			bShow = TRUE;
	if (dynamic_cast<CVirtualFileReader*>(this))	bShow = TRUE;
	if (bShow)			{
  		FS.dwOpenCounter	++		;
	}
#endif
};

IC BOOL			is_term		(char a) { return (a==13)||(a==10); };
IC s32	IReader::advance_term_string()
{
	s32 sz		= 0;
	char *src 	= (char *) data;
	while (!eof()) {
        Pos++;
        sz++;
		if (!eof()&&is_term(src[Pos])) {
        	while(!eof()&&is_term(src[Pos])) Pos++;
			break;
		}
	}
    return sz;
}
void	IReader::r_string	(char *dest, u32 tgt_sz)
{
	char *src 	= (char *) data+Pos;
	s64 _pos = Pos;
	u32 sz 		= advance_term_string();
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', _pos, sz);
#endif
	if (sz > tgt_sz)
	{
		Debug.fatal(DEBUG_INFO, "r_string trying to read %d chars in %d bytes buffer from string \"%s\"", sz, tgt_sz, src);
		R_ASSERT2(sz < (tgt_sz - 1), "Dest string less than needed.");
	}
    strncpy		(dest,src,sz);
    dest[sz]	= 0;
}
void	IReader::r_string	(xr_string& dest)
{
	char *src 	= (char *) data+Pos;	
	s64 _pos = Pos;
	u64 sz 		= advance_term_string();
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', _pos, limit32(sz));
#endif
    dest.assign	(src,limit32(sz));
}
void	IReader::r_stringZ	(char *dest, u32 tgt_sz)
{
	char *src 	= (char *) data;
	u32 sz 		= xr_strlen(src);
    R_ASSERT2(sz<tgt_sz,"Dest string less than needed.");
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', Pos, tgt_sz);
#endif
	while ((src[Pos]!=0) && (!eof())) *dest++ = src[Pos++];
	*dest		=	0;
	Pos++;
}
void 	IReader::r_stringZ	(shared_str& dest)
{
	dest		= (char*)(data+Pos);
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', Pos, dest.size() +1);
#endif
    Pos			+=(dest.size()+1);
}
void	IReader::r_stringZ	(xr_string& dest)
{
    dest 		= (char*)(data+Pos);
#ifdef GATHER_FILE_STATS
	FS.gather_file_stat(this, 'r', Pos, dest.size() +1);
#endif
    Pos			+=int(dest.size()+1);
};

void	IReader::skip_stringZ	()
{
	char *src = (char *) data;
	while ((src[Pos]!=0) && (!eof())) Pos++;
	Pos		++;
};

//---------------------------------------------------
// temp stream
CTempReader::~CTempReader()
{	fs_free(data);	};
//---------------------------------------------------
// pack stream
CPackReader::~CPackReader()
{
#ifdef DEBUG
	unregister_file_mapping	(base_address,Size);
#endif // DEBUG

	UnmapViewOfFile	(base_address);
	mapping_mem_usage -= (size_t)Size;
};
//---------------------------------------------------
// file stream
CFileReader::CFileReader(const char *name):
	IReader()
{
    data	= (char *)FileDownload(name, (u64*)&Size);
    Pos		= 0;
};
CFileReader::~CFileReader()
{	fs_free(data);	};
//---------------------------------------------------
// compressed stream
CCompressedReader::CCompressedReader(const char *name, const char *sign)
{
    data	= (char *)FileDecompress(name,sign,(u32*)&Size);
    Pos		= 0;
}
CCompressedReader::~CCompressedReader()
{	fs_free(data);	};


CVirtualFileRW::CVirtualFileRW(const char *cFileName) 
{
	// Open the file
	hSrcFile		= CreateFile(cFileName, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	R_ASSERT3		(hSrcFile!=INVALID_HANDLE_VALUE,cFileName,Debug.error2string(GetLastError()));
	LARGE_INTEGER   size;
	GetFileSizeEx  (hSrcFile, &size);
	Size			= size.QuadPart;
	R_ASSERT3		(Size, cFileName, Debug.error2string(GetLastError()));

	hSrcMap			= CreateFileMapping (hSrcFile, 0, PAGE_READWRITE, 0, 0, 0);
	R_ASSERT3		(hSrcMap!=INVALID_HANDLE_VALUE,cFileName,Debug.error2string(GetLastError()));

	data			= (char*)MapViewOfFile (hSrcMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	R_ASSERT3		(data,cFileName,Debug.error2string(GetLastError()));
	mapping_mem_usage += (size_t)Size;

#ifdef DEBUG
	register_file_mapping	(data,Size,cFileName);
#endif // DEBUG
}

CVirtualFileRW::~CVirtualFileRW() 
{
#ifdef DEBUG
	unregister_file_mapping	(data,Size);
#endif // DEBUG

	UnmapViewOfFile ((void*)data);
	mapping_mem_usage -= (size_t)Size;
	CloseHandle		(hSrcMap);
	CloseHandle		(hSrcFile);
}

CVirtualFileReader::CVirtualFileReader(const char *cFileName) 
{
	R_ASSERT2( cFileName && xr_strlen(cFileName),
			   make_string("CVirtualFileReader('%s') - specified invalid argument", (cFileName ? cFileName : "(null)")) 
		     );
	// Open the file
	hSrcFile		= CreateFile(cFileName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	R_ASSERT3		(hSrcFile != INVALID_HANDLE_VALUE, cFileName, Debug.error2string(GetLastError()));
	DWORD *size_ref = (DWORD*) &Size;
	size_ref[0]		= (int)GetFileSize(hSrcFile, &size_ref[1]);
	R_ASSERT3		(Size,cFileName,Debug.error2string(GetLastError()));

	hSrcMap			= CreateFileMapping (hSrcFile, 0, PAGE_READONLY, 0, 0, 0);
	R_ASSERT3		(hSrcMap!=INVALID_HANDLE_VALUE,cFileName,Debug.error2string(GetLastError()));

	MsgCB("$#CONTEXT: trying map %s, size = %u", cFileName, Size);
	data			= (char*)MapViewOfFile (hSrcMap, FILE_MAP_READ, 0, 0, 0);
	R_ASSERT3		(data,cFileName,Debug.error2string(GetLastError()));
	mapping_mem_usage += (size_t)Size;
#ifdef DEBUG
	register_file_mapping	(data,Size,cFileName);
#endif // DEBUG
}

CVirtualFileReader::~CVirtualFileReader() 
{
#ifdef DEBUG
	unregister_file_mapping	(data,Size);
#endif // DEBUG

	UnmapViewOfFile ((void*)data);
	CloseHandle		(hSrcMap);
	CloseHandle		(hSrcFile);
	mapping_mem_usage -= (size_t)Size;
}
