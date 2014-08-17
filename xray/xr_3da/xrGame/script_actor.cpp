////////////////////////////////////////////////////////////////////////////
//	Module 		: script_actor.cpp
//	Created 	: 12.08.2014
//  Modified 	: 14.08.2014
//	Author		: Alexander Petrov
//	Description : Script Actor (params)
////////////////////////////////////////////////////////////////////////////
#include "pch_script.h"
#include "script_game_object.h"
#include "CharacterPhysicsSupport.h"
#include "script_actor.h"
#include "PHSimpleCharacter.h"
#include "Inventory.h"
#include "Wound.h"

#include "HUDManager.h"
#include "ui/UIDialogWnd.h"
#include "ui/UIInventoryWnd.h"


using namespace luabind;

CPHMovementControl *get_movement(CActor *pActor)
{
	return pActor->character_physics_support()->movement();
}

#pragma optimize("s",on)

void set_health(CActorCondition *C, float h)			   { C->health() = h; }
void set_max_health (CActorCondition *C, float h)		   { C->max_health() = h; }

float get_radiation_v (CActorCondition *C)					{ return CScriptActor::sccv(C).m_fV_Radiation; }
void  set_radiation_v(CActorCondition *C, float v)			{ CScriptActor::sccv(C).m_fV_Radiation = v; }

float get_psy_health_v(CActorCondition *C)					{ return CScriptActor::sccv(C).m_fV_PsyHealth; }
void  set_psy_health_v(CActorCondition *C, float v)			{ CScriptActor::sccv(C).m_fV_PsyHealth = v; }

float get_circumspection_v(CActorCondition *C)				{ return CScriptActor::sccv(C).m_fV_Circumspection; }
void  set_circumspection_v(CActorCondition *C, float v)		{ CScriptActor::sccv(C).m_fV_Circumspection = v; }

float get_morale_v(CActorCondition *C)						{ return CScriptActor::sccv(C).m_fV_EntityMorale; }
void  set_morale_v(CActorCondition *C, float v)			    { CScriptActor::sccv(C).m_fV_EntityMorale = v; }

float get_radiation_health_v(CActorCondition *C)			{ return CScriptActor::sccv(C).m_fV_RadiationHealth; }
void  set_radiation_health_v(CActorCondition *C, float v)	{ CScriptActor::sccv(C).m_fV_RadiationHealth = v; }

float get_bleeding_v(CActorCondition *C)					{ return CScriptActor::sccv(C).m_fV_Bleeding; }
void  set_bleeding_v(CActorCondition *C, float v)			{ CScriptActor::sccv(C).m_fV_Bleeding = v; }

float get_wound_incarnation_v(CActorCondition *C)			{ return CScriptActor::sccv(C).m_fV_WoundIncarnation; }
void  set_wound_incarnation_v(CActorCondition *C, float v)	{ CScriptActor::sccv(C).m_fV_WoundIncarnation = v; }

float get_health_restore_v(CActorCondition *C)				{ return CScriptActor::sccv(C).m_fV_HealthRestore; }
void  set_health_restore_v(CActorCondition *C, float v)		{ CScriptActor::sccv(C).m_fV_HealthRestore = v; }

float get_wound_size (CActorCondition *C, u32 bone, u32 hit_type) 
{ 
	if ( C->wounds().size() <= bone ) return 0;	
	return C->wounds().at(bone)->TypeSize( (ALife::EHitType) hit_type);
}

float get_wound_total_size(CActorCondition *C, u32 bone)
{
	if (C->wounds().size() <= bone) return 0;
	return C->wounds().at(bone)->TotalSize();
}

float get_jump_up_velocity(CPHMovementControl *M)
{
	CPHSimpleCharacter *sp = smart_cast <CPHSimpleCharacter *> (M->PHCharacter());
	if (sp) return sp->get_jump_up_velocity();
	return 0;
}

float	get_jump_speed(CActor *pActor) { return CScriptActor::jump_speed(pActor); }
void	set_jump_speed(CActor *pActor, float speed)
{
	CScriptActor::jump_speed(pActor) = speed;
	get_movement(pActor)->SetJumpUpVelocity(speed);
}

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

CHitImmunity *get_immunities(CActor *pActor) { return pActor->conditions().cast_hit_immunities(); }

