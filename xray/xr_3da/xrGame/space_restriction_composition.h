////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_composition.h
//	Created 	: 17.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction composition
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "space_restriction_base.h"
#include "space_restriction_holder.h"

class CSpaceRestrictionBridge;
class CSpaceRestrictionHolder;

extern int g_restriction_checker;

class CSpaceRestrictionComposition : public CSpaceRestrictionBase {
public:
	using CSpaceRestrictionBase::inside;

protected:
	typedef SpaceRestrictionHolder::CBaseRestrictionPtr CBaseRestrictionPtr;
	typedef xr_vector<CBaseRestrictionPtr> RESTRICTIONS;

protected:
	int						m_create_cycle;
	float					m_create_time;

	RESTRICTIONS			m_restrictions;
	shared_str				m_space_restrictors;
	CSpaceRestrictionHolder	*m_space_restriction_holder;
	Fsphere					m_sphere;
	int						m_init_count;	

#ifdef DEBUG
private:
				void		check_restrictor_type			();
#endif // DEBUG

protected:
	IC			void		merge							(CBaseRestrictionPtr restriction);

public:
	bool					m_incomplete;
	bool					m_type_none;					// заглушка для default-none и none рестрикторов (не in/out)

	IC						CSpaceRestrictionComposition	(CSpaceRestrictionHolder *space_restriction_holder, shared_str space_restrictors);
		virtual				~CSpaceRestrictionComposition	();
		virtual	bool		effective						() const;
		virtual void		initialize						();
		virtual bool		inside							(const Fsphere &sphere);
	IC	virtual shared_str	name							() const;
	IC	virtual bool		shape							() const;
	IC	virtual bool		default_restrictor				() const;
		virtual	Fsphere		sphere							() const;
				
#ifdef DEBUG
				void		test_correctness				();
#endif
};

#include "space_restriction_composition_inline.h"
