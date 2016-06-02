////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_bridge.cpp
//	Created 	: 27.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction bridge
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "space_restriction_bridge.h"
#include "space_restriction_base.h"
#include "space_restriction_holder.h"
#include "ai_space.h"
#include "level_graph.h"
#include "profiler.h"

#pragma optimize("gyts", off)

xr_map<u32, CSpaceRestrictionBridge*> g_bridges;


void RegisterBridge (CSpaceRestrictionBridge *bridge)
{
	u32 _ptr = (u32)bridge;
	auto it = g_bridges.find(_ptr);
	if (it != g_bridges.end())
		return;
	g_bridges[_ptr] = bridge;
}

bool UnusedBridge(CSpaceRestrictionBridge *bridge)
{
	return  !bridge->in_use();
}

u32 UnregisterBridge(CSpaceRestrictionBridge *bridge)
{
	u32 _ptr = (u32)bridge;
	auto it = g_bridges.find(_ptr);
	if (it == g_bridges.end())
		return g_bridges.size();
	g_bridges.erase(it);
	return g_bridges.size();
}

void DestroyBridge (CSpaceRestrictionBridge *bridge)
{
	__try
	{
		UnregisterBridge(bridge);
		xr_delete(bridge);		
	}
	__except (SIMPLE_FILTER)
	{
		Msg("!#EXCEPTION: catched in DestroyBridge.");
	}
}

void  CheckBridgesLeak()
{
	if (!g_bridges.empty())
	{
		Msg("!#LEAK: not destroyed space restriction bridges count = %d ", g_bridges.size());
		auto it = g_bridges.begin();
		while (it != g_bridges.end())
		{
			CSpaceRestrictionBridge *bridge = it->second;
			it->second						= NULL;
			it++;
			xr_delete				(bridge);
		}		
	}

	g_bridges.clear();
}



CSpaceRestrictionBridge::~CSpaceRestrictionBridge		()
{
	if (m_ref_count)
		MsgCB("! #WARN: destroying CSpaceRestrictionBridge with m_ref_count = %d ", m_ref_count);

	xr_delete			(m_object); // удаление шейпа или композиции (CSpaceRestrictionShape/CSpaceRestrictionComposition)
	u32 rest = UnregisterBridge	(this); // не более чем предосторожность
	if (m_owner && !m_owner->m_finalization)
		MsgCB("$ #PERF: CSpaceRestrictionBridge destructor, rest = %d ", rest);
}

void CSpaceRestrictionBridge::change_implementation		(CSpaceRestrictionBase *object)
{
	xr_delete			(m_object);
	m_object			= object;
}

const xr_vector<u32> &CSpaceRestrictionBridge::border	() const
{
	return				(object().border());
}

bool CSpaceRestrictionBridge::initialized				() const
{
	return				(object().initialized());
}

void CSpaceRestrictionBridge::initialize				()
{
	auto &O = object();
	O.initialize	();
}

shared_str CSpaceRestrictionBridge::name				() const
{
	return				(object().name());
}

u32	CSpaceRestrictionBridge::accessible_nearest			(const Fvector &position, Fvector &result, bool out_restriction)
{
	return				(accessible_nearest(m_object,position,result,out_restriction));
}

bool CSpaceRestrictionBridge::shape						() const
{
	return				(object().shape());
}

bool CSpaceRestrictionBridge::default_restrictor		() const
{
	return				(object().default_restrictor());
}

Fsphere CSpaceRestrictionBridge::sphere					() const
{
	return				(object().sphere());
}

bool CSpaceRestrictionBridge::inside					(u32 level_vertex_id, bool partially_inside)
{
	START_PROFILE("Restricted Object/Bridge/Inside Vertex");
	return		(object().inside(level_vertex_id,partially_inside));
	STOP_PROFILE;
}

bool CSpaceRestrictionBridge::inside					(u32 level_vertex_id, bool partially_inside, float radius)
{
	START_PROFILE("Restricted Object/Bridge/Inside Vertex");
	R_ASSERT2 ((u32)this > 0x10000, "this is bad pointer!");
	return		(object().inside(level_vertex_id,partially_inside,radius));
	STOP_PROFILE;
}

bool CSpaceRestrictionBridge::inside					(const Fsphere &sphere)
{
	START_PROFILE("Restricted Object/Bridge/Inside Position");
	return		(object().inside(sphere));
	STOP_PROFILE;
}

struct CFindByXZ_predicate {
	IC	bool	operator()	(u32 vertex_id, u32 xz) const
	{
		return			(ai().level_graph().vertex(vertex_id)->position().xz() < xz);
	}
};

bool CSpaceRestrictionBridge::on_border					(const Fvector &position) const
{
	START_PROFILE("Restricted Object/Bridge/On Border");
	
	VERIFY					(ai().level_graph().valid_vertex_position(position));

	CLevelGraph::CPosition	pos = ai().level_graph().vertex_position(position);

	xr_vector<u32>::const_iterator E = object().border().end();
	xr_vector<u32>::const_iterator I = std::lower_bound	(
		object().border().begin(),
		object().border().end(),
		pos.xz(),
		CFindByXZ_predicate()
	);

	if ((I == E) || (ai().level_graph().vertex(*I)->position().xz() != pos.xz()))
		return			(false);

	for (I ; I != E; ++I) {
		if (ai().level_graph().vertex(*I)->position().xz() != pos.xz())
			break;

		if (_abs(ai().level_graph().vertex_plane_y(*I) - position.y) < 2.f)
			return	(true);
	}	

	return				(false);

	STOP_PROFILE;
}

bool CSpaceRestrictionBridge::out_of_border				(const Fvector &position)
{
	START_PROFILE("Restricted Object/Bridge/Out Of Border");

	VERIFY					(ai().level_graph().valid_vertex_position(position));

	u32						level_vertex_id = ai().level_graph().vertex_id(position);
	if (!ai().level_graph().valid_vertex_id(level_vertex_id))
		return				(true);

	Fsphere					temp;
	temp.R					= EPS_L;
	temp.P					= position;
	temp.P.y				= ai().level_graph().vertex_plane_y(level_vertex_id,position.x,position.z);
	return					(!inside(temp));

	STOP_PROFILE;
}
