////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_shape_inline.h
//	Created 	: 17.08.2004
//  Modified 	: 21.12.2014
//	Author		: Dmitriy Iassenev
//	Description : Space restriction shape inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "space_restrictor.h"

extern int g_game_cycle;

IC	Fvector	CSpaceRestrictionShape::position		(const CCF_Shape::shape_def &data) const
{
	switch (data.type) {
		case 0	: return(data.data.sphere.P);
		case 1	: return(data.data.box.c);
		default : NODEFAULT;
	}
#ifdef DEBUG
	return	(Fvector().set(0.f,0.f,0.f));
#endif
}

IC	float CSpaceRestrictionShape::radius			(const CCF_Shape::shape_def &data) const
{
	switch (data.type) {
		case 0	: return(data.data.sphere.R);
		case 1	: return(Fbox().set(Fvector().set(-.5f,-.5f,-.5f),Fvector().set(.5f,.5f,.5f)).xform(data.data.box).getradius());
		default : NODEFAULT;
	}
#ifdef DEBUG
	return	(0.f);
#endif
}

IC	CSpaceRestrictionShape::CSpaceRestrictionShape	(CSpaceRestrictor *space_restrictor, bool default_restrictor)
{
	m_default					= default_restrictor;
	m_initialized				= true;
	m_bridge					= NULL;

	R_ASSERT					(space_restrictor);
	m_restrictor				= space_restrictor;
	m_restrictor->m_owner_shape = this;	

	m_create_cycle				= g_game_cycle;
	m_create_time				= Device.dwTimeGlobal;
	
	build_border				();
}

IC	void CSpaceRestrictionShape::initialize			()
{
	FORCE_VERIFY				(m_initialized);
}

IC	bool CSpaceRestrictionShape::shape				() const
{
	return						(true);
}

IC	bool CSpaceRestrictionShape::default_restrictor	() const
{
	return						(m_default);
}
