////////////////////////////////////////////////////////////////////////////
//	Module 		: script_game_object_script4.cpp
//	Created 	: 14.08.2014
//  Modified 	: 15.08.2014
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
#include "Artifact.h"
#include "Car.h"
#include "Entity.h"
#include "eatable_item_object.h"
#include "Grenade.h"
#include "HangingLamp.h"
#include "helicopter.h"
#include "Inventory.h"
#include "inventory_item_object.h"
#include "InventoryOwner.h"
#include "Torch.h"
#include "script_actor.h"
#include "Weapon.h"
#include "WeaponMagazined.h"
#include "WeaponMagazinedWGrenade.h"

#include "script_engine.h"

#include "xrServer_Objects_ALife.h"


template <typename T>
T* script_game_object_cast (CScriptGameObject *script_obj)
{
	CGameObject *obj = &script_obj->object();
	return smart_cast<T *>(obj);
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

	CGameObject *obj = &script_obj->object();	
	CArtefact *pArt = smart_cast<CArtefact*> (obj);
	if (pArt)
		return &pArt->m_ArtefactHitImmunities;
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

	obj->lua_game_object()->set_lua_state(L);

	// базовые классы в иерархии должны быть добавлены последними
	if (smart_cast<CInventoryItem*>(obj))
	{
		if (// наследнички CInventoryItem 
			test_pushobject<CTorch>						(L, obj) ||						
			test_pushobject<CArtefact>					(L, obj) ||			
			test_pushobject<CEatableItemObject>			(L, obj) ||
			test_pushobject<CGrenade>					(L, obj) ||
			test_pushobject<CWeaponMagazinedWGrenade>	(L, obj) ||
			test_pushobject<CWeaponMagazined>			(L, obj) ||
			test_pushobject<CWeapon>					(L, obj) ||
			test_pushobject<CInventoryItemObject>		(L, obj) ||
			test_pushobject<CInventoryItem>				(L, obj)
			) return;
	}

	if (smart_cast<CActor*>(obj))
	{
		luabind::detail::convert_to_lua<CActorObject*>(L, (CActorObject*)obj);
		return;
	}


	if ( test_pushobject<CCar>						(L, obj) ||		 
		 test_pushobject<CHangingLamp>				(L, obj) || 
	 	 test_pushobject<CHelicopter>				(L, obj) ||		 		 
		 test_pushobject<CEntityAlive>				(L, obj) ||
		 test_pushobject<CEntity>					(L, obj)	 
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

bool test_in_stack(u32 *pstack, u32 pvalue)
{
	for (int i = 0; i < 16; i ++)
	if (pstack[i] == pvalue)
		return true;

	return false;
}

lua_State* active_vm(CGameObject *obj = NULL)
{
	lua_State *L = NULL;

	if (obj)
		L = obj->lua_game_object()->lua_state();
		
    if (!L) L =	ai().script_engine().lua();	 

#ifdef LUAICP_COMPAT


	#pragma todo("alpet: свойство interface может не возвращаться, в колбеках движка при использовании LuaSafeCall ")

	LPCSTR member = NULL;
	for (int i = lua_gettop(L); i > 0; i--)
	{		
		if (lua_type(L, i) == LUA_TSTRING)
			member = lua_tostring(L, i); // for verify 'interface'

		CGameObject *ref = lua_togameobject(L, i);
		if (member && obj && ref == obj && strstr(member, "interface"))
			return L;   // иногда работает при использовании iterate_inventory из вызова LuaSafeCall
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "active_vm");	
	if (lua_islightuserdata(L, -1))
	{
		lua_State *Lsrc = L;
		L = (lua_State*)lua_topointer(L, -1);
		lua_pop(Lsrc, 1);
	}
	else
	{
		Msg("!WARN: active_vm not set in LUA_REGISTRY. type(L,-1) = %d", lua_type(L, -1));
		lua_pop(L, 1);
	}
		
#endif
	return L;
}

LPCSTR script_object_class_name(lua_State *L)
{
	using namespace luabind::detail;

	static string64 class_name;
	sprintf_s (class_name, 63, "lua_type = %d", lua_type(L, 1));

	if (lua_isuserdata(L, 1))
	{
		object_rep* rep = is_class_object(L, 1);
		if (rep)
			strcpy_s(class_name, 63, rep->crep()->name());
	}
	
	return class_name;
}


// alpet: получение произвольного объекта движка по ID  или game_object
void dynamic_engine_object(lua_State *L)
{	
	using namespace luabind::detail;

	CGameObject *obj = lua_togameobject(L, 1);

	if (obj)	
		lua_pushgameobject(L, obj);			
	else
		lua_pushnil (L);
}

void raw_get_interface(CScriptGameObject *script_obj, lua_State *L)
{	
	script_obj->set_lua_state(L);  // for future use
	CGameObject *obj = &script_obj->object();
	lua_pushgameobject(L, obj);
	static int type = lua_type(L, -1);
	static int top = lua_gettop(L);
	VERIFY(type == LUA_TUSERDATA && top > 0);
}

void get_interface(CScriptGameObject *script_obj)
{	
	lua_State *L = active_vm(&script_obj->object());
	raw_get_interface (script_obj, L);
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
		.def("get_actor",					&script_game_object_cast<CActorObject>)
		.def("get_artefact",				&script_game_object_cast<CArtefact>)
		.def("get_eatable_item",			&script_game_object_cast<CEatableItemObject>)
		.def("get_grenade",					&script_game_object_cast<CGrenade>)
		.def("get_inventory_item",			&script_game_object_cast<CInventoryItemObject>)
		.def("get_interface",				&raw_get_interface, raw(_2))    // более надежное и быстрое решение, при использовании LuaSafeCall
		.def("get_torch",					&get_torch)
		.def("get_weapon",					&script_game_object_cast<CWeapon>)
		.def("get_weapon_m",				&script_game_object_cast<CWeaponMagazined>)
		.def("get_weapon_mwg",				&script_game_object_cast<CWeaponMagazinedWGrenade>)

		.def("get_hud_visual",				&CScriptGameObject::GetWeaponHUD_Visual)
		.def("load_hud_visual",				&CScriptGameObject::LoadWeaponHUD_Visual)
		.property("interface",				&get_interface,  &set_interface, raw(_2))	
		.property("inventory",				&get_obj_inventory)
		.property("immunities",				&get_obj_immunities)
		.property("conditions",				&get_obj_conditions)		
		
		,

		def("script_object_class_name",		&script_object_class_name, raw(_1)),
		def("engine_object",				&dynamic_engine_object, raw(_1)),
		def("get_actor_obj",				&Actor)
	; return instance;
}