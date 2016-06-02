#include "stdafx.h"
#include "r2.h"
#include "../resourcemanager.h"
#include "../fbasicvisual.h"
#include "../fmesh.h"
#include "../xrLevel.h"
#include "../x_ray.h"
#include "../IGame_Persistent.h"
#include "../../xrCore/stream_reader.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include <malloc.h>
#pragma warning(pop)

void CRender::level_Load(IReader* fs)
{
	i_swi_allocated = 0;

	R_ASSERT						(0!=g_pGameLevel);
	R_ASSERT						(!b_loaded);

	// Begin
	pApp->LoadBegin					();
	Device.Resources->DeferredLoad	(TRUE);
	IReader*						chunk;

	// Shaders
	g_pGamePersistent->LoadTitle		("st_loading_shaders");
	{
		chunk = fs->open_chunk		(fsL_SHADERS);
		R_ASSERT2					(chunk,"Level doesn't builded correctly.");
		u32 count = chunk->r_u32	();
		Shaders.resize				(count);
		for(u32 i=0; i<count; i++)	// skip first shader as "reserved" one
		{
			string512				n_sh,n_tlist;
			LPCSTR			n		= LPCSTR(chunk->pointer());
			chunk->skip_stringZ		();
			if (0==n[0])			continue;
			strcpy					(n_sh,n);
			LPSTR			delim	= strchr(n_sh,'/');
			*delim					= 0;
			strcpy					(n_tlist,delim+1);
			Shaders[i]				= Device.Resources->Create(n_sh,n_tlist);
		}
		chunk->close();
	}

	// Components
	Wallmarks					= xr_new<CWallmarksEngine>	();
	Details						= xr_new<CDetailManager>	();

	if	(!g_dedicated_server)	{
		// VB,IB,SWI
		g_pGamePersistent->LoadTitle("st_loading_geometry");
		{
			CStreamReader			*geom = FS.rs_open("$level$","level.geom");
			R_ASSERT2				(geom, "level.geom");
			LoadBuffers				(geom,FALSE);
			LoadSWIs				(geom);
			FS.r_close				(geom);
		}

		//...and alternate/fast geometry
		{
			CStreamReader			*geom = FS.rs_open("$level$","level.geomx");
			R_ASSERT2				(geom, "level.geomX");
			LoadBuffers				(geom,TRUE);
			FS.r_close				(geom);
		}

		// Visuals
		g_pGamePersistent->LoadTitle("st_loading_spatial_db");
		// chunk						= fs->open_chunk(fsL_VISUALS);
		LoadVisuals					(fs, false);
		// chunk->close				();

		// Details
		g_pGamePersistent->LoadTitle("st_loading_details");
		Details->Load				();
	}

	// Sectors
	g_pGamePersistent->LoadTitle("st_loading_sectors_portals");
	LoadSectors					(fs);

	// HOM
	HOM.Load					();

	// Lights
	// pApp->LoadTitle			("Loading lights...");
	LoadLights					(fs);

	// End
	pApp->LoadEnd				();

	// sanity-clear
	lstLODs.clear				();
	lstLODgroups.clear			();
	mapLOD.clear				();

	// signal loaded
	b_loaded					= TRUE	;
}

void CRender::level_Unload()
{
	if (0==g_pGameLevel)		return;
	if (!b_loaded)				return;

	u32 I;

	// HOM
	HOM.Unload				();

	//*** Details
	Details->Unload			();

	//*** Sectors
	// 1.
	xr_delete				(rmPortals);
	pLastSector				= 0;
	vLastCameraPos.set		(0,0,0);
	// 2.
	for (I=0; I<Sectors.size(); I++)
		xr_delete(Sectors[I]);
	Sectors.clear			();
	// 3.
	Portals.clear			();

	//*** Lights
	// Glows.Unload			();
	Lights.Unload			();

	//*** Visuals
	for (I=0; I<Visuals.size(); I++)
	{
		Visuals[I]->Release();
		xr_delete(Visuals[I]);
	}
	Visuals.clear			();

	//*** VB/IB
	for (I=0; I<nVB.size(); I++)	_RELEASE(nVB[I]);
	for (I=0; I<xVB.size(); I++)	_RELEASE(xVB[I]);
	nVB.clear(); xVB.clear();
	for (I=0; I<nIB.size(); I++)	_RELEASE(nIB[I]);
	for (I=0; I<xIB.size(); I++)	_RELEASE(xIB[I]);
	nIB.clear(); xIB.clear();
	nDC.clear(); xDC.clear();

	//*** Components
	xr_delete					(Details);
	xr_delete					(Wallmarks);

	//*** Shaders
	Shaders.clear_and_free		();
	b_loaded					= FALSE;
}

