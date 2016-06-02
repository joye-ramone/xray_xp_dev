////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_base.cpp
//	Created 	: 17.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction base
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "space_restriction_base.h"
#include "ai_space.h"
#include "level_graph.h"

#define DEFAULT_RADIUS EPS_L

bool CSpaceRestrictionBase::effective() const
{
	return							true;
}

bool CSpaceRestrictionBase::inside	(u32 level_vertex_id, bool partially_inside)
{
	return							(inside(level_vertex_id,partially_inside,DEFAULT_RADIUS));
}

IC	Fvector construct_position		(u32 level_vertex_id, float x, float z)
{
	return							(Fvector().set(x,ai().level_graph().vertex_plane_y(level_vertex_id,x,z),z));
}

IC	bool CSpaceRestrictionBase_inside	(CSpaceRestrictionBase *self, const Fvector &position, const float &radius)
{
	R_ASSERT2 (!IsBadReadPtr(self, sizeof(CSpaceRestrictionBase)), "this is bad pointer!");
	Fsphere							sphere;	
	sphere.P						= position;
	sphere.R						= radius;
	return							(self->inside(sphere));
}

bool CSpaceRestrictionBase::inside	(u32 level_vertex_id, bool partially_inside, float radius)
{
	const Fvector					&position = ai().level_graph().vertex_position(level_vertex_id);
	float							offset = ai().level_graph().header().cell_size()*.5f - EPS_L;
	R_ASSERT2 (!IsBadReadPtr(this, sizeof(CSpaceRestrictionBase)), "this is bad pointer!");
	m_last_inside = false;

	__try
	{
		if (partially_inside)
			m_last_inside = (
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x + offset, position.z + offset), radius) ||
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x + offset, position.z - offset), radius) ||
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x - offset, position.z + offset), radius) ||
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x - offset, position.z - offset), radius) ||
			CSpaceRestrictionBase_inside(this, Fvector().set(position.x, position.y, position.z), radius)
			);
		else
			m_last_inside = (
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x + offset, position.z + offset), radius) &&
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x + offset, position.z - offset), radius) &&
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x - offset, position.z + offset), radius) &&
			CSpaceRestrictionBase_inside(this, construct_position(level_vertex_id, position.x - offset, position.z - offset), radius) &&
			CSpaceRestrictionBase_inside(this, Fvector().set(position.x, position.y, position.z), radius)
			);
	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("! #EXCEPTION: catched in CSpaceRestrictionBase(0x%p)::inside, position = { %.1f, %.1f, %.1f }, lvid = %d ", 
					this, position.x, position.y, position.z, level_vertex_id );
	}
	if (m_last_inside)
		__asm nop;
	 
	return m_last_inside;
}

struct SortByXZ_predicate {
	IC	bool	operator()	(u32 v0, u32 v1) const
	{
		return						(ai().level_graph().vertex(v0)->position().xz() < ai().level_graph().vertex(v1)->position().xz());
	}
};

void CSpaceRestrictionBase::process_borders			()
{
	std::sort					(m_border.begin(),m_border.end());
	m_border.erase				(
		std::unique(
			m_border.begin(),
			m_border.end()
		),
		m_border.end()
	);
	std::sort					(m_border.begin(),m_border.end(),SortByXZ_predicate());
}
