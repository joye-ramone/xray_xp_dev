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

#include "Actor.h"
#include "Artifact.h"
#include "eatable_item.h"
#include "eatable_item_object.h"
#include "Entity.h"
#include "entity_alive.h"
#include "Explosive.h"
#include "ExplosiveItem.h"
#include "Grenade.h"
#include "HudItem.h"
#include "hud_item_object.h"
#include "inventory_item.h"
#include "inventory_item_object.h"
#include "Missile.h"
#include "Weapon.h"
#include "WeaponMagazined.h"
#include "WeaponMagazinedWGrenade.h"

using namespace luabind;
#pragma optimize("s", on)


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

void CInventoryItemScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CInventoryItem>("CInventoryItem")
			.def_readonly("item_place",					&CInventoryItem::m_eItemPlace)
			.property("class_name",						&get_class_name<CInventoryItem>),
			class_<CInventoryItemObject, bases<CInventoryItem, CGameObject>>("CInventoryItemObject")
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
