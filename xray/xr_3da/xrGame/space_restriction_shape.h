////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_shape.h
//	Created 	: 17.08.2004
//  Modified 	: 21.12.2014
//	Author		: Dmitriy Iassenev
//	Description : Space restriction shape
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "space_restriction_base.h"
#include "../xr_collide_form.h"

class CSpaceRestrictor;
class CSpaceRestrictionBridge;

class CSpaceRestrictionShape : public CSpaceRestrictionBase {
private:
	friend struct CBorderMergePredicate;
	friend class  CSpaceRestrictor;
	friend class  CSpaceRestrictionComposition;

public:
	using CSpaceRestrictionBase::inside;
	int						m_create_cycle; // alpet: для отладки времени появления
	u32						m_create_time;

protected:	
	bool					m_default;
protected:
	IC			Fvector		position				(const CCF_Shape::shape_def &data) const;
	IC			float		radius					(const CCF_Shape::shape_def &data) const;
				void		build_border			();
				void		fill_shape				(const CCF_Shape::shape_def &shape);

public:
	CSpaceRestrictionBridge  *m_bridge;			// alpet: leak control ref
	CSpaceRestrictor		 *m_restrictor;	    // alpet: unregister ref check

	IC						CSpaceRestrictionShape	(CSpaceRestrictor *space_restrictor, bool default_restrictor);		
	virtual				   ~CSpaceRestrictionShape  ();
	IC	virtual void		initialize				();
		virtual bool		inside					(const Fsphere &sphere);
		virtual shared_str	name					() const;
	IC	virtual bool		shape					() const;
	IC	virtual bool		default_restrictor		() const;
		virtual	Fsphere		sphere					() const;	

#ifdef DEBUG
				void		test_correctness		();
#endif
};

#include "space_restriction_shape_inline.h"
