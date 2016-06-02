////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_monster_brain.cpp
//	Created 	: 06.10.2005
//  Modified 	: 22.11.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife monster brain class
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_monster_brain.h"
#include "object_broker.h"
#include "xrServer_Objects_ALife_Monsters.h"

XRCORE_API BOOL GetObjectInfo(PVOID pAddr, LPSTR name, LPDWORD pDisp);

#ifdef XRGAME_EXPORTS
#	include "alife_monster_movement_manager.h"
#	include "alife_monster_detail_path_manager.h"
#	include "alife_monster_patrol_path_manager.h"
#	include "ai_space.h"
#	include "ef_storage.h"
#	include "ef_primary.h"
#	include "alife_simulator.h"
#	include "alife_graph_registry.h"
#	include "movement_manager_space.h"
#	include "alife_smart_terrain_registry.h"
#	include "alife_time_manager.h"
#	include "date_time.h"
#	ifdef DEBUG
#		include "level.h"
#		include "map_location.h"
#		include "map_manager.h"
#	endif
#endif

#define MAX_ITEM_FOOD_COUNT			3
#define MAX_ITEM_MEDIKIT_COUNT		3
#define MAX_AMMO_ATTACH_COUNT		10

LPCSTR info_ptr(void *P)
{
	static string2048 info;
	DWORD disp;
	if (GetObjectInfo(P, info, &disp))
	{
		if (disp)
			sprintf_s(info + xr_strlen(info), 1024, "+ 0x%x", disp);
	}
	else
		sprintf_s(info, 1024, "Unknown at 0x%p", P);
	return info;
}

XRCORE_API LPCSTR format_time(LPCSTR format, __time64_t t, bool add_ms = true);
XRCORE_API __time64_t XrLocalTime64();
extern	   LPCSTR     se_obj_level_name(CSE_ALifeObject *O);

void LogicLog(LPCSTR format, ...)
{
	va_list mark;	
	va_start(mark, format);					
	char	buf[0x1000]; 
	int sz = _vsnprintf_s (buf, sizeof(buf) - 1, _TRUNCATE, format, mark); buf[sizeof(buf) - 1] = 0;
	va_end(mark);	
	static string_path log_file = { 0 };
	LPCSTR ts;
	__time64_t t = XrLocalTime64();	

	if (!strlen(log_file))
	{
		ts = format_time("%y%m%d_%H%M", t);
		FS.update_path(log_file, "$logs$",  xrx_sprintf("logic-%s.log", ts));
	}

	ts = format_time("%H:%M:%S", t);
	FILE *f = fopen(log_file, "a+t");
	fprintf(f, "[%s]. %s\n", ts, buf);
	fclose(f);
}



CALifeMonsterBrain::CALifeMonsterBrain		(object_type *object)
{
	R_ASSERT2						(object, "Попытка создать мозг монстра без объекта");
	m_object						= object;
	m_last_search_time				= 0;
	m_smart_terrain					= 0;
	m_debug							= !!strstr ( object->name(), "hellcar" );

#ifdef XRGAME_EXPORTS
	m_movement_manager				= xr_new<CALifeMonsterMovementManager>(object);
#endif
	
#ifdef XRGAME_EXPORTS
	u32								hours,minutes,seconds;
	sscanf							(pSettings->r_string(this->object().name(),"smart_terrain_choose_interval"),"%d:%d:%d",&hours,&minutes,&seconds);
	m_time_interval					= (u32)generate_time(1,1,1,hours,minutes,seconds);
#endif

	m_can_choose_alife_tasks		= true;
	m_default_behavior				= false;
	m_last_task						= "";
}

CALifeMonsterBrain::~CALifeMonsterBrain			()
{
#ifdef XRGAME_EXPORTS
	xr_delete						(m_movement_manager);
#endif
}

void CALifeMonsterBrain::on_state_write		(NET_Packet &packet)
{
}

void CALifeMonsterBrain::on_state_read		(NET_Packet &packet)
{
}

#ifdef XRGAME_EXPORTS

bool CALifeMonsterBrain::perform_attack		()
{
	return							(false);
}

ALife::EMeetActionType CALifeMonsterBrain::action_type	(CSE_ALifeSchedulable *tpALifeSchedulable, const int &iGroupIndex, const bool &bMutualDetection)
{
	return							(ALife::eMeetActionTypeIgnore);
}

void CALifeMonsterBrain::on_register			()
{
}

void CALifeMonsterBrain::on_unregister		()
{
}

void CALifeMonsterBrain::on_location_change	()
{
	Fvector &p = object().position();
	// object().synchronize_location();
	if (object().cast_human_abstract())
		LogicLog("#DBG: CALifeMonsterBrain::on_location_change object_name = %s, position now = { %.1f, %.1f, %.1f }, gvid = %d, level = '%s' ", 
				object().name_replace(), p.x, p.y, p.z, object().m_tGraphID, se_obj_level_name (m_object));

}

