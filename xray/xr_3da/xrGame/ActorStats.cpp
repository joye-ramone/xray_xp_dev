////////////////////////////////////////////////////////////////////////////
//	Module 		: ActorStats.h
//	Created 	: 03.05.2015
//  Modified 	: 03.05.2015
//	Author		: Alexander Petrov
//	Description : Actor detailed game stats 
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "Actor.h"
#include "ActorCondition.h"
#include "ActorStats.h"

CActorGameStats		g_ActorGameStats;

#pragma optimize("gyts", off)

void w_timestamp(IWriter *w)
{
	__time64_t ts;
	_time64(&ts);
	w->w_u64(ts);          // path update time
}

CActorGameStats::CActorGameStats()
{
	m_collector = xr_new<CMemoryWriter>();
	m_iRecords = 0;
	m_stats_file = NULL;
	fname[0] = 0;
	wcopy[0] = 0;
	// OpenStatsFile();	
}

CActorGameStats::~CActorGameStats()
{	
	if (m_stats_file)
		FS.w_close(m_stats_file);
	xr_delete(m_collector);
	// xr_delete(m_stats_file);
}

void CActorGameStats::AddSimpleEvent(u32 tag, LPCSTR format, ...)
{
	m_collector->open_chunk(tag);

	va_list list;
	va_start(list, format);
	char buf[16384];
	_vsnprintf(buf, sizeof(buf)-1, format, list); 
	buf[sizeof(buf)-1]=0;
	va_end(list);
	w_timestamp(m_collector);
	m_collector->w_u32(xr_strlen(buf));	
	m_collector->w_stringZ(buf);
	m_collector->close_chunk();
}
void CActorGameStats::FlushRecord(bool bForce)
{
	if (!m_collector->size() || !m_stats_file) return;
	__try
	{

		__time64_t  ts;
		_time64(&ts);
		if (!bForce && ts - m_last_flush < 60) return;
		m_last_flush = ts;

		m_iRecords++;
		m_stats_file->open_chunk(m_iRecords);
		m_stats_file->w(m_collector->pointer(), m_collector->size());
		m_stats_file->close_chunk();
		m_stats_file->flush();
		// =========== patching file header ==========
		u32 pos = m_stats_file->tell();
		m_stats_file->seek(8);			 // skip chunk id & size
		m_stats_file->w_u32(m_iRecords); // patch count
		m_stats_file->w_u32(0x10001);    // file version
		m_stats_file->w_u64(ts);
		R_ASSERT(xr_strlen(Core.UserName) < 32); // to long user name
		m_stats_file->w_stringZ(Core.UserName);
		m_stats_file->flush();
		m_stats_file->seek(pos);			  // go back 
		// =========== patching file header ==========

		FS.rescan_pathes();
		// FS.file_copy(wcopy, fname);       // sync file
		CopyFile(wcopy, fname, FALSE);
		m_collector->clear();
	}
	__except (SIMPLE_FILTER)
	{
		Msg("#EXCEPTION: catched in CActorGameStats::FlushRecord");
	}
}

void CActorGameStats::OpenStatsFile()
{
#define BLOCK_SIZE 0x10000
	// 
	if (m_stats_file) return;  // уже открыт

	if (!xr_strlen(wcopy))
	{
		FS.update_path(fname, "$profile_dir$", "actor_stats.dat");
		FS.update_path(wcopy, "$profile_dir$", "actor_stats.wrk");
	}

	m_stats_file = FS.w_open(wcopy); // создание рабочей копии
	IReader *r = NULL;
	if (FS.exist(fname))
	{   // считывание данных заголовка файла статистики
		r = FS.r_open(fname);
		u32 sz = r->find_chunk(0);
		if (sz > 4)
		{
			m_iRecords = r->r_u32();
			r->advance64(sz - 4); // skip other fields
		}
	}


	m_stats_file->open_chunk(0);
	m_stats_file->w_u32(m_iRecords);
	m_stats_file->w_u32(0x10001);    // file version	
	w_timestamp(m_stats_file);
	char tmp[32];
	ZeroMemory(&tmp, sizeof(tmp));
	m_stats_file->w(&tmp, sizeof(tmp));  // user name reserved
	m_stats_file->close_chunk();

	int rest = r ? r->elapsed() : 0;
	// простое копирование контента крупными блоками
	while (rest > 0)
	{
		char buff[BLOCK_SIZE];
		int need = _min(rest, BLOCK_SIZE);
		r->r(&buff, need);
		m_stats_file->w(&buff, need);
		rest -= need;
	}

	if (r) FS.r_close(r);
}


void	CActorGameStats::OnActorUpdate()
{
	CActor *pA = Actor();
	if (!pA) return;	
	__time64_t ts;
	_time64(&ts);
	if (ts == m_last_check) return;
	m_last_check = ts;

	Fvector &p = pA->Position();
	if (p.distance_to(m_last_pos) > 1.0f)
	{
		m_last_pos = p;
		m_collector->open_chunk(STAT_EVENT_MOVE);
		w_timestamp(m_collector);
		m_collector->w_fvector3(p);
		m_collector->w_u16(pA->game_vertex_id());
		m_collector->w_u32(pA->level_vertex_id());
		m_collector->close_chunk();
	}

	float h = pA->GetHealth();
	if (abs(h - m_last_health) > 0.05f)
	{
		m_last_health = h;
		m_collector->open_chunk(STAT_EVENT_HEALTH);
		w_timestamp(m_collector);
		m_collector->w_float(h);
		m_collector->close_chunk();
	}

	float pp = pA->conditions().GetPower();
	if (abs(pp - m_last_power) > 0.1f)
	{
		m_last_power = pp;
		m_collector->open_chunk(STAT_EVENT_POWER);
		w_timestamp(m_collector);
		m_collector->w_float(pp);
		m_collector->close_chunk();
	}

	float w = pA->GetCarryWeight();
	if (abs(w - m_last_weight) > 0.5f)
	{
		m_last_weight = w;
		m_collector->open_chunk(STAT_EVENT_WEIGHT);
		w_timestamp(m_collector);
		m_collector->w_float(w);
		m_collector->close_chunk();		
	}

	int m = pA->get_money();
	if (abs(m - m_last_money) > 100)
	{
		m_last_money = m;
		m_collector->open_chunk(STAT_EVENT_MONEY);
		w_timestamp(m_collector);
		m_collector->w_u32(m);
		m_collector->close_chunk();
	}
 
}


void save_stat_event(CActor *pActor, u32 tag, LPCSTR msg)
{
	R_ASSERT(tag > 0);
	g_ActorGameStats.AddSimpleEvent(tag, "%s", msg);
}