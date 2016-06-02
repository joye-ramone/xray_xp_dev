////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_bridge.h
//	Created 	: 27.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction bridge
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "restriction_space.h"

class CSpaceRestrictionBase;
class CSpaceRestrictionHolder;

class CSpaceRestrictionBridge : public RestrictionSpace::CTimeIntrusiveBase {
protected:
	CSpaceRestrictionBase			*m_object;
	CSpaceRestrictionHolder			*m_owner;
	bool							m_used;
public:
	int								m_create_cycle;
	float							m_create_time;

	IC		CSpaceRestrictionBase	&object						() const;

public:
	IC								CSpaceRestrictionBridge		(CSpaceRestrictionHolder *owner, CSpaceRestrictionBase *object);
	virtual							~CSpaceRestrictionBridge	();
			void					change_implementation		(CSpaceRestrictionBase *object);
			const xr_vector<u32>	&border						() const;
	IC		CSpaceRestrictionBase*	get_implementation			() { return m_object; }	
			bool					initialized					() const;
			void					initialize					();
			bool					inside						(const Fsphere &sphere);
			bool					inside						(u32 level_vertex_id, bool partially_inside);
			bool					inside						(u32 level_vertex_id, bool partially_inside, float radius);
			shared_str				name						() const;
			u32						accessible_nearest			(const Fvector &position, Fvector &result, bool out_restriction);
			bool					shape						() const;
			bool					default_restrictor			() const;
			bool					on_border					(const Fvector &position) const;
			bool					out_of_border				(const Fvector &position);
			Fsphere					sphere						() const;
	IC		bool					in_use						() const { return m_used || m_ref_count; };

	IC		void					release						() { m_used = false; };
	


	template <typename T>
	IC		u32						accessible_nearest			(T &restriction, const Fvector &position, Fvector &result, bool out_restriction);
	template <typename T>
	IC		const xr_vector<u32>	&accessible_neighbour_border(T &restriction, bool out_restriction);
};

#include "space_restriction_bridge_inline.h"


// alpet: anti-leak stuff
void					CheckBridgesLeak			();
void					DestroyBridge				(CSpaceRestrictionBridge *bridge);
void					RegisterBridge				(CSpaceRestrictionBridge *bridge);
bool					UnusedBridge				(CSpaceRestrictionBridge *bridge);
u32						UnregisterBridge			(CSpaceRestrictionBridge *bridge); 