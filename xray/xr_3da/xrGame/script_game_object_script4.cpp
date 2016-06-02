////////////////////////////////////////////////////////////////////////////
//	Module 		: script_game_object_script4.cpp
//	Created 	: 14.08.2014
//  Modified 	: 19.11.2014
//	Author		: Alexander Petrov
//	Description : Script Actor (params)
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "../lua_tools.h"

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

#include "exported_classes_def.h"
#include "script_actor.h"

#include "script_engine.h"

#include "xrServer_Objects_ALife.h"
#include "ai_object_location.h"


void *lua_to_object(lua_State *L, int index, LPCSTR class_name)
{
	using namespace luabind::detail;
	if (lua_isuserdata(L, index))
	{
		object_rep* rep = is_class_object(L, index);
		if (rep && strstr(rep->crep()->name(), class_name))
			return rep->ptr();
	}
	return NULL;
}


CScriptGameObject *lua_script_game_object(lua_State *L, int index)
{	
	return (CScriptGameObject *) lua_to_object(L, index, "game_object");
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

CWeaponHUD*  CScriptGameObject::GetWeaponHUD() const
{
	CGameObject *obj = &this->object();
	CWeapon *wpn = dynamic_cast<CWeapon*> (obj);
	if (!wpn) return NULL;

	return wpn->GetHUD();
}

IRender_Visual* CScriptGameObject::GetWeaponHUD_Visual() const
{
	CWeaponHUD *hud = GetWeaponHUD();
	if (!hud) return NULL;
	return hud->Visual();
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

void lua_pushgameobject(lua_State *L, CScriptGameObject *obj) // overloaded 1
{
	using namespace luabind::detail;
	convert_to_lua<CScriptGameObject*>(L, obj);
}

void lua_pushgameobject(lua_State *L, CGameObject *obj) // overloaded 2
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
			test_pushobject<CInventoryContainer>		(L, obj) ||
			test_pushobject<CGrenade>					(L, obj) ||
			test_pushobject<CMissile>					(L, obj) ||
			test_pushobject<CCustomOutfit>				(L, obj) ||
			test_pushobject<CWeaponAmmo>				(L, obj) ||
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
		 test_pushobject<CSpaceRestrictor>			(L, obj) ||
		 test_pushobject<CCustomZone>				(L, obj) ||
		 test_pushobject<IInventoryBox>				(L, obj) ||
		 test_pushobject<CEntityAlive>				(L, obj) ||
		 test_pushobject<CEntity>					(L, obj)	 
	   ) return;

	convert_to_lua<CGameObject*> (L, obj); // for default 
}

CGameObject *lua_togameobject(lua_State *L, int index)
{
	using namespace luabind::detail;
	if (lua_isnumber(L, index))
	{
		u32 id = lua_tointeger(L, index);
		if (id < 0xFFFF)
			return client_obj(id);
	}
	
	CScriptGameObject *script_obj = lua_script_game_object(L, index);
	CGameObject *obj = NULL;
	if (script_obj)		
		obj = &script_obj->object();
	return obj;
}

bool test_in_stack(u32 *pstack, u32 pvalue)
{
	for (int i = 0; i < 16; i ++)
	if (pstack[i] == pvalue)
		return true;

	return false;
}

