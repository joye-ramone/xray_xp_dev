////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 16.08.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "base_client_classes.h"
#include "derived_client_classes.h"
#include "HUDManager.h"
#include "exported_classes_def.h"
#include "script_game_object.h"
#include "ui/UIDialogWnd.h"
#include "ui/UIInventoryWnd.h"


using namespace luabind;
#pragma optimize("s", on)

extern LPCSTR get_lua_class_name(luabind::object O);

IC void alive_entity_set_radiation(CEntityAlive *E, float value)
{
	E->SetfRadiation(value);
}

void CEntityScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEntity, CGameObject>("CEntity")
			,
			class_<CEntityAlive, CEntity>("CEntityAlive")
			.property("radiation"			,					&CEntityAlive::g_Radiation, &alive_entity_set_radiation) // доза в %
			.property("condition"           ,                   &CEntityAlive::conditions)
		];
}

void CEatableItemScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEatableItem, CInventoryItem>("CEatableItem")
			.def_readwrite("eat_health"					,			&CEatableItem::m_fHealthInfluence)
			.def_readwrite("eat_power"					,			&CEatableItem::m_fPowerInfluence)
			.def_readwrite("eat_satiety"				,			&CEatableItem::m_fSatietyInfluence)
			.def_readwrite("eat_radiation"				,			&CEatableItem::m_fRadiationInfluence)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_fMaxPowerUpInfluence)
			.def_readwrite("wounds_heal_perc"			,			&CEatableItem::m_fWoundsHealPerc)
			.def_readwrite("eat_portions_num"			,			&CEatableItem::m_iPortionsNum)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_iStartPortionsNum)			
			,
			class_<CEatableItemObject, bases<CEatableItem, CGameObject>>("CEatableItemObject")
		];
}

void set_io_money(CInventoryOwner *IO, u32 money) { IO->set_money(money, true); }

CScriptGameObject  *item_lua_object(PIItem itm)
{	
	if (itm)
	{
		CGameObject *obj = smart_cast<CGameObject *>(itm);
		if (obj) return obj->lua_game_object();
	}
	return NULL;
}


CScriptGameObject  *inventory_active_item(CInventory *I)	{ return item_lua_object (I->ActiveItem()); }
CScriptGameObject  *inventory_selected_item(CInventory *I)
{
	CUIDialogWnd *IR = HUD().GetUI()->MainInputReceiver();
	if (!IR) return NULL;
	CUIInventoryWnd *wnd = smart_cast<CUIInventoryWnd *>(IR);
	if (!wnd) return NULL;
	if (wnd->GetInventory() != I) return NULL;		
	return item_lua_object( wnd->CurrentIItem() );	
}

CScriptGameObject  *get_inventory_target(CInventory *I)		{ return item_lua_object(I->m_pTarget); }


void CInventoryScript::script_register(lua_State *L)
{
	module(L)
		[

			class_<CInventoryItem>("CInventoryItem")
			.def_readonly("item_place"					,			&CInventoryItem::m_eItemPlace)
			.property("class_name"						,			&get_lua_class_name),
			class_<CInventoryItemObject, bases<CInventoryItem, CGameObject>>("CInventoryItemObject"),

			class_ <CInventory>("CInventory")
			.def_readwrite("max_belt"					,			&CInventory::m_iMaxBelt)
			.def_readwrite("max_weight"					,			&CInventory::m_fMaxWeight)
			.def_readwrite("take_dist"					,			&CInventory::m_fTakeDist)
			.def_readonly ("total_weight"				,			&CInventory::m_fTotalWeight)
			.property	  ("active_item"				,			&inventory_active_item)
			.property	  ("selected_item"				,			&inventory_selected_item)
			.property	  ("target"						,			&get_inventory_target)
			.property	  ("class_name"					,			&get_lua_class_name)
			,
			class_<CInventoryOwner>("CInventoryOwner")
			.def_readonly ("inventory"					,			&CInventoryOwner::m_inventory)
			.def_readonly ("talking"					,			&CInventoryOwner::m_bTalking)
			.def_readwrite("allow_talk"					,			&CInventoryOwner::m_bAllowTalk)
			.def_readwrite("allow_trade"				,			&CInventoryOwner::m_bAllowTrade)
			.def_readwrite("raw_money"					,			&CInventoryOwner::m_money)
			.property	  ("money"						,			&CInventoryOwner::get_money,				&set_io_money)			
			.property	  ("class_name"					,			&get_lua_class_name)			
			
		];
}

void CMonsterScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CBaseMonster, bases<CInventoryOwner, CEntityAlive>>("CBaseMonster")
			.def_readwrite("agressive"					,			&CBaseMonster::m_bAggressive)
			.def_readwrite("angry"						,			&CBaseMonster::m_bAngry)
			.def_readwrite("damaged"					,			&CBaseMonster::m_bDamaged)
			.def_readwrite("grownlig"					,			&CBaseMonster::m_bGrowling)						
			.def_readwrite("run_turn_left"				,			&CBaseMonster::m_bRunTurnLeft)
			.def_readwrite("run_turn_right"				,			&CBaseMonster::m_bRunTurnRight)
			.def_readwrite("sleep"						,			&CBaseMonster::m_bSleep)
			.def_readwrite("state_invisible"			,			&CBaseMonster::state_invisible)



		];
}

void CWeaponScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CWeapon,	CInventoryItemObject>		("CWeapon")
			// из неэкспортируемого класса CHudItemObject:
			.property("state", &CHudItemObject::GetState)
			.property("next_state", &CHudItemObject::GetNextState)
			// ============================================================================= //
			.def_readwrite("ammo_mag_size"				,			&CWeapon::iMagazineSize)
			.def_readwrite("scope_dynamic_zoom"			,			&CWeapon::m_bScopeDynamicZoom)
			.def_readwrite("zoom_enabled"				,			&CWeapon::m_bZoomEnabled)
			.def_readwrite("zoom_factor"				,			&CWeapon::m_fZoomFactor)
			.def_readwrite("zoom_rotate_time"			,			&CWeapon::m_fZoomRotateTime)
			.def_readwrite("iron_sight_zoom_factor"		,			&CWeapon::m_fIronSightZoomFactor)
			.def_readwrite("scrope_zoom_factor"			,			&CWeapon::m_fScopeZoomFactor)
			.def_readonly("misfire"						,			&CWeapon::bMisfire)
			.def_readonly("zoom_mode"					,			&CWeapon::m_bZoomMode)

			.property("ammo_elapsed"					,			&CWeapon::GetAmmoElapsed, &CWeapon::SetAmmoElapsed)
			.def("get_ammo_current"						,			&CWeapon::GetAmmoCurrent)
			//.def("load_config"						,			&CWeapon::Load)
			.def("start_fire"							,			&CWeapon::FireStart)
			.def("stop_fire"							,			&CWeapon::FireEnd)
			.def("start_fire2"							,			&CWeapon::Fire2Start)			// огонь ножом - правой кнопкой? )
			.def("stop_fire2"							,			&CWeapon::Fire2End)
			.def("stop_shoothing"						,			&CWeapon::StopShooting)

			,
			class_<CWeaponMagazined, CWeapon>			("CWeaponMagazined")
			.def_readonly("shot_num"					,			&CWeaponMagazined::m_iShotNum)
			.def_readwrite("queue_size"					,			&CWeaponMagazined::m_iQueueSize)
			.def_readwrite("shoot_effector_start"		,			&CWeaponMagazined::m_iShootEffectorStart)
			.def_readwrite("cur_fire_mode"				,			&CWeaponMagazined::m_iCurFireMode)			
			,
			class_<CWeaponMagazinedWGrenade,			CWeaponMagazined>("CWeaponMagazinedWGrenade")
			.def_readwrite("gren_mag_size"				,			&CWeaponMagazinedWGrenade::iMagazineSize2)			
			,
			class_<CMissile, CInventoryItemObject>		("CMissile")
			.def_readwrite("destroy_time"				,			&CMissile::m_dwDestroyTime)
			.def_readwrite("destroy_time_max"			,			&CMissile::m_dwDestroyTimeMax)			
			
		];
}
