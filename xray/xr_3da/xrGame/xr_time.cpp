#include "stdafx.h"
#include "xr_time.h"
#include "ui/UIInventoryUtilities.h"
#include "level.h"
#include "date_time.h"
#include "ai_space.h"
#include "alife_simulator.h"
#include "alife_time_manager.h"

#define sec2ms		1000
#define min2ms		60*sec2ms
#define hour2ms		60*min2ms
#define day2ms		24*hour2ms

ALife::_TIME_ID __game_time()
{
	return	(ai().get_alife() ? ai().alife().time().game_time() : Level().GetGameTime());
}

u32 get_time()
{
	return u32(__game_time() & u32(-1));
}

xrTime get_time_struct()
{
	return xrTime(__game_time());
}
void set_time_struct(xrTime *t)
{
	ALife::_TIME_ID  rt = t->raw_get();
	if (ai().get_alife())
	{
		const CALifeTimeManager *mgr = &ai().alife().time();
		((CALifeTimeManager*)(mgr))->set_game_time(rt);		  // dirty hack
	}
	else
		Level().SetGameTimeFactor(rt, Level().GetGameTimeFactor());
}


LPCSTR	xrTime::dateToString	(int mode)								
{ 
	return *InventoryUtilities::GetDateAsString(m_time,(InventoryUtilities::EDatePrecision)mode);
}
LPCSTR	xrTime::timeToString	(int mode)								
{ 
	return *InventoryUtilities::GetTimeAsString(m_time,(InventoryUtilities::ETimePrecision)mode);
}

void	xrTime::add				(const xrTime& other)					
{  
	m_time += other.m_time;				
}
void	xrTime::sub				(const xrTime& other)					
{  
	if(*this > other)
		m_time -= other.m_time; 
	else 
		m_time = 0;	
}

void	xrTime::setHMS			(int h, int m, int s)					
{ 
	set(1, 1, 1, h, m, s, 0);	
}

void	xrTime::setHMSms		(int h, int m, int s, int ms)			
{ 
	set(1, 1, 1, h, m, s, ms);	
}

void	xrTime::updHMSms		(int h, int m, int s, int ms)			
{ 
	u32 y, mo, d, hh, mm, ss, msec;
	split_time(m_time, y, mo, d, hh, mm, ss, msec);
	m_time = 0; 
	m_time += generate_time (y,mo,d, h,m,s,ms);
}

void	xrTime::updHMS(int h, int m, int s)
{
	updHMSms(h, m, s, 0);
}

void	xrTime::set				(int y, int mo, int d, int h, int mi, int s, int ms)
{ 
	m_time = 0;  // ?? wtf ??
	m_time += generate_time(y,mo,d,h,mi,s,ms);
}

void	xrTime::get				(u32 &y, u32 &mo, u32 &d, u32 &h, u32 &mi, u32 &s, u32 &ms)
{
	split_time(m_time,y,mo,d,h,mi,s,ms);
}

float	xrTime::diffSec			(const xrTime& other)					
{ 
	if(*this > other) 
		return (m_time-other.m_time) / (float)sec2ms; 
	return ((other.m_time - m_time) / (float)sec2ms)*(-1.0f); // а в чем смысл перестановки??	
}