void CRender::LoadBuffers		(CStreamReader *base_fs,	BOOL _alternative)
{
	R_ASSERT2					(base_fs,"Could not load geometry. File not found.");
	Device.Resources->Evict		();
	u32	dwUsage					= D3DUSAGE_WRITEONLY;

	xr_vector<VertexDeclarator>				&_DC	= _alternative?xDC:nDC;
	xr_vector<IDirect3DVertexBuffer9*>		&_VB	= _alternative?xVB:nVB;
	xr_vector<IDirect3DIndexBuffer9*>		&_IB	= _alternative?xIB:nIB;

	// Vertex buffers
	{
		// Use DX9-style declarators
		CStreamReader			*fs	= base_fs->open_chunk(fsL_VB);
		R_ASSERT2				(fs,"Could not load geometry. File 'level.geom?' corrupted.");
		u32 count				= fs->r_u32();
		_DC.resize				(count);
		_VB.resize				(count);
		for (u32 i=0; i<count; i++)
		{
			// decl
//			D3DVERTEXELEMENT9*	dcl		= (D3DVERTEXELEMENT9*) fs().pointer();
			u32					buffer_size = (MAXD3DDECLLENGTH+1)*sizeof(D3DVERTEXELEMENT9);
			D3DVERTEXELEMENT9	*dcl = (D3DVERTEXELEMENT9*)_alloca(buffer_size);
			fs->r				(dcl,buffer_size);
			fs->advance			(-(int)buffer_size);

			u32 dcl_len			= D3DXGetDeclLength		(dcl)+1;
			_DC[i].resize		(dcl_len);
			fs->r				(_DC[i].begin(),dcl_len*sizeof(D3DVERTEXELEMENT9));

			// count, size
			u32 vCount			= fs->r_u32	();
			u32 vSize			= D3DXGetDeclVertexSize	(dcl,0);
			MsgV				("7LOAD_GEOM", "* [Loading VB] %d verts, %d Kb",vCount,(vCount*vSize)/1024);

			// Create and fill
			BYTE*	pData		= 0;
			R_CHK				(HW.pDevice->CreateVertexBuffer(vCount*vSize,dwUsage,0,D3DPOOL_MANAGED,&_VB[i],0));
			R_CHK				(_VB[i]->Lock(0,0,(void**)&pData,0));
//			CopyMemory			(pData,fs().pointer(),vCount*vSize);
			fs->r				(pData,vCount*vSize);
			_VB[i]->Unlock		();

//			fs->advance			(vCount*vSize);
		}
		fs->close				();
	}

	// Index buffers
	{
		CStreamReader			*fs	= base_fs->open_chunk(fsL_IB);
		u32 count				= fs->r_u32();
		_IB.resize				(count);
		for (u32 i=0; i<count; i++)
		{
			u32 iCount			= fs->r_u32	();
			MsgV				("7LOAD_GEOM", "* [Loading IB] %d indices, %d Kb",iCount,(iCount*2)/1024);

			// Create and fill
			BYTE*	pData		= 0;
			R_CHK				(HW.pDevice->CreateIndexBuffer(iCount*2,dwUsage,D3DFMT_INDEX16,D3DPOOL_MANAGED,&_IB[i],0));
			R_CHK				(_IB[i]->Lock(0,0,(void**)&pData,0));
//			CopyMemory			(pData,fs().pointer(),iCount*2);
			fs->r				(pData,iCount*2);
			_IB[i]->Unlock		();

//			fs().advance		(iCount*2);
		}
		fs->close				();
	}
}

#pragma optimize("gyts", off)

typedef struct _VIS_LOAD {
  volatile ULONG   is_busy;  
  int				 index;  
  int				  skip;
  HANDLE			m_busy;  // mutex busy
  CModelPool		 *pool;
  IReader		       *fs;  
  IRender_Visual **results;
  IReader		 **chunks;
} VIS_LOAD;


volatile ULONG alive_childs = 0;
volatile LONG  load_index = 0;

