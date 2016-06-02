////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_holder.cpp
//	Created 	: 17.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction holder
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "space_restriction_holder.h"
#include "object_broker.h"
#include "space_restrictor.h"
#include "space_restriction_bridge.h"
#include "space_restriction_shape.h"
#include "space_restriction_composition.h"
#include "restriction_space.h"
#include "xrServer_Objects_ALife.h"
#include "Level.h"


#pragma warning(push)
#pragma warning(disable:4995)
#include <malloc.h>
#pragma warning(pop)

const u32 time_to_delete = 300000;

#pragma optimize("gyts", off)
extern xr_map<u32, CSpaceRestrictionBridge*> g_bridges;
extern xr_map<shared_str, CSpaceRestrictor*> g_restrictors;

int i_holder_count = 0;


CSpaceRestrictionHolder::~CSpaceRestrictionHolder			()
{
	i_holder_count--;
	m_finalization				= true;

	Device.dwTimeGlobal += time_to_delete;

	collect_garbage();
	clear					();
	collect_garbage();
	m_restrictions.clear();
	Device.dwTimeGlobal -= time_to_delete;
}

void CSpaceRestrictionHolder::clear							()
{
	m_finalization				= true;
	auto it = m_restrictions.begin(); // memory leak re-test
	for (; it != m_restrictions.end(); it++)
	{
		auto *bridge = (*it).second;
		bridge->release();			
		bridge->m_last_time_dec = 0;  // for fast release
		it->second = NULL;
	}
	delete_data					(m_restrictions);
	m_default_out_restrictions	= "";
	m_default_in_restrictions	= "";
	collect_garbage ();
}

shared_str CSpaceRestrictionHolder::normalize_string		(shared_str space_restrictors)
{
	u32						n = xr_strlen(space_restrictors);
	if (!n)
		return				("");

	//1. parse the string, copying to temp buffer with leading zeroes, storing pointers in vector
	LPSTR					*strings = (LPSTR*)_alloca(MAX_RESTRICTION_PER_TYPE_COUNT*sizeof(LPSTR));
	LPSTR					*string_current = strings;

	LPSTR					temp_string = (LPSTR)_alloca((n+1)*sizeof(char));
	LPCSTR					I = *space_restrictors;
	LPSTR					i = temp_string, j = i;
	for ( ; *I; ++I, ++i) {
		if (*I != ',') {
			*i				= *I;
			continue;
		}

		*i					= 0;
		VERIFY				(u32(string_current - strings) < MAX_RESTRICTION_PER_TYPE_COUNT);
		*string_current		= j;
		++string_current;
		j					= i + 1;
	}
	if (string_current == strings)
		return				(space_restrictors);

	*i						= 0;
	VERIFY					(u32(string_current - strings) < MAX_RESTRICTION_PER_TYPE_COUNT);
	*string_current			= j;
	++string_current;

	//2. sort the vector (svector???)
	std::sort				(strings,string_current,pred_str());

	//3. copy back to another temp string, based on sorted vector
	LPSTR					result_string = (LPSTR)_alloca((n+1)*sizeof(char));
	LPSTR					pointer = result_string;
	{
		LPSTR				*I = strings;
		LPSTR				*E = string_current;
		for ( ; I != E; ++I) {
			for (LPSTR i = *I; *i; ++i, ++pointer)
				*pointer	= *i;

			*pointer		= ',';
			++pointer;
		}
	}
	*(pointer - 1)			= 0;

	//4. finally, dock shared_str
	return					(result_string);
}

