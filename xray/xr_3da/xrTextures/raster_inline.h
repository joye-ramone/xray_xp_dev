#pragma once
#include "xrTextures.h"

#define UNROLL 8

IC void	ReduceDimensions			(int& w, int& h, int& l, int& skip)
{
	while ((l>1) && skip)
	{
		w /= 2;
		h /= 2;
		l -= 1;

		skip--;
	}
	if (w<1)	w=1;
	if (h<1)	h=1;
}

ICF LPCSTR Assigned(void *P)
{
	return (P ? "yes" : "no");
}
#pragma optimize("s", off)

// обработка на единственном уровне MIPS (!)
template	<class _It, typename PARAM>
IC	bool	TW_Iterate_1OP
(
	UINT	Level,
	CTextureContext		&t_dst,
	CTextureContext		&t_src,
	CRectIntersection*		intsc,
	const _It				pred, PARAM param
)
{

	D3DSURFACE_DESC				*Dsrc = t_src.load_desc(Level);
	D3DSURFACE_DESC				*Ddst = t_dst.load_desc(Level);
	D3DLOCKED_RECT				*Rsrc, *Rdst;

	UINT avail_width  = __min(Ddst->Width,  Dsrc->Width);
	UINT avail_height = __min(Ddst->Height, Ddst->Height);
	LPRECT src_rect = &t_src.m_selection;
	LPRECT dst_rect = &t_dst.m_selection;

	if (intsc)
	{
		avail_width  = __min(avail_width, intsc->m_avail_width);
		avail_height = __min(avail_height, intsc->m_avail_height);
		Rdst  = t_dst.lock_rect(Level);
		Rsrc  = t_src.lock_rect(Level);
		src_rect = &intsc->m_source;
		dst_rect = &intsc->m_target;
	}
	else
	{
		Rdst = t_dst.lock_rect(Level);
		Rsrc = t_src.lock_rect(Level);
		
	}

	if (NULL == Rsrc || NULL == Rdst) return false;

  // обрезать для развертывания цикла
	UINT width_rounded  = UINT(avail_width / UNROLL)  * UNROLL;
	__try
	{
		VERIFY(Dsrc->Format == Ddst->Format);
		VERIFY(Dsrc->Format == D3DFMT_A8R8G8B8);		
		PBYTE bitsS = (PBYTE)Rsrc->pBits + src_rect->top * Rsrc->Pitch + src_rect->left * 4;
		PBYTE bitsD = (PBYTE)Rdst->pBits + dst_rect->top * Rsrc->Pitch + dst_rect->left * 4;				
		for (UINT y = 0; y < avail_height; y++)	{
			PUINT lineS = PUINT(bitsS);
			PUINT lineD = PUINT(bitsD);
			
			for (UINT x = 0; x < width_rounded; x += UNROLL)	
			{
				lineD[0] = pred(lineD[0], lineS[0], param);				
				lineD[1] = pred(lineD[1], lineS[1], param);
				lineD[2] = pred(lineD[2], lineS[2], param);
				lineD[3] = pred(lineD[3], lineS[3], param);
				lineD[4] = pred(lineD[4], lineS[4], param);
				lineD[5] = pred(lineD[5], lineS[5], param);
				lineD[6] = pred(lineD[6], lineS[6], param);
				lineD[7] = pred(lineD[7], lineS[7], param);
				lineS += UNROLL;
				lineD += UNROLL;				
			}
			for (UINT x = width_rounded; x < avail_width; x ++)			
				lineD[x] = pred(lineD[x], lineS[x], param);				

			bitsS += Rsrc->Pitch;
			bitsD += Rdst->Pitch;
		}
	}
	__finally
	{
		t_src.unlock_rect();
		t_dst.unlock_rect();
	}

	return true;
}


// alpet: шаблон смешивающий две текстуры в одну, посредством оператора pred (как правило инлайн-функции с 32-битным результатом)
// собственно шаблон используется для работы инлайна на полную катушку
template	<class _It>
	bool	TW_Iterate_2OP
