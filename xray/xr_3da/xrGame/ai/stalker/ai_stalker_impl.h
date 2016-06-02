////////////////////////////////////////////////////////////////////////////
//	Module 		: ai_stalker_impl.h
//	Created 	: 25.02.2003
//  Modified 	: 07.12.2014
//	Author		: Dmitriy Iassenev
//	Description : AI Behaviour for monster "Stalker" (inline functions implementation)
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../../level.h"
#include "../../seniority_hierarchy_holder.h"
#include "../../team_hierarchy_holder.h"
#include "../../squad_hierarchy_holder.h"
#include "../../group_hierarchy_holder.h"
#include "../../effectorshot.h"

IC	CAgentManager &CAI_Stalker::agent_manager	() const
{	
	__try
	{
		auto &holder = Level().seniority_holder();
		auto *team = &holder.team(g_Team());
		R_ASSERT2(team, make_string("retuned NULL team for %d", g_Team()));
		auto *squad = &team->squad(g_Squad());
		R_ASSERT2(squad, make_string("retuned NULL squad for %d", g_Squad()));
		auto *group = &squad->group(g_Group());
		R_ASSERT2(group, make_string("retuned NULL group for %d", g_Group()));
		return	group->agent_manager();
	}
	__except (SIMPLE_FILTER)
	{
		Msg("#EXCEPTION in CAI_Stalker::agent_manager	()");
	}
	NODEFAULT;
}

IC	Fvector CAI_Stalker::weapon_shot_effector_direction	(const Fvector &current) const
{
	VERIFY			(weapon_shot_effector().IsActive());
	Fvector			result;
	weapon_shot_effector().GetDeltaAngle(result);

	float			y,p;
	current.getHP	(y,p);

	result.setHP	(-result.y + y, -result.x + p);

	return			(result);
}