SpaceRestrictionHolder::CBaseRestrictionPtr CSpaceRestrictionHolder::restriction	(shared_str space_restrictors)
{
	if (!xr_strlen(space_restrictors))
		return				(0);

	space_restrictors		= normalize_string(space_restrictors);
	CSpaceRestrictionBridge	*bridge = NULL;

	
	RESTRICTIONS::const_iterator	I = m_restrictions.find(space_restrictors);

	static int _in_register = 0;

	if (I != m_restrictions.end())
	{	
		bridge = I->second;	
		auto *impl = bridge->get_implementation();
		while (!impl->effective() && !_in_register)
		{			
			CSpaceRestrictionComposition *composition = smart_cast<CSpaceRestrictionComposition*>(impl);
			// 
			auto it = g_restrictors.find(space_restrictors);
			CSpaceRestrictor *SR = NULL;
			if (it != g_restrictors.end())
			{
				SR = it->second;
				if (!SR) return (bridge);				
				RestrictionSpace::ERestrictorTypes type = SR->restrictor_type();									
				// рестрикторы none не требуют регистрации?
				if (RestrictionSpace::eRestrictorTypeNone != type)
				{
					MsgCB("! #WARN: JIT restrictor registration, name = <%s>, type = %d ", *space_restrictors, int(type));
					_in_register++;
					bridge = register_restrictor(SR, type);
					_in_register--;
					return (bridge);
				}
				else
					MsgCB("# #DBG: restrictor %s have type incompatible with registration(?) - ignoring ", SR->Name_script());

			} // if (it found)

			if (!SR)
			{
				// очень долгий поиск(!!!!)
				CObject *R = Level().Objects.FindObjectByName(*space_restrictors);
				if (R) SR = smart_cast<CSpaceRestrictor*>(R);
				if (SR)
				{
					g_restrictors[space_restrictors] = SR;
					auto type = SR->restrictor_type();
					if (RestrictionSpace::eRestrictorTypeNone != type)	continue;
				}
			}

			if (composition)
				composition->m_type_none = true;
			break;
		}

		return	(bridge);
	}

	collect_garbage			();

	return register_fake_restrictor(space_restrictors);	
}

CSpaceRestrictionBridge* CSpaceRestrictionHolder::register_fake_restrictor(shared_str restrictor_id) // создание фейковой заглушки, композиции из одного элемента
{
	CSpaceRestrictionBase	*composition = xr_new<CSpaceRestrictionComposition>(this, restrictor_id);
	CSpaceRestrictionBridge *bridge = xr_new<CSpaceRestrictionBridge>(this, composition);
	m_restrictions.insert(std::make_pair(restrictor_id, bridge));
	RegisterBridge(bridge);
	collect_garbage();
	return			bridge;
}

CSpaceRestrictionBridge* CSpaceRestrictionHolder::register_restrictor				(CSpaceRestrictor *space_restrictor, const RestrictionSpace::ERestrictorTypes &restrictor_type)
{
	string4096					m_temp_string;
	shared_str					space_restrictors = space_restrictor->cName();	
	LPCSTR	name				= *space_restrictors;

	if (restrictor_type != RestrictionSpace::eDefaultRestrictorTypeNone) {
		shared_str				*temp = 0, temp1;
		if (restrictor_type == RestrictionSpace::eDefaultRestrictorTypeOut)
			temp			= &m_default_out_restrictions;
		else
			if (restrictor_type == RestrictionSpace::eDefaultRestrictorTypeIn)
				temp		= &m_default_in_restrictions;
			else
				NODEFAULT;
		temp1				= *temp;
		

		LPCSTR curr = temp->c_str();

		if (xr_strlen(curr) && xr_strlen(name) && !strstr(name, curr))
			strconcat		(sizeof(m_temp_string),  m_temp_string, curr, ",", name); // m_temp_string = curr + "," + name
		else
			strcpy_s		(m_temp_string, sizeof(m_temp_string), name);

		*temp				= normalize_string(m_temp_string);
		
		if (xr_strcmp(curr, temp1))
			on_default_restrictions_changed	();
	}
	
	
	CSpaceRestrictionShape	*shape = xr_new<CSpaceRestrictionShape>(space_restrictor,restrictor_type != RestrictionSpace::eDefaultRestrictorTypeNone);

	RESTRICTIONS::iterator	I = m_restrictions.find(space_restrictors);
	if (I == m_restrictions.end()) {
		CSpaceRestrictionBridge	*bridge = xr_new<CSpaceRestrictionBridge>(this, shape);
		shape->m_bridge			= bridge;
		m_restrictions.insert	(std::make_pair(space_restrictors,bridge));
		MsgCB("# #DBG: registering restrictor object <%s>, bridge = 0x%p, m_restrictions.size = %d ", *space_restrictors, bridge, m_restrictions.size());
		RegisterBridge			(bridge);
		return bridge;
	}

	xr_delete<CSpaceRestrictionShape>(shape); // debug memory leak
	shape = xr_new<CSpaceRestrictionShape>(space_restrictor,restrictor_type != RestrictionSpace::eDefaultRestrictorTypeNone);
	shape->m_bridge = I->second;
	auto *prv = I->second->get_implementation();
	MsgCB("# #DBG: replacing restrictor object <%s>, prev = 0x%p, new @shape = 0x%p ", *space_restrictors, prv, shape);
	(*I).second->change_implementation(shape); // замена композиции из одного элемента	
	return I->second;
}

