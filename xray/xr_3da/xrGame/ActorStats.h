////////////////////////////////////////////////////////////////////////////
//	Module 		: ActorStats.h
//	Created 	: 03.05.2015
//  Modified 	: 03.05.2015
//	Author		: Alexander Petrov
//	Description : Actor detailed game stats 
////////////////////////////////////////////////////////////////////////////

#pragma once
#include "../../build_config_defines.h"
#include "../../xrCore/FS.h"


/*
   Игровая статистика представляет собой постоянно дополняемый файл с игровыми событиями.
 По формату файл двоичный, делиться на записи-чанки с событиями (events).  
 Заголовочный чанк описывает содержимое файла: количество чанков, дату модификации.
 Каждая транзакция в файл фиксирует его в валидном состоянии, т.е. частичные данные хранятся только в памяти.
 Предполагается сохранение каждую минуту, т.е. группирующие чанки (records) фактически индексируются по времени 
 прохождения. При достаточно заметных размерах группирующие чанки подвергаются компрессии.

 Описываемые события.

 1. Загрузка и сохранение уровня - сохраняется имя уровня, имя сейва.
 2. Гибель ГГ, подсчет убийств монстров и неписей
 3. Носимый вес, текущая позиция, количество денег в моменте, стамина и здоровье


*/

#define  ACTOR_STATS

#define  STAT_EVENT_LOAD		0x100
#define  STAT_EVENT_SAVE		0x101
#define  STAT_EVENT_ANCHOR		0x102
#define  STAT_EVENT_SLEEP		0x10E
#define  STAT_EVENT_EAT			0x10F
#define  STAT_EVENT_DEATH		0x110
#define  STAT_EVENT_KILL		0x111
#define  STAT_EVENT_MOVE		0x112
#define  STAT_EVENT_HEALTH		0x113
#define  STAT_EVENT_POWER		0x114
#define  STAT_EVENT_MONEY		0x115
#define  STAT_EVENT_WEIGHT		0x116
#define  STAT_EVENT_MESSAGE		0x400


class CActorGameStats
{
protected:
	string_path		fname;
	string_path		wcopy;
	int				m_iRecords;						// total records in file
	CMemoryWriter  *m_collector;					// intermediate events storage
	IWriter		   *m_stats_file;
	__time64_t		m_last_check;
	__time64_t		m_last_flush;

	Fvector  		m_last_pos;	
	float			m_last_health;
	float			m_last_power;	
	float			m_last_weight;
	int 			m_last_money;
	

public:
	CActorGameStats();
	virtual			~CActorGameStats		();
	void			FlushRecord				(bool bForce = FALSE);
	IWriter		   *GetCache				()									{ return m_collector;  }
	void			AddSimpleEvent			(u32 tag, LPCSTR format, ...);
	void			OpenStatsFile			();
	int				RecordsCount			()									{ return m_iRecords; }
	void			OnActorUpdate			();
};


extern CActorGameStats		g_ActorGameStats;

extern void save_stat_event  (CActor *pActor, u32 tag, LPCSTR msg);