void CScriptActor::script_register(lua_State *L)
{
	module(L)
		[
			class_ <CInventory>("CInventory")
			.def_readwrite("max_belt", &CInventory::m_iMaxBelt)
			.def_readwrite("max_weight", &CInventory::m_fMaxWeight)
			.def_readwrite("take_dist", &CInventory::m_fTakeDist)
			.def_readonly("total_weight", &CInventory::m_fTotalWeight)
			.property("active_item", &inventory_active_item)
			.property("selected_item", &inventory_selected_item)
			.property("target", &get_inventory_target)
			,
			class_ <CActorCondition, CEntityCondition>("CActorCondition")
			.property("health", &CActorCondition::GetHealth, &set_health)
			.property("health_max", &CActorCondition::GetMaxHealth, &set_max_health)
			.def_readwrite("alcohol_health", &CActorCondition::m_fAlcohol)
			.def_readwrite("alcohol_v", &CActorCondition::m_fV_Alcohol)
			.def_readwrite("satiety", &CActorCondition::m_fSatiety)
			.def_readwrite("satiety_v", &CActorCondition::m_fV_Satiety)
			.def_readwrite("satiety_health_v", &CActorCondition::m_fV_SatietyHealth)


			.def_readwrite("max_power_leak_speed", &CActorCondition::m_fPowerLeakSpeed)
			.def_readwrite("jump_power", &CActorCondition::m_fJumpPower)
			.def_readwrite("stand_power", &CActorCondition::m_fStandPower)
			.def_readwrite("walk_power", &CActorCondition::m_fWalkPower)
			.def_readwrite("jump_weight_power", &CActorCondition::m_fJumpWeightPower)
			.def_readwrite("walk_weight_power", &CActorCondition::m_fWalkWeightPower)
			.def_readwrite("overweight_walk_k", &CActorCondition::m_fOverweightWalkK)
			.def_readwrite("overweight_jump_k", &CActorCondition::m_fOverweightJumpK)
			.def_readwrite("accel_k", &CActorCondition::m_fAccelK)
			.def_readwrite("sprint_k", &CActorCondition::m_fSprintK)
			.def_readwrite("max_walk_weight", &CActorCondition::m_MaxWalkWeight)

			.def_readwrite("limping_power_begin", &CActorCondition::m_fLimpingPowerBegin)
			.def_readwrite("limping_power_end", &CActorCondition::m_fLimpingPowerEnd)
			.def_readwrite("cant_walk_power_begin", &CActorCondition::m_fCantWalkPowerBegin)
			.def_readwrite("cant_walk_power_end", &CActorCondition::m_fCantWalkPowerEnd)
			.def_readwrite("cant_spint_power_begin", &CActorCondition::m_fCantSprintPowerBegin)
			.def_readwrite("cant_spint_power_end", &CActorCondition::m_fCantSprintPowerEnd)
			.def_readwrite("limping_health_begin", &CActorCondition::m_fLimpingHealthBegin)
			.def_readwrite("limping_health_end", &CActorCondition::m_fLimpingHealthEnd)
			.def_readonly("limping", &CActorCondition::m_bLimping)
			.def_readonly("cant_walk", &CActorCondition::m_bCantWalk)
			.def_readonly("cant_sprint", &CActorCondition::m_bCantSprint)


			.property("radiation_v", &get_radiation_v, &set_radiation_v)
			.property("psy_health_v", &get_psy_health_v, &set_psy_health_v)
			.property("circumspection_v", &get_circumspection_v, &set_circumspection_v)
			.property("morale_v", &get_morale_v, &set_morale_v)
			.property("radiation_health_v", &get_radiation_health_v, &set_radiation_health_v)
			.property("bleeding_v", &get_bleeding_v, &set_bleeding_v)
			.property("wound_incarnation_v", &get_wound_incarnation_v, &set_wound_incarnation_v)
			.property("health_restore_v", &get_health_restore_v, &set_health_restore_v)
			.def("get_wound_size", &get_wound_size)
			.def("get_wound_total_size", &get_wound_total_size)
			,
			class_<CPHMovementControl>("CPHMovementControl")
			.def_readwrite("ph_mass", &CPHMovementControl::fMass)
			.def_readwrite("crash_speed_max", &CPHMovementControl::fMaxCrashSpeed)
			.def_readwrite("crash_speed_min", &CPHMovementControl::fMinCrashSpeed)
			.def_readwrite("collision_damage_factor", &CPHMovementControl::fCollisionDamageFactor)
			.def_readwrite("air_control_param", &CPHMovementControl::fAirControlParam)
			.property("jump_up_velocity", &get_jump_up_velocity, &CPHMovementControl::SetJumpUpVelocity)
			,
			class_<CActor>("CActorBase")
			.property  ("condition",					&CActor::conditions)
			.property  ("immunities",					&get_immunities)
			.def_readwrite("hit_slowmo",				&CActor::hit_slowmo)
			.def_readwrite("hit_probability",			&CActor::hit_probability)	
			.def_readwrite("walk_accel",				&CActor::m_fWalkAccel)
			
			.def_readwrite("run_factor",				&CActor::m_fRunFactor)
			.def_readwrite("run_back_factor",			&CActor::m_fRunBackFactor)
			.def_readwrite("walk_back_factor",			&CActor::m_fWalkBackFactor)
			.def_readwrite("crouch_coef",				&CActor::m_fCrouchFactor)
			.def_readwrite("climb_coef",				&CActor::m_fClimbFactor)
			.def_readwrite("sprint_koef",				&CActor::m_fSprintFactor)
			.def_readwrite("walk_strafe_coef",			&CActor::m_fWalk_StrafeFactor)
			.def_readwrite("run_strafe_coef",			&CActor::m_fRun_StrafeFactor)
			.def_readwrite("disp_base",					&CActor::m_fDispBase)
			.def_readwrite("disp_aim",					&CActor::m_fDispAim)
			.def_readwrite("disp_vel_factor",			&CActor::m_fDispVelFactor)
			.def_readwrite("disp_accel_factor",			&CActor::m_fDispAccelFactor)
			.def_readwrite("disp_crouch_factor",		&CActor::m_fDispCrouchFactor)
			.def_readwrite("disp_crouch_no_acc_factor", &CActor::m_fDispCrouchNoAccelFactor)			

			.def_readonly("inventory",					&CActor::m_inventory)
			.property("movement",						&get_movement)
			.property("jump_speed",						&get_jump_speed, &set_jump_speed)					

			,
			class_<CActorObject, bases<CActor, CEntityAlive>>("CActor")	// хак с наследованием нужен для переопределения свойств. Luabind не поддерживает property getters override
			
			// .property("class_name",						&get_class_name<CActor>)
		];		
}