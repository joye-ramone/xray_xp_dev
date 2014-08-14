////////////////////////////////////////////////////////////////////////////
//	Module 		: script_game_object_script4.cpp
//	Created 	: 14.08.2014
//  Modified 	: 14.08.2014
//	Author		: Alexander Petrov
//	Description : Script Actor (params)
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"

#include "pda_space.h"
#include "memory_space.h"
#include "movement_manager_space.h"


#include "cover_point.h"

#include "script_binder_object.h"
#include "script_entity_action.h"
#include "script_game_object.h"
#include "script_hit.h"
#include "script_ini_file.h"
#include "script_monster_hit_info.h"
#include "script_sound_info.h"


#include "action_planner.h"
#include "PhysicsShell.h"

#include "script_zone.h"
#include "relation_registry.h"
#include "danger_object.h"

#include "alife_space.h"

#include "hit_immunity.h"
#include "ActorCondition.h"
#include "EntityCondition.h"
#include "holder_custom.h"

#include "ai_space_inline.h"

#include "Actor.h"
#include "Car.h"
#include "Entity.h"
#include "HangingLamp.h"
#include "helicopter.h"
#include "Inventory.h"
#include "InventoryOwner.h"
#include "Torch.h"
#include "Weapon.h"



#include "script_engine.h"

#include "xrServer_Objects_ALife.h"


CActor *get_actor(CScriptGameObject *script_obj)
{
	CGameObject *obj = &script_obj->object();
	return smart_cast<CActor*>(obj);
}

CEntityCondition *get_obj_conditions(CScriptGameObject *script_obj)
{
	CGameObject *obj = &script_obj->object();
	CActor *pA = smart_cast<CActor*> (obj);
	if (pA)
		return &pA->conditions();

	CEntity *pE = smart_cast<CEntity*> (obj);
	if (pE)
		return pE->conditions();

	return NULL;
}

CHitImmunity *get_obj_immunities(CScriptGameObject *script_obj)
{
	CEntityCondition *cond = get_obj_conditions(script_obj);
	if (cond)
		return smart_cast<CHitImmunity*> (cond);

	return NULL;
}

CInventory *get_obj_inventory(CScriptGameObject *script_obj)
{
	CInventoryOwner *owner = smart_cast<CInventoryOwner *>(&script_obj->object());
	if (owner) return owner->m_inventory;
	CHolderCustom* holder = script_obj->get_current_holder();
	if (holder) return holder->GetInventory();
	return NULL;
}


// alpet: получение визуала для худа оружия

CSE_ALifeDynamicObject* CScriptGameObject::alife_object() const
{
	return object().alife_object();
}

IRender_Visual* CScriptGameObject::GetWeaponHUD_Visual() const
{
	CGameObject *obj = &this->object();
	CWeapon *wpn = dynamic_cast<CWeapon*> (obj);
	if (!wpn) return NULL;

	return wpn->GetHUD()->Visual();
}

void CScriptGameObject::LoadWeaponHUD_Visual(LPCSTR wpn_hud_section)
{
	CGameObject *obj = &this->object();
	CWeapon *wpn = dynamic_cast<CWeapon*> (obj);
	if (!wpn) return;

	wpn->GetHUD()->Load(wpn_hud_section);
}

CGameObject *client_obj(u32 id)
{
	CObject *obj = Level().Objects.net_Find(id);
	return smart_cast<CGameObject*>(obj);
}



template <typename T>
IC bool test_pushobject(lua_State *L, CGameObject * obj)
{	
	using namespace luabind::detail;
	T *pObj = smart_cast<T*> (obj);
	if (pObj && get_class_rep<T>(L))
	{		
		convert_to_lua<T*>(L, pObj);  // обязательно конвертировать указатель, а не значение. Иначе вызов деструктора при сборке мусора!
		return true;		
	}
	return false;
}


void lua_pushgameobject(lua_State *L, CGameObject *obj)
{
	using namespace luabind::detail;

	if ( test_pushobject<CActor>		(L, obj) || // script_actor.cpp
		 test_pushobject<CCar>			(L, obj) ||
		 test_pushobject<CHangingLamp>  (L, obj) ||
	 	 test_pushobject<CHelicopter>   (L, obj) ||
		 test_pushobject<CTorch>		(L, obj)
	   ) return;

	convert_to_lua<CGameObject*> (L, obj); // for default 
}

CGameObject *lua_togameobject(lua_State *L, int index)
{
	using namespace luabind::detail;
	CScriptGameObject *script_obj = NULL;
	CGameObject *obj = NULL;

	if (lua_isuserdata(L, index))
	{
		object_rep* rep = is_class_object(L, index);
		if (rep && strstr(rep->crep()->name(), "game_object"))
		{
			script_obj = (CScriptGameObject *)rep->ptr();
			obj = &script_obj->object();
		}

	}
	if (lua_isnumber(L, index))
	{
		u32 id = lua_tointeger(L, index);
		if (id < 0xFFFF)
			obj = client_obj(id);
	}

	return obj;
}


// alpet: получение произвольного объекта движка по ID  или game_object
void dynamic_engine_object(lua_State *L)
{	
	using namespace luabind::detail;
#ifdef LUAICP_COMPAT
	xr_string s = stack_content_by_name(L, 1); // luabind::detail::
	MsgCB("#DEBUG: Params: %s", s.c_str());
#endif

	CGameObject *obj = lua_togameobject(L, 1);

	if (obj)
		lua_pushgameobject(L, obj);
	else
		lua_pushnil (L);
}

void get_interface(CScriptGameObject *script_obj)
{
	lua_State *L = ai().script_engine().lua();
	lua_pushgameobject(L, &script_obj->object());	
}

void set_interface (CScriptGameObject *script_obj, void *param) { }

using namespace luabind;

#pragma optimize("s",on)

class_<CScriptGameObject> &script_register_game_object3(class_<CScriptGameObject> &instance)
{
	instance
		// alpet: export object cast		 
		.def("get_game_object",				&CScriptGameObject::object)
		.def("get_alife_object",			&CScriptGameObject::alife_object)
		.def("get_actor",					&get_actor)
		.def("get_torch",					&get_torch)
		.def("get_hud_visual",				&CScriptGameObject::GetWeaponHUD_Visual)
		.def("load_hud_visual",				&CScriptGameObject::LoadWeaponHUD_Visual)
		.property("interface",				&get_interface,  &set_interface, raw(_2))	
		.property("inventory",				&get_obj_inventory)
		.property("immunities",				&get_obj_immunities)
		.property("conditions",				&get_obj_conditions)		
		,

		def("engine_object",				&dynamic_engine_object, raw(_1)),
		def("get_actor_obj",				&Actor)
	; return instance;
}