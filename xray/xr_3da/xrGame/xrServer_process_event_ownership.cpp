#include "stdafx.h"
#include "xrserver.h"
#include "xrserver_objects.h"
#include "../luaicp_events.h"
#include "../../build_config_defines.h"

#pragma optimize("gyts", off)

void ReplaceOwnershipHeader	(NET_Packet& P)
{
	//способ очень грубый, но на данный момент иного выбора нет. Заранее приношу извинения
	u16 NewType = GE_OWNERSHIP_TAKE;
	CopyMemory(&P.B.data[6],&NewType,2);
};

void xrServer::Process_event_ownership(NET_Packet& P, ClientID sender, u32 time, u16 ID, BOOL bForced)
{
	u32 MODE			= net_flags(TRUE,TRUE, FALSE, TRUE);

	u16					id_parent=ID,id_entity;
	P.r_u16				(id_entity);
	CSE_Abstract*		e_parent	= game->get_entity_from_eid	(id_parent);
	CSE_Abstract*		e_entity	= game->get_entity_from_eid	(id_entity);

	if (id_parent == e_entity->ID_Parent)
	{
		MsgCB("$#CONTEXT: repeat event ownership for object %s to parent %s skipped! ", ent_name_safe(id_entity).c_str(), ent_name_safe(id_parent).c_str());
		return; // already parent		
    }

#ifdef DEBUG
	Msg("sv ownership id_parent %s id_entity %s [%d]",ent_name_safe(id_parent).c_str(), ent_name_safe(id_entity).c_str(), Device.dwFrame);
#else
	
	if (e_entity)
		MsgCB("$#CONTEXT: event ownership for object %s to parent %s, current owner = %d ", ent_name_safe(id_entity).c_str(), ent_name_safe(id_parent).c_str(), (int) e_entity->ID_Parent);
	else
		MsgCB("$#CONTEXT: event ownership for object %s to parent %s ", ent_name_safe(id_entity).c_str(), ent_name_safe(id_parent).c_str());

#endif


	if(!e_entity)		return;

	if (P.r_elapsed() >= 2)	P.r_u16(); // callback suppres flag

	R_ASSERT			(/*e_entity &&*/ e_parent);
	
	if (0xffff != e_entity->ID_Parent)
	{
		MsgCB("$#CONTEXT: sv !ownership (entity already has parent) new_parent %s id_parent %s id_entity %s [%d]",
					ent_name_safe(e_entity->ID_Parent).c_str(), ent_name_safe(id_parent).c_str(), ent_name_safe(id_entity).c_str(), Device.dwFrame);
		return;
	}

	#ifdef LUAICP_COMPAT
	// для обновлений реестра объектов в перехватчике
	// MsgCB("sv ownership id_parent %s id_entity %s [%d]", ent_name_safe(id_parent).c_str(), ent_name_safe(id_entity).c_str(), Device.dwFrame);
	process_object_event(EVT_OBJECT_PARENT | EVT_OBJECT_SERVER, id_entity, NULL, e_entity, id_parent);
	#endif

	xrClientData*		c_parent		= e_parent->owner;
	xrClientData*		c_entity		= e_entity->owner;
	xrClientData*		c_from			= ID_to_client	(sender);

	if ( (GetServerClient() != c_from) && (c_parent != c_from) )
	{
		// trust only ServerClient or new_ownerClient
		return;
	}

	// Game allows ownership of entity
	if (game->OnTouch	(id_parent,id_entity, bForced))
	{

		// Perform migration if needed
		if (c_parent != c_entity)		PerformMigration(e_entity,c_entity,c_parent);

		// Rebuild parentness
		e_entity->ID_Parent			= id_parent;
		e_parent->children.push_back(id_entity);

		if (bForced)
		{
			ReplaceOwnershipHeader(P);
		}
		// Signal to everyone (including sender)
		SendBroadcast		(BroadcastCID,P,MODE);
	}

}