(
	UINT					Level,
	CTextureContext			&t_dst,
	CTextureContext			&t_src0,
	CTextureContext			&t_src1,	
	CRectIntersection*		intsc,
	const _It				pred
 )
{
	D3DLOCKED_RECT				*Rsrc0, *Rsrc1, *Rdst;
	D3DSURFACE_DESC				*descD, *descS0, *descS1;
	descS0 = t_src0.load_desc(Level);
	descS1 = t_src1.load_desc(Level);
	descD  = t_dst.load_desc(Level);

	R_ASSERT(descS0 && descS1 && descD);
	VERIFY						(descD->Format == descS0->Format);
	VERIFY						(descD->Format == descS1->Format);
	VERIFY						(descD->Format == D3DFMT_A8R8G8B8);
	
	UINT avail_width  = __min(descD->Width,  descS1->Width);
	UINT avail_height = __min(descD->Height, descS1->Height);
	LPRECT src_rect = &t_src1.m_selection;
	LPRECT dst_rect = &t_dst.m_selection;

	if (intsc)
	{
		avail_width  = __min(avail_width,  intsc->m_avail_width);
		avail_height = __min(avail_height, intsc->m_avail_height);
		Rdst  = t_dst.lock_rect(Level);
		Rsrc0 = t_src0.lock_rect(Level);
		Rsrc1 = t_src1.lock_rect(Level);
		src_rect = &intsc->m_source;
		dst_rect = &intsc->m_target;
	}
	else
	{
		Rdst  = t_dst.lock_rect(Level);
		Rsrc0 = t_src0.lock_rect(Level);
		Rsrc1 = t_src1.lock_rect(Level);
	}
	if (Rdst && Rsrc0 && Rsrc1);
	else
	{
		Msg("! ERROR: cannot lock textures DST = %s, S0 = %s, S1 = %s ", Assigned(Rdst), Assigned(Rsrc0), Assigned(Rsrc1));
		if (Rdst)  t_dst.unlock_rect();
		if (Rsrc0) t_src0.unlock_rect();
		if (Rsrc1) t_src1.unlock_rect();
		return false;
	}
	/* alpet: Блокировка полной текстуры позволяет кэшировать текстуры в заблокированном состоянии, т.е. DIB-растры фактически	
	   В результате обсчет производится с смещением внутри растра каждой используемой текстуры.
	*/
	
	UINT dst_offset = dst_rect->top * Rsrc0->Pitch + dst_rect->left * 4;

	PBYTE bits0 = (PBYTE)Rsrc0->pBits + dst_offset;
	PBYTE bits1 = (PBYTE)Rsrc1->pBits + src_rect->top * Rsrc1->Pitch + src_rect->left * 4;;
	PBYTE bitsD = (PBYTE)Rdst->pBits  + dst_offset;
	UINT width_rounded = UINT(avail_width / UNROLL)  * UNROLL;
	UINT_PTR limit0 = (UINT_PTR)Rsrc0->pBits + Rsrc0->Pitch * descS0->Height;
	UINT_PTR addr0;

	bool err_flag = false;
	while ( ( addr0 = UINT_PTR(bits0 + (avail_height - 1) * Rsrc0->Pitch + avail_width * 4) ) > limit0  && avail_height > 0 )
	{
		MsgCB("! #WARN(TW_Iterate_2OP): possible texture buffer overrun 0x%x > 0x%x, aw = %d, ah = %d", 
				addr0, limit0, avail_width, avail_height);
		avail_height--;
		err_flag = true;
	}
	if (err_flag) MsgCB("* pitches = { %d, %d, %d } ", Rsrc0->Pitch, Rsrc1->Pitch, Rdst->Pitch);
	if (intsc && err_flag)
	{
		DumpIntRect("* intersection.m_source ", intsc->m_source);
		DumpIntRect("* intersection.m_target ", intsc->m_target);
		MsgCB(" * src texture size = %d x %d", descS0->Width, descS0->Height);
		MsgCB(" * dst texture size = %d x %d", descD->Width,   descD->Height);
	}

	for (UINT y = 0; y < avail_height; y++) {
		// в любом случае каждый цикл проходит одну линейку из кэша ЦП?
		PDWORD line0 = PDWORD(bits0);
		PDWORD line1 = PDWORD(bits1);
		PDWORD lineD = PDWORD(bitsD);
		bits0 += Rsrc0->Pitch;
		bits1 += Rsrc1->Pitch;
		bitsD += Rdst->Pitch;
		// line0 += offset.x; 
		// lineD += offset.x;
		UINT x = 0;
		while (x < width_rounded) {
			lineD[0] = pred(lineD[0], line0[0], line1[0]);
			lineD[1] = pred(lineD[1], line0[1], line1[1]);
			lineD[2] = pred(lineD[2], line0[2], line1[2]);
			lineD[3] = pred(lineD[3], line0[3], line1[3]);
			lineD[4] = pred(lineD[4], line0[4], line1[4]);
			lineD[5] = pred(lineD[5], line0[5], line1[5]);
			lineD[6] = pred(lineD[6], line0[6], line1[6]);
			lineD[7] = pred(lineD[7], line0[7], line1[7]);
			line0 += UNROLL;
			line1 += UNROLL;
			lineD += UNROLL;
			x += UNROLL;
		}
		// неразвернутая часть цикла
		__try
		{
			while (x < avail_width) {
				static UINT sv0 = line0[x];
				static UINT sv1 = line1[x];
				lineD[x] = pred(lineD[x], sv0, sv1);
				x++;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			MsgCB("! #EXCEPTION(TW_Iterate_2OP): x = %d, y = %d, aw = %d, ah = %d, pitches = { %d, %d, %d } ",
				x, y, avail_width, avail_height,
				Rsrc0->Pitch, Rsrc1->Pitch, Rdst->Pitch);

			MsgCB("! #POINTERS: bits = { 0x%p, 0x%p, 0x%p }, lines = { 0x%p, 0x%p, 0x%p } ",
					Rsrc0->pBits, Rsrc1->pBits, Rdst->pBits,
					line0, line1, lineD);
			Beep(100, 500);
		}
	} // for y

	t_dst.unlock_rect();
	t_src0.unlock_rect();
	t_src1.unlock_rect();
	return true;
}

ICF UINT it_gloss_rev		(UINT d, UINT s, UINT p)	{	return	color_rgba	(
	color_get_A(s),		// gloss
	color_get_B(d),
	color_get_G(d),
	color_get_R(d)		);
}
ICF UINT it_gloss_rev_base(UINT d, UINT s, UINT p)	{	
	UINT		occ		= color_get_A(d)/3;
	UINT		def		= 8;
	UINT		gloss	= (occ*1+def*3)/4;
	return	color_rgba	(
		gloss,			// gloss
		color_get_B(d),
		color_get_G(d),
		color_get_R(d)
	);
}
ICF UINT it_difference	(UINT d, UINT orig, UINT ucomp)	{	return	color_rgba(
	128+(int(color_get_R(orig))-int(color_get_R(ucomp)))*2,		// R-error
	128+(int(color_get_G(orig))-int(color_get_G(ucomp)))*2,		// G-error
	128+(int(color_get_B(orig))-int(color_get_B(ucomp)))*2,		// B-error
	128+(int(color_get_A(orig))-int(color_get_A(ucomp)))*2	);	// A-error	
}
ICF UINT it_height_rev	(UINT d, UINT s, UINT p)	{	return	color_rgba	(
	color_get_A(d),					// diff x
	color_get_B(d),					// diff y
	color_get_G(d),					// diff z
	color_get_R(s)	);				// height
}
ICF UINT it_height_rev_base(UINT d, UINT s, UINT p)	{	return	color_rgba	(
	color_get_A(d),					// diff x
	color_get_B(d),					// diff y
	color_get_G(d),					// diff z
	(color_get_R(s)+color_get_G(s)+color_get_B(s))/3	);	// height
}


ICF UINT alpha_blend(UINT dst, UINT src, UINT add) { 
	// B,G,R,A
	UCHAR  *a = (UCHAR*)&add;
	UCHAR  *d = (UCHAR*)&dst;
	UCHAR  *s = (UCHAR*)&src;	
	u16    da = d[3];
	da = __min( u16(255), da + (u16)a[3] ); // Складывание альфы
// #define NATIVE_CODE 1

#ifdef NATIVE_CODE
	u16 add_a = a[3];  // color_get_A(add);	
	u16 inv_a = 255 - add_a;
	u16 sb = u16(s[0]) * inv_a;
	u16 sg = u16(s[1]) * inv_a;
	u16 sr = u16(s[2]) * inv_a;
	add_a ++;
	u16 ab = u16(a[0]) * add_a;	
	u16 ag = u16(a[1]) * add_a;	
	u16 ar = u16(a[2]) * add_a;

	d[0] = UCHAR( ( sb + ab ) >> 8 );
	d[1] = UCHAR( ( sg + ag ) >> 8 );
	d[2] = UCHAR( ( sr + ar ) >> 8 );	
#else
	UINT alpha_32 = (UINT)a[3] * 0x01010101;			// TEST: 0xffffffff
	UINT inv_32   = UINT(-1) - alpha_32;				// TEST: 0x0
	__m128i _src = _mm_cvtsi32_si128(src);			// TEST: 0x0
	__m128i _add = _mm_cvtsi32_si128(add);			// TEST: 0xffffffff
	__m128i _inv   = _mm_cvtsi32_si128(inv_32);		// TEST: 0x0
	__m128i _alpha = _mm_cvtsi32_si128(alpha_32);		// TEST: 0xffffffff
	__m128i _0	   = _mm_setzero_si128();
	__m128i _1111  = _mm_cvtsi32_si128(0x01010101);
	// argb => 8 x u16 { a r g b }		
	_alpha = _mm_adds_epu16(_mm_unpacklo_epi8(_alpha, _0), _mm_unpacklo_epi8(_1111, _0)); // alpha = 0x100[4]
	_inv = _mm_unpacklo_epi8(_inv, _0);
	_src = _mm_mullo_epi16(_mm_unpacklo_epi8(_src, _0), _inv);  // src * inv_alpha, TEST: 0 * 0 = 0
	_add = _mm_mullo_epi16(_mm_unpacklo_epi8(_add, _0), _alpha);  // add * alpha, TEST: 0xff[4] x 0x100[4] = 0xff00
	__m128i _res = _mm_srli_epi16(_mm_adds_epu16(_src, _add), 8);								   // (S + A) <<= 8	TEST: sum = 0xff00[4], shifed = 0xff[4]
	_res = _mm_packus_epi16(_res, _0);												   // pack 8 x u16 => 8 x 8, TEST: 0xffffffff
	dst = _mm_cvtsi128_si32(_res);	     // store
	// if ((ver & 0xffffff) != (dst & 0xffffff)) __asm nop;		
#endif
	// da = da < 255 ? da + 1 : da;
	d[3] = (u8)da;
	return dst;	
}

ICF UINT alpha_blend_mask (UINT dst, UINT src, UINT add) { 
	// B,G,R,A
	UCHAR *s = (UCHAR*)&src;
	UCHAR *a = (UCHAR*)&dst;
	UCHAR src_a = s[3];
	if (0 == a[3]) return add; // добавляется полная прозрачность, дыра в текстуре
	if (0 == src_a || src_a > 200) return src; // при полной прозрачности, или наоборот заполнености - не затирать
	return alpha_blend(dst, src, add);	
}

ICF UINT alpha_replace(UINT &dst, const UINT &src, UINT rep)
{
	UCHAR *d = (UCHAR*)&dst;		
	d[3] = (UCHAR)rep;
	return dst;
}


#pragma optimize("s", on)