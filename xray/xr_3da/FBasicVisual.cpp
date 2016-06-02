// IRender_Visual.cpp: implementation of the IRender_Visual class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#pragma hdrstop

#ifndef _EDITOR
    #include "render.h"
#endif    
#include "fbasicvisual.h"
#include "fmesh.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IRender_Mesh::~IRender_Mesh()		
{ 
	_RELEASE(p_rm_Vertices); 
	_RELEASE(p_rm_Indices);		
}

IRender_Visual::IRender_Visual		()
{
	Type				= 0;
	shader_ref			= 0;
	vis.clear			();
	shader_name			= 0;
	textures			= 0;
	prefetched			= false;
}

IRender_Visual::~IRender_Visual		()
{
}

void IRender_Visual::Release		()
{
}

CStatTimer						tscreate;

void IRender_Visual::Load		(const char* N, IReader *data, u32 )
{
#ifdef DEBUG
	dbg_name	= N;
#endif	
	MsgCB("$#CONTEXT: Loading visual %s from reader 0x%p pointer = 0x%p, position = %d, elapsed = %d", N, data, data->pointer(), data->tell(), data->elapsed());

	static volatile ULONG load_count = 0;
	ULONG last = InterlockedIncrement(&load_count);
	if (last % 1500 == 0)
		MsgCB ("##PERF: loading %d visual/mesh... ", last);



	R_ASSERT2(!IsBadReadPtr(data->pointer(), data->elapsed()), "Invalid data pointer in IReader");
	R_ASSERT2(data->length_mib() < 4250, make_string("to large reader size = %.3lf MiB for %s ", data->length_mib(), N));
	// if (data->length64() > 0) R_ASSERT2(data->tell64() < data->length64(),	 make_string("reader position %lf >= size %lf for %s", data->tell_mib(), data->length_mib(), N));
	// header
	VERIFY		(data);
	ogf_header	hdr;
	if (data->r_chunk_safe(OGF_HEADER,&hdr,sizeof(hdr)))
	{
		R_ASSERT2			(hdr.format_version==xrOGF_FormatVersion, "Invalid visual version");
		Type				= hdr.type;
		if (hdr.shader_id)	shader_ref	= ::Render->getShader	(hdr.shader_id);
		vis.box.set			(hdr.bb.min,hdr.bb.max	);
		vis.sphere.set		(hdr.bs.c,	hdr.bs.r	);
	} else {
		FATAL				("Invalid visual");
	}

	// Shader
	if (data->find_chunk(OGF_TEXTURE)) {
		string256		fnT,fnS;
		data->r_stringZ	(fnT,sizeof(fnT));
		data->r_stringZ	(fnS,sizeof(fnS));
		shader_ref.create	(fnS,fnT);
		shader_name = fnS;
		textures = fnT;
	}

    // desc
#ifdef _EDITOR
    if (data->find_chunk(OGF_S_DESC)) 
	    desc.Load		(*data);
#endif
}

#define PCOPY(a)	a = pFrom->a
void	IRender_Visual::Copy(IRender_Visual *pFrom)
{
	PCOPY(Type);
	PCOPY(shader_ref);
	PCOPY(vis);
	PCOPY(shader_name);
	PCOPY(textures);	

#ifdef _EDITOR
	PCOPY(desc);
#endif
#ifdef DEBUG
	PCOPY(dbg_name);
#endif
}
