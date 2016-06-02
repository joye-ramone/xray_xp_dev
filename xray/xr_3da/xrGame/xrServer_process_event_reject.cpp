#include "stdafx.h"
#include "xrserver.h"
#include "xrserver_objects.h"
#include "../lua_tools.h"
#include "script_engine.h"

#pragma optimize("gyts", off)

bool xrServer::Process_event_reject	(NET_Packet& P, const ClientID sender, const u32 time, const u16 id_parent, const u16 id_entity, bool send_message)
{
	// Parse message
	CSE_Abstract*		e_parent	= game->get_entity_from_eid	(id_parent);
	CSE_Abstract*		e_entity	= game->get_entity_from_eid	(id_entity);
	
#ifdef DEBUG
	Msg("sv reject. id_parent %s id_entity %s [%d]",ent_name_safe(id_parent).c_str(),ent_name_safe(id_entity).c_str(), Device.dwFrame);
#endif
	if (!e_parent)
	{
		Msg("!#ERROR: xrServer::Process_event_reject no parent object found for id %d ", id_parent);
		return (false);
	}

	R_ASSERT			(e_parent && e_entity);
	game->OnDetach		(id_parent,id_entity);

	static int err_count = 0;

	if (0xffff == e_entity->ID_Parent) 
	{
		MsgV	("7PICKUP", "! ERROR: can't detach independant object. entity[%s:%d], parent[%s:%d], section[%s]",
			e_entity->name_replace(),id_entity,e_parent->name_replace(),id_parent, *e_entity->s_name);
		// if (game_lua())	Msg("~ %s", get_lua_traceback(game_lua(), 2));
		return			(false);
	}

	// Rebuild parentness
	if (e_entity->ID_Parent != id_parent)
	{		
		Msg("!------------------------------------------------------------------------------");
		Msg ("!#ERROR: Cannot drop/reject %s not owned by %s, current owner id = %d ",
					 e_entity->name_replace(), e_parent->name_replace(), e_entity->ID_Parent );
		MsgCB("$#DUMP_CONTEXT");
		Msg(" lua stack %s", GetLuaTraceback());		
		Msg("!------------------------------------------------------------------------------");
	  	if (0 != e_entity->ID_Parent)  err_count++; // веро€тно проблемы с скрытым инвентарем оп€ть. 
		if (err_count > 50)
			Debug.fatal(DEBUG_INFO, "To many errors occured");
		return (false);
	}
	if (err_count > 0)
	    err_count--;

	e_entity->ID_Parent		= 0xffff;
	xr_vector<u16>& C		= e_parent->children;

	xr_vector<u16>::iterator c	= std::find	(C.begin(),C.end(),id_entity);
	if (C.end()!=c)  
		C.erase					(c);
	else
		Msg("! #FAIL: child %s not found in parent %s", e_entity->name_replace(), e_parent->name_replace());

	// Signal to everyone (including sender)
	if (send_message)
	{
		DWORD MODE		= net_flags(TRUE,TRUE, FALSE, TRUE);
		SendBroadcast	(BroadcastCID,P,MODE);
	}
	
	return				(true);
}