void MtVisLoader(void *params)
{
	VIS_LOAD *vl = (VIS_LOAD*) params;
	IRender_Visual*		V		= 0;
	ogf_header		H;	
	CTimer perf;
	// SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	InterlockedExchange(&vl->is_busy, 1);

	R_ASSERT(vl->fs);
	perf.Start();

	while (true)
	{			
		
		int index = InterlockedIncrement(&load_index);
		vl->index = index;
		R_ASSERT(index >= 0 && index < 32768);

		IReader *chunk = vl->fs->open_chunk(index); // найти чанк в потоке
		if (!chunk) break;
		 perf.Start();
		__try
		{
			chunk->r_chunk_safe(OGF_HEADER, &H, sizeof(H));
			V = vl->pool->Instance_Create(H.type);
			vl->chunks[index] = chunk;
			vl->results[index] = V;		
			//V->Load(0, chunk, 0);			 // TODO: это надо сделать синхронно!
		}
		__except (SIMPLE_FILTER)
		{
			Msg("!EXCEPTION: catched in MtVisLoader ");
		}

		// chunk->close();
		// vl->chunk = NULL;		
		if (index % 500 == 0)
				MsgCB("##PERF: load visuals in progress, index = %7d, thread = ~I ", index);
		// ReleaseMutex(vl->m_busy);
	}
	InterlockedExchange(&vl->is_busy, 0);
	ReleaseMutex(vl->m_busy);
	float elps = perf.GetElapsed_sec() * 1000.f;
	MsgCB("##PERF: visual loader ~I completed execution, work time = %.1f ", elps);
	InterlockedDecrement(&alive_childs);
}



void CRender::LoadVisuals(IReader *fs, bool is_chunk)
{
	// IReader*		chunk	= 0;	

#define MT_CHILDS 8
#define PRE_ALLOC 32768

	// HANDLE		mt_list [MT_CHILDS];
	VIS_LOAD	params  [MT_CHILDS];

	IReader*	chunks  [PRE_ALLOC];

	const u32 count_childs = 0; 
	string16 child_name;	

	u32 first = Visuals.size();
	Visuals.resize(PRE_ALLOC);
	IRender_Visual**  results = Visuals.data(); // in-stack storage
	ZeroMemory(results, sizeof(IRender_Visual*) * PRE_ALLOC);
	ZeroMemory(chunks,  sizeof(IReader*) * PRE_ALLOC);

	load_index = -1;

	for (u32 i = 0; i < count_childs; i++)
	{		
		params[i].fs = fs->open_chunk(fsL_VISUALS);
		params[i].index  = i;
		params[i].skip   = count_childs;
		params[i].m_busy = CreateMutex (NULL, FALSE, NULL);
		params[i].pool	 = Models;
		params[i].is_busy = 1;
		params[i].results = results;
		params[i].chunks  = chunks;
		sprintf_s(child_name, 16, "vis_loader_%d", i);
		InterlockedIncrement(&alive_childs);
		thread_spawn(MtVisLoader, child_name, 0, &params[i]);
	}

	

	u32 last_loaded = 0;	

	__try
	{
		IReader *vis_fs = fs->open_chunk(fsL_VISUALS);
		IReader *chunk  = NULL;
		ogf_header H;
		while (vis_fs) 
		{			
			chunk = vis_fs->open_chunk( InterlockedIncrement(&load_index) );
			if (!chunk) break;
			chunk->r_chunk_safe(OGF_HEADER, &H, sizeof(H));
			results[load_index] = Models->Instance_Create(H.type);
			results[load_index]->Load(0, chunk, 0);
			chunk->close();
		}

		if (vis_fs) vis_fs->close();

		// u32 busy_count;
		/*

		while (alive_childs > 0)
		{
			busy_count = 0;
			for (u32 i = 0; i < count_childs; i++)
				if (params[i].is_busy)				
					mt_list[busy_count++] = params[i].m_busy;				

			if (busy_count > 0)
			{
				u32 wait = WaitForMultipleObjects(busy_count, mt_list, TRUE, 1000); // ждать выхода потока
				if (WAIT_TIMEOUT == wait && busy_count == count_childs)
					continue;
			}					
			else
				Sleep(1); // дать шанс на переключение

			while (chunks[last_loaded] && results[last_loaded])
			{
				results[last_loaded]->Load(0, chunks[last_loaded], 0);
				chunks[last_loaded]->close();
				last_loaded++;
			}

		}
		*/
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!EXCEPTION: catched in CRender::LoadVisuals ");
	}

	
	// finalization
	for (u32 i = 0; i < count_childs; i++)
	{
		VIS_LOAD &p = params[i];
		WaitForSingleObject(p.m_busy, 100000);		
		CloseHandle (p.m_busy);
		if (!is_chunk)
		    p.fs->close();
	}
	
	while (chunks[last_loaded] && results[last_loaded])
	{
		results[last_loaded]->Load(0, chunks[last_loaded], 0);
		chunks[last_loaded]->close();
		last_loaded++;
	}

	while (load_index > 0)
		if (NULL == results[load_index])
			load_index --;
		else 
			break;

	Visuals.resize(load_index + 1);
	
	Msg("##PERF: loaded %d visuals, last_loaded = %d, ", Visuals.size() - first, last_loaded);
}

