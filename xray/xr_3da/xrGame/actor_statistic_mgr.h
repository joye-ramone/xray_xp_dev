#pragma once
#include "actor_statistic_defs.h"

class CActorStatisticsWrapper;
class CActorStatisticMgr
{
private:
	CActorStatisticsWrapper*		m_actor_stats_wrapper;
	vStatSectionData&				GetStorage		();
public:
							CActorStatisticMgr		();
							~CActorStatisticMgr		();
	SStatSectionData&		GetSection				(const shared_str& key);
	
	void					AddPoints				(const shared_str& key, const shared_str& detail_key, const shared_str& str_value);
	void					AddPoints				(const shared_str& key, const shared_str& detail_key, s32 cnt, float pts);
	float					GetSectionPoints		(const shared_str& key);
	const vStatSectionData&	GetCStorage				();
};