IC	CSE_ALifeSmartZone &CALifeMonsterBrain::smart_terrain	()
{
	u16 stid = object().m_smart_terrain_id;
	if (0xffff == stid)
	{
		LogicLog("#WARN: CALifeMonsterBrain::smart_terrain m_smart_terrain_id == 0xffff for object %s ", object().name_replace());
		m_smart_terrain = NULL;
		return (*m_smart_terrain);
	}
	if (m_smart_terrain && (stid == m_smart_terrain->ID))
		return						(*m_smart_terrain);

	m_smart_terrain					= ai().alife().smart_terrains().object(stid);
	FORCE_VERIFY					(m_smart_terrain);
	return							(*m_smart_terrain);
}

void CALifeMonsterBrain::process_task			() 
{	
	R_ASSERT(m_object);
	u16 stid = object().m_smart_terrain_id;		
	if (0xffff == stid) return;

	auto &zone = smart_terrain();
	LPCSTR o_name = object().name_replace();

	if (!m_smart_terrain)
	{		
		object().m_smart_terrain_id = 0xffff;		
		Msg("! #ERROR: CALifeMonsterBrain::process_task m_smart_terrain #%d == NULL for %s ", stid, o_name);				
		return;
	}

	Fvector &p = object().position();
	xr_string task_info = xr_sprintf("zone = %s, object_name = %s, position now = { %.1f, %.1f, %.1f }, gvid = %d, level = '%s' ", 
									  zone.name_replace(), o_name, p.x, p.y, p.z, object().m_tGraphID, se_obj_level_name (m_object));
	if (m_object->cast_human_abstract() && task_info != m_last_task)
		LogicLog("#DBG: CALifeMonsterBrain::process_task [ %s ]", task_info.c_str());

	m_last_task						= task_info;
	CALifeSmartTerrainTask			*task = zone.task(&object());
	R_ASSERT3						(task, "smart terrain returned nil task, while npc is registered in it", zone.name_replace());
	movement().path_type			(MovementManager::ePathTypeGamePath);
	movement().detail().target		(*task);
}

void CALifeMonsterBrain::select_task()
{
	if (object().m_smart_terrain_id != 0xffff) // уже назначен?
	{
		smart_terrain();
		return;
	}
	/// здесь объекту назначается смарт-террейн, если у него до сих пор оного не было
	
	if (!can_choose_alife_tasks())
		return;

	ALife::_TIME_ID					current_time = ai().alife().time_manager().game_time();

	if (m_last_search_time + m_time_interval > current_time)
		return;

	m_last_search_time				= current_time;

	float							best_value = flt_min;
	auto &list = ai().alife().smart_terrains().objects();
	auto I = list.begin();
	auto E = list.end();
	for ( ; I != E; ++I) {
		if (!(*I).second->enabled(&object()))
			continue;

		float						value = (*I).second->suitable(&object());
		if (value > best_value) {
			best_value				= value;
			object().m_smart_terrain_id	= (*I).second->ID;
		}
	}

	u16 stid = object().m_smart_terrain_id;
	if (stid != 0xffff) {
		auto &st = smart_terrain();
		R_ASSERT2 (m_smart_terrain, make_string("cannot find smart_terrain %d", stid));
		st.register_npc	(&object());
		m_last_search_time				= 0;
		LogicLog("#DBG: for object %s assigned smart_terrain %s (%d), last_task:\n\t %s ", object().name_replace(), st.name_replace(), st.ID, m_last_task.c_str());
	}
}

void CALifeMonsterBrain::update				()
{
	R_ASSERT2(m_object, "CALifeMonsterBrain::update - мозг есть, а объекта монстра нет.");
	
#if 0//def DEBUG
	if (!Level().MapManager().HasMapLocation("debug_stalker",object().ID)) {
		CMapLocation				*map_location = 
			Level().MapManager().AddMapLocation(
				"debug_stalker",
				object().ID
			);

		map_location->SetHint		(object().name_replace());
	}
#endif

	__try
	{
		select_task();
		u16 stid = object().m_smart_terrain_id;
		if (stid != 0xffff)
		{
			R_ASSERT2(m_smart_terrain, make_string("id %d specified, but object not assigned ", stid));
			process_task();
			m_default_behavior = false;
		}
		else
		{
			default_behaviour();
			if (object().cast_human_abstract() && !m_default_behavior)
				LogicLog("#DBG: for %s used default behavior", object().name_replace());
			m_default_behavior = true;
		}

		movement().update();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{		
		Msg("! #EXCEPTION: энцефалопатия моба детектед, object = %s, last_task:\n\t %s", info_ptr(m_object), m_last_task.c_str());		
		Beep(300, 100);
		if (m_object &&  u32(m_object) >> 16 != 0xeeee && !IsBadReadPtr(m_object, 100))
			Msg("! object.name = %s", m_object->name_replace());
		else
			m_object = NULL;
	}
}

void CALifeMonsterBrain::default_behaviour	()
{
	movement().path_type			(MovementManager::ePathTypeNoPath);
}

void CALifeMonsterBrain::on_switch_online	()
{
	movement().on_switch_online		();
}

void CALifeMonsterBrain::on_switch_offline	()
{
	movement().on_switch_offline	();
}

#endif // XRGAME_EXPORTS