bool try_remove_string				(shared_str &search_string, const shared_str &string_to_search)
{
	bool					found = false;
	string256				temp;
	string4096				temp1;
	*temp1					= 0;
	for (int i=0, j=0, n=_GetItemCount(*search_string); i<n; ++i, ++j) {
		if (xr_strcmp(string_to_search,_GetItem(*search_string,i,temp))) {
			if (j)
				strcat		(temp1,",");
			strcat			(temp1,temp);
			continue;
		}

		found				= true;
		--j;
	}

	if (!found)
		return				(false);

	search_string			= temp1;
	return					(true);
}

void CSpaceRestrictionHolder::unregister_restrictor			(CSpaceRestrictor *space_restrictor, LPCSTR szMsg)
{	
	RESTRICTIONS::iterator	I = m_restrictions.find(space_restrictor->cName());
	if (I == m_restrictions.end()) return;
	
	CSpaceRestrictionBridge *bridge = (*I).second;
	static shared_str restrictor_id;

	collect_garbage();

	if (bridge)
	__try
	{	
		restrictor_id = space_restrictor->cName();

		if (!bridge->shape() && !bridge->default_restrictor()) return; // уже заглушка, зачем её заменять?
		CSpaceRestrictionShape *shape = smart_cast<CSpaceRestrictionShape*> ( bridge->get_implementation() );
		if (shape && shape->m_restrictor != space_restrictor) return;

		if (szMsg)
			MsgCB("# #DBG: %s ", szMsg);
		space_restrictor->m_owner_shape = NULL; // обрубить ссылку

		(*I).second = NULL;
		m_restrictions.erase(I);

		CSpaceRestrictionComposition	*composition = xr_new<CSpaceRestrictionComposition>(this, restrictor_id);

		bridge->change_implementation(composition);
		if (RestrictionSpace::eRestrictorTypeNone == space_restrictor->restrictor_type())
			composition->m_type_none = true;
		
		/*
		if (shape)
		{
			bridge->release();
			bridge->_release(shape);			
			register_fake_restrictor(restrictor_id);
		}
		*/
		if (try_remove_string(m_default_out_restrictions, restrictor_id))
				on_default_restrictions_changed();
			else {
				if (try_remove_string(m_default_in_restrictions, restrictor_id))
					on_default_restrictions_changed();
			}

	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("! #EXCEPTION: CSpaceRestrictionHolder::unregister_restrictor id = <%s>  ", *restrictor_id);
	}
}

IC	void CSpaceRestrictionHolder::collect_garbage			()
{	
	RESTRICTIONS::iterator	I = m_restrictions.begin(), J;
	RESTRICTIONS::iterator	E = m_restrictions.end();
	for ( ; I != E; ) 
	{
		J				= I++;
		auto *bridge	= J->second;
		if ( 
			 UnusedBridge (bridge) &&
				(Device.dwTimeGlobal >= bridge->m_last_time_dec + time_to_delete  )
 		   ) 
		{	
			(*J).second = NULL;
			m_restrictions.erase(J);
		}
		
	}

	auto it = g_bridges.begin();

	while (it != g_bridges.end())
	{
		auto *bridge = it->second;	it++;
		if (UnusedBridge(bridge) &&
			 (Device.dwTimeGlobal >= bridge->m_last_time_dec + time_to_delete + 3500) )
			  DestroyBridge(bridge); // post-delete
	}

}