void CRender::LoadLights(IReader *fs)
{
	// lights
	Lights.Load	(fs);
}

struct b_portal
{
	u16				sector_front;
	u16				sector_back;
	svector<Fvector,6>	vertices;
};

void CRender::LoadSectors(IReader* fs)
{
	// allocate memory for portals
	u32 size = fs->find_chunk(fsL_PORTALS); 
	R_ASSERT(0==size%sizeof(b_portal));
	u32 count = size/sizeof(b_portal);
	Portals.resize	(count);
	for (u32 c=0; c<count; c++)
		Portals[c]	= xr_new<CPortal> ();

	// load sectors
	IReader* S = fs->open_chunk(fsL_SECTORS);
	for (u32 i=0; ; i++)
	{
		IReader* P = S->open_chunk(i);
		if (0==P) break;

		CSector* __S		= xr_new<CSector> ();
		__S->load			(*P);
		Sectors.push_back	(__S);

		P->close();
	}
	S->close();

	// load portals
	if (count) 
	{
		CDB::Collector	CL;
		fs->find_chunk	(fsL_PORTALS);
		for (i=0; i<count; i++)
		{
			b_portal	P;
			fs->r		(&P,sizeof(P));
			CPortal*	__P	= (CPortal*)Portals[i];
			__P->Setup	(P.vertices.begin(),P.vertices.size(),
				(CSector*)getSector(P.sector_front),
				(CSector*)getSector(P.sector_back));
			for (u32 j=2; j<P.vertices.size(); j++)
				CL.add_face_packed_D(
				P.vertices[0],P.vertices[j-1],P.vertices[j],
				u32(i)
				);
		}
		if (CL.getTS()<2)
		{
			Fvector					v1,v2,v3;
			v1.set					(-20000.f,-20000.f,-20000.f);
			v2.set					(-20001.f,-20001.f,-20001.f);
			v3.set					(-20002.f,-20002.f,-20002.f);
			CL.add_face_packed_D	(v1,v2,v3,0);
		}

		// build portal model
		rmPortals = xr_new<CDB::MODEL> ();
		rmPortals->build	(CL.getV(),int(CL.getVS()),CL.getT(),int(CL.getTS()));
	} else {
		rmPortals = 0;
	}

	// debug
	//	for (int d=0; d<Sectors.size(); d++)
	//		Sectors[d]->DebugDump	();

	pLastSector = 0;
}



void CRender::LoadSWIs(CStreamReader* base_fs)
{	

	// allocate memory for portals
	if (base_fs->find_chunk(fsL_SWIS)){
		CStreamReader		*fs	= base_fs->open_chunk(fsL_SWIS);
		u32 item_count		= fs->r_u32();

		xr_vector<FSlideWindowItem>::iterator it	= SWIs.begin();
		xr_vector<FSlideWindowItem>::iterator it_e	= SWIs.end();

		for (; it != it_e; ++it)
		{
			i_swi_allocated -= it->count;
			xr_free(it->sw);
		}

		SWIs.clear_not_free();

		SWIs.resize			(item_count);
		for (u32 c=0; c<item_count; c++){
			FSlideWindowItem& swi = SWIs[c];
			swi.reserved[0]	= fs->r_u32();	
			swi.reserved[1]	= fs->r_u32();	
			swi.reserved[2]	= fs->r_u32();	
			swi.reserved[3]	= fs->r_u32();	
			swi.count		= fs->r_u32();
			VERIFY			(NULL==swi.sw);
			i_swi_allocated   += swi.count;
			swi.sw			= xr_alloc<FSlideWindow> (swi.count);
			fs->r			(swi.sw,sizeof(FSlideWindow)*swi.count);
		}
		fs->close			();
	}
}