lua_State* active_vm(CGameObject *obj = NULL) // deprecated
{
	lua_State *L = NULL;

	if (obj)
		L = obj->lua_game_object()->lua_state();
		
    if (!L) L =	ai().script_engine().lua();	 

#ifdef LUAICP_COMPAT

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

LPCSTR script_object_class_name(lua_State *L) // для raw-функции. Так-же см. get_lua_class_name для raw-свойства.
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

void raw_get_inv_item(CScriptGameObject *script_obj, lua_State *L)
{
	script_obj->set_lua_state(L);  
	CGameObject *obj = &script_obj->object();
	if (smart_cast<CInventoryItem*>(obj))
		lua_pushgameobject(L, obj);
	else
		lua_pushnil(L);
}


#pragma message(" get_interface raw function")
using namespace luabind;

void get_interface(object O)
{	
	lua_State *L = O.lua_state();
	dynamic_engine_object(L);	
}

void fake_set_interface (CScriptGameObject *script_obj, object value) { }

u32 get_level_id(u32 gvid)
{
	CGameGraph &gg = ai().game_graph(); 
	if (gg.valid_vertex_id(gvid))	
		return gg.vertex(gvid)->level_id();		
	return (u32)-1; // ERROR signal
}

LPCSTR get_level_name_by_id (u32 level_id)
{
	if (level_id < 0xff)
		return ai().game_graph().header().level((GameGraph::_LEVEL_ID) level_id).name().c_str();
	else
		return "l255_invalid_level";
}

bool	get_obj_alive(CScriptGameObject *O)
{
	CGameObject *obj = &O->object();
	CEntityAlive *ent = smart_cast<CEntityAlive*> (obj);
	if (ent)
		return !!ent->g_Alive();
	else
		return false;
}

u32 obj_level_id(CScriptGameObject *O)
{
	return get_level_id (O->object().ai_location().game_vertex_id());
}

LPCSTR obj_level_name(CScriptGameObject *O) { return get_level_name_by_id ( obj_level_id(O) ); }

#ifndef LUABIND_NO_ERROR_CHECKING3
extern IWriter *OpenMemoryDumper();
extern void CloseDumper(IWriter* &dumper);
extern void print_class(lua_State *L, luabind::detail::class_rep *crep);
#endif


DLL_API bool GetUserdataInfo(lua_State *L, int index, LPSTR buffer, size_t buff_size)
{
	using namespace luabind::detail;
	// strcpy_s(buffer, buff_size, "no_info");
	if (!lua_isuserdata(L, index)) return false; 
	void *obj_ptr = lua_touserdata(L, index);
	__try
	{		
		object_rep* rep = is_class_object(L, index);
		ZeroMemory(buffer, buff_size);

		if (rep)
		{
			LPCSTR type = rep->crep()->name();
			if (!stricmp(type, "game_object"))
			{
				CScriptGameObject *O = lua_script_game_object(L, index);
				CGameObject &obj = O->object();
				shared_str *tmp = xr_new<shared_str>();

				tmp->sprintf("game_object #%5d %-25s %s", obj.ID(), obj.Name_script(), obj.CppClassName()).c_str();
				if (tmp->size() >= buff_size)
					strncpy_s(buffer, buff_size, **tmp, _TRUNCATE);
				else
					strcpy_s(buffer, buff_size, **tmp);
				buffer[tmp->size()] = 0;

				xr_delete(tmp);
			}
			else
			{
				strcpy_s(buffer, buff_size, type);
				MsgCB("$#CONTEXT: GetUserDataInfo for object class '%s' ", buffer);
#ifndef LUABIND_NO_ERROR_CHECKING3
				IWriter *dump = NULL;
				if (buff_size > 128)
					__try
				{
					dump = OpenMemoryDumper();
					print_class(L, rep->crep());
					dump->flush();
					if (dump->tell() > 0)
					{
						CMemoryWriter *mw = smart_cast<CMemoryWriter *>(dump);
						size_t _size = __min(buff_size - 1, mw->size());
						strncpy_s(buffer, buff_size, (LPCSTR)mw->pointer(), _size);
						buffer[_size] = 0;
					}
				}
				__except (SIMPLE_FILTER)
				{
					Msg("!#EXCEPTION: catched while print_class %s", buffer);
				}
				if (dump) CloseDumper(dump);
#endif		
			}
			return true;
		}

	}
	__except (SIMPLE_FILTER)
	{
		sprintf_s(buffer, buff_size, "#EXCEPTION catched for obj = 0x%p ", obj_ptr);
	}
	return false;
}

int get_object_info(lua_State *L)
{
	static string4096 buffer;

	if (lua_isuserdata(L, 1))
		GetUserdataInfo(L, 1, buffer, 4096);
	else
		sprintf_s(buffer, 4096, "type = %s ", lua_typename(L, 1));
	lua_pushstring(L, buffer);
	return 1;
}


#pragma optimize("s",on)

class_<CScriptGameObject> &script_register_game_object3(class_<CScriptGameObject> &instance)
{
	instance
		#pragma message("+ game_object.extensions export begin")
		// alpet: export object cast		 
		.def("get_game_object",				&CScriptGameObject::object)
		.def("get_alife_object",			&CScriptGameObject::alife_object)
		.def("get_actor",					&script_game_object_cast<CActorObject>,			raw(_2))
		.def("get_ammo",					&script_game_object_cast<CWeaponAmmo>,			raw(_2))
		.def("get_anomaly",					&script_game_object_cast<CCustomZone>,			raw(_2))
		.def("get_artefact",				&script_game_object_cast<CArtefact>,			raw(_2))		
		.def("get_base_monster",			&script_game_object_cast<CBaseMonster>,			raw(_2))
		.def("get_container",				&script_game_object_cast<CInventoryContainer>,	raw(_2))
		.def("get_eatable_item",			&script_game_object_cast<CEatableItemObject>,	raw(_2))
		.def("get_grenade",					&script_game_object_cast<CGrenade>,				raw(_2))
		.def("get_inventory_box",			&script_game_object_cast<IInventoryBox>,		raw(_2))
		.def("get_inventory_item",			&raw_get_inv_item, raw(_2))
		.def("get_inventory_owner",			&script_game_object_cast<CInventoryOwner>,		raw(_2))
		.def("get_interface",				&raw_get_interface,								raw(_2))
		.def("get_missile",					&script_game_object_cast<CMissile>,				raw(_2))
		.def("get_outfit",					&script_game_object_cast<CCustomOutfit>,		raw(_2))
		.def("get_space_restrictor",		&script_game_object_cast<CSpaceRestrictor>,		raw(_2))
		.def("get_torch",					&get_torch)
		.def("get_weapon",					&script_game_object_cast<CWeapon>,				raw(_2))
		.def("get_weapon_m",				&script_game_object_cast<CWeaponMagazined>,		raw(_2))
		.def("get_weapon_mwg",				&script_game_object_cast<CWeaponMagazinedWGrenade>, raw(_2))
		.def("get_weapon_hud",				&CScriptGameObject::GetWeaponHUD)
		.def("get_hud_visual",				&CScriptGameObject::GetWeaponHUD_Visual)
		.def("load_hud_visual",				&CScriptGameObject::LoadWeaponHUD_Visual)
		.property("interface",				&get_interface,  &fake_set_interface, raw(_2))	
		.property("inventory",				&get_obj_inventory)
		.property("immunities",				&get_obj_immunities)
		.property("is_alive",				&get_obj_alive)
		.property("conditions",				&get_obj_conditions)		
		.property("level_id",				&obj_level_id)
		.property("level_name",				&obj_level_name)
		,

		def("script_object_class_name",		&script_object_class_name, raw(_1)),
		def("engine_object",				&dynamic_engine_object, raw(_1)),		
		def("get_actor_obj",				&Actor),
		def("get_level_id",					&get_level_id),
		def("get_object_info",				&get_object_info, raw(_1))

		#pragma message("+ game_object.extensions export end")
	; return instance;
}
