#include "stdafx.h"
#pragma hdrstop
#define  LZ4_COMPRESSION
#ifdef   LZ4_COMPRESSION
#include "LZ4/lz4lib.h"
#endif
// #include "rt_lzo.h"
#include "rt_lzo1x.h"

// #pragma optimize("gyts", off)

#define HEAP_ALLOC(var,size) \
	lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

__declspec(thread) HEAP_ALLOC(rtc_wrkmem,LZO1X_1_MEM_COMPRESS);


u32    rtc_check_fastpack(const void *data, u32 size)
{
	if (size < 9) return 0;
	if (0 == strncmp((LPCSTR)data, "FASTPACK", 8))	
		return 1;
	if (0 == strncmp((LPCSTR)data, "LZ4F", 4))
	
		return ((u32*)data)[1];
	
	return 0;	
}

void	rtc_initialize	()
{
	VERIFY			(lzo_init()==LZO_E_OK);
}

u32		rtc_csize		(u32 in)
{
#ifdef LZ4_COMPRESSION		
	return	(u32) LZ4_compressBound(in) + 16;
#else
	VERIFY			(in);
	return			in + in/64 + 16 + 3;
#endif
}

u32		rtc_compress	(void *dst, u32 dst_len, const void* src, u32 src_len)
{
#ifdef LZ4_COMPRESSION		
	strcpy_s((LPSTR) dst, 5, "LZ4F"); 
	((u32*)dst)[1] = src_len;  // unpacked size save
	LPSTR body				= (LPSTR)dst + 9;
	dst_len				= LZ4_compress_limitedOutput((LPCSTR)src, body, src_len, dst_len - 9);
	return			    dst_len + 9; // чтобы не обрезались упакованные данные
#else
	u32		out_size	= dst_len;
	int r = lzo1x_1_compress	( 
		(const lzo_byte *) src, (lzo_uint)	src_len, 
		(lzo_byte *) dst,		(lzo_uintp) &out_size, 
		rtc_wrkmem);
	VERIFY	(r==LZO_E_OK);
	return	out_size;
#endif
}
u32		rtc_decompress	(void *dst, u32 dst_len, const void* src, u32 src_len)
{
#ifdef  LZ4_COMPRESSION
	LPCSTR   packed_data = (LPCSTR)src;
	if (rtc_check_fastpack(src, src_len))
	{
		int result = LZ4_decompress_safe (packed_data + 9, (LPSTR)dst, src_len - 9, dst_len);
		if  (result != (int)dst_len)
			 Debug.fatal (DEBUG_INFO, "decompression error, mistmatch sizes: %d <> %d", result, dst_len);
		return (u32)result;
	}
#endif
	u32		out_size	= dst_len;
	int r = lzo1x_decompress	( 
		(const lzo_byte *) src, (lzo_uint)	src_len,
		(lzo_byte *) dst,		(lzo_uintp) &out_size,
		rtc_wrkmem);
	VERIFY	(r==LZO_E_OK);
	return	out_size;
}


