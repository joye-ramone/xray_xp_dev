////////////////////////////////////////////////////////////////////////////
//	Module 		: script_binder_object_wrapper.cpp
//	Created 	: 29.03.2004
//  Modified 	: 29.03.2004
//	Author		: Dmitriy Iassenev
//	Description : Script object binder wrapper
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_binder_object_wrapper.h"
#include "script_game_object.h"
#include "xrServer_Objects_ALife.h"
#include "../../xrNetServer/net_utils.h"
#include "../../build_config_defines.h"
#include "GameObject.h"
#include "GamePersistent.h"

// #define PERF_CHECK


#pragma optimize("gyts", on)
#ifdef PERF_CHECK
#pragma message("alpet: дефайн PERF_CHECK не рекомендуется для trunk/release")
#endif 

typedef LPCSTR(WINAPI *DUMP_VAR_PROC) (lua_State *L, int index);
DUMP_VAR_PROC dump_var = NULL;

int g_spawn_calls = 0;

IC void remove_flood(lua_State *L, LPCSTR context)
{		
#ifdef LUA_STACK_LEAK_FIND
	if (NULL == L) return;
	static int previous = 0;

	int top = lua_gettop(L);
	if (top > previous && strstr(context, "after"))
		Msg("! #WARN: lua stack leak detected, context = '%s' ", context);
	previous = top;
	if (top < 100) return;

	int pcnt = 0;
	for (int i = top; i > 0; i--)
	if (lua_isnil(L, i))
		{	
			lua_remove(L, i);
			pcnt++;
		}
	if (pcnt > 100)
		Msg("! #WARN: removed %d nil flood entries from lua stack at '%s' ", pcnt, context);
#endif
}

CScriptBinderObjectWrapper::CScriptBinderObjectWrapper	(CScriptGameObject *object) :
	CScriptBinderObject	(object)
{
	if (object->object().cast_actor())
		Msg(" created CScriptBinderObjectWrapper %p for single_player ", this);

#ifdef LUAICP_COMPAT
	if (!dump_var)
	{
		HMODULE hDLL = GetModuleHandle("luaicp.dll");
	    dump_var = (DUMP_VAR_PROC)GetProcAddress(hDLL, "ExpDumpVar");
	}
#endif
}

CScriptBinderObjectWrapper::~CScriptBinderObjectWrapper ()
{
}

void CScriptBinderObjectWrapper::reinit					()
{
	remove_flood(m_object->lua_state(), "before CScriptObject::reinit");
	luabind::call_member<void>		(this,"reinit");
	remove_flood(m_object->lua_state(), "before CScriptObject::reinit");
}

void CScriptBinderObjectWrapper::reinit_static			(CScriptBinderObject *script_binder_object)
{
	script_binder_object->CScriptBinderObject::reinit	();
}

void CScriptBinderObjectWrapper::reload					(LPCSTR section)
{
	remove_flood(m_object->lua_state(), "before CScriptObject::reload");
	luabind::call_member<void>		(this,"reload",section);
	remove_flood(m_object->lua_state(), "before CScriptObject::reload");
}

void CScriptBinderObjectWrapper::reload_static			(CScriptBinderObject *script_binder_object, LPCSTR section)
{
	script_binder_object->CScriptBinderObject::reload	(section);
}

bool CScriptBinderObjectWrapper::net_Spawn(SpawnType DC)
{
	bool result;
#ifdef PERF_CHECK
	SetThreadAffinityMask(GetCurrentThread(), 0x0002);
#endif	
	m_timer.Start();
	Device.Statistic->ScriptBinder.Begin();
	Device.Statistic->ScriptBinder_netSpawn.Begin();
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Spawn");
	__try
	{
		result = (luabind::call_member<bool>(this, "net_spawn", DC));
	}
	__except (SIMPLE_FILTER)
	{
		Msg("! #EXCEPTION: in CScriptBinderObjectWrapper::net_Spawn for %s ", m_object->Name());
	}
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Spawn");
	Device.Statistic->ScriptBinder_netSpawn.End();
	Device.Statistic->ScriptBinder.End();				
#ifdef PERF_CHECK
	SetThreadAffinityMask(GetCurrentThread(), 0xFFFE);
#endif	
	float elps = m_timer.GetElapsed_sec() * 1000.f;
	if (m_object && elps >= 20.f)
	{
		const Fvector p = m_object->Position();
		MsgCB("##PERF_WARN: CScriptBinderObjectWrapper::net_Spawn for object %25s, work_time = %.1f ms, position = { %.1f, %.1f, %.1f } ", 
					m_object->Name(), elps, p.x, p.y, p.z);
	}
	return result;
}

bool CScriptBinderObjectWrapper::net_Spawn_static		(CScriptBinderObject *script_binder_object, SpawnType DC)
{
	return							(script_binder_object->CScriptBinderObject::net_Spawn(DC));
}

void CScriptBinderObjectWrapper::net_Destroy			()
{
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Destroy");
	__try
	{
		luabind::call_member<void>(this, "net_destroy");
	}
	__except (SIMPLE_FILTER)
	{
		Msg("! #EXCEPTION: catched in CScriptBinderObjectWrapper::net_Destroy for object %s ", m_object->Name());
	}

	remove_flood(m_object->lua_state(), "after CScriptObject::net_Destroy");
}

void CScriptBinderObjectWrapper::net_Destroy_static		(CScriptBinderObject *script_binder_object)
{
	script_binder_object->CScriptBinderObject::net_Destroy();
}

void CScriptBinderObjectWrapper::net_Import				(NET_Packet *net_packet)
{
	m_timer.Start();
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Import");
	luabind::call_member<void>		(this,"net_import",net_packet);
	remove_flood(m_object->lua_state(), "after CScriptObject::net_Import");
	float elps = m_timer.GetElapsed_sec() * 1000.f;
	if (m_object && elps >= 5.f)
		MsgCB("##PERF: net_Import processed for %25s, work_time = %.1f ms ", m_object->Name(), elps);

}

void CScriptBinderObjectWrapper::net_Import_static		(CScriptBinderObject *script_binder_object, NET_Packet *net_packet)
{
	script_binder_object->CScriptBinderObject::net_Import	(net_packet);
}

void CScriptBinderObjectWrapper::net_Export				(NET_Packet *net_packet)
{
	m_timer.Start();
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Export");
	luabind::call_member<void>		(this,"net_export",net_packet);
	remove_flood(m_object->lua_state(), "after CScriptObject::net_Export");
	float elps = m_timer.GetElapsed_sec() * 1000.f;
	if (m_object && elps >= 5.f)
		MsgCB("##PERF: net_Export processed for %25s, work_time = %.1f ms ", m_object->Name(), elps);

}

void CScriptBinderObjectWrapper::net_Export_static		(CScriptBinderObject *script_binder_object, NET_Packet *net_packet)
{
	script_binder_object->CScriptBinderObject::net_Export	(net_packet);
}

void CScriptBinderObjectWrapper::shedule_Update			(u32 time_delta)
{	
	CGameObject *obj = &this->m_object->object();
#ifdef PERF_CHECK	
	if (psDeviceFlags.test(rsStatistic))
	{
		DWORD_PTR need = (obj && obj->cast_actor()) ? 8 : 4;
		DWORD_PTR mask = 0;
		do {
			mask = SetThreadAffinityMask(GetCurrentThread(), need);
			SwitchToThread();
		} while (mask != need);		  
		if (0 == mask)
			__asm nop;
	}
#endif	
	m_timer.Start();

	lua_State *L = m_object->lua_state();
	int before_top = 0;
	if (L)
	{		
		remove_flood(m_object->lua_state(), "before CScriptObject::update");

		before_top = lua_gettop(L);

		lua_pushinteger(L, before_top);
		lua_setglobal(L, "g_lua_stack_top");
		if (before_top >= 5)
		{
			if (!lua_checkstack(L, 50))
				Msg("! #WARN: lua_checkstack(L, 50) returned FALSE, stack top = %d, object binder for %s ", before_top, obj->Name_script());
#ifdef LUAICP_COMPAT
			Msg("!#WARN: lua stack flood dump, top = %d: ", before_top);
			// string128 buff;
			if (dump_var)
			for (int i = 1; i <= before_top; i++)
			 if (!lua_isnil(L, i))
			 {
				Msg("\t [%4d] %s ", i, dump_var(L, i));
			 } 

#endif
			lua_settop(L, 0);
			before_top = 0;
		}
	}
	__try
	{
		Device.Statistic->ScriptBinder.Begin();
		luabind::call_member<void>(this, "update", time_delta);
		Device.Statistic->ScriptBinder.End();
	}
	__except (SIMPLE_FILTER)
	{
		MsgCB("! #EXCEPTION: CScriptBinderObjectWrapper::shedule_Update for object %s ", obj->Name_script());
	}
	if (L)
	{
		remove_flood(m_object->lua_state(), "after CScriptObject::update");
		int after_top = lua_gettop(L);
		if (after_top > before_top)
		{
			Msg("!#LEAK: after binder::update for %s lua stack top was changed from %d to %d (returned result?)", obj->Name_script(), before_top, after_top);			
			for (int i = before_top + 1; i <= after_top; i++)
			 if (!lua_isnil(L, i) && dump_var)			 
				 Msg("\t [%4d] %s ", i, dump_var(L, i));
		}

		lua_settop(L, 0); // для отладки всякого мусора
	}

	u32 ms = m_timer.GetElapsed_ms();
	if (ms > 50)
		MsgCB("!#PERF: binder::update for object %s eats time = %d ms ", obj->Name_script(), ms);

#ifdef PERF_CHECK
	if (psDeviceFlags.test(rsStatistic))
	{
		SetThreadAffinityMask(GetCurrentThread(), 0x0002);
		SwitchToThread();
	}
#endif
}

void CScriptBinderObjectWrapper::shedule_Update_static	(CScriptBinderObject *script_binder_object, u32 time_delta)
{
	script_binder_object->CScriptBinderObject::shedule_Update	(time_delta);
}

void CScriptBinderObjectWrapper::save					(NET_Packet *output_packet)
{
	if (m_object->object().cast_actor())
		Msg("# executing save callback for %s", m_object->object().Name_script());

	remove_flood(m_object->lua_state(), "before CScriptObject::save");
	luabind::call_member<void>		(this, "save", output_packet);
	remove_flood(m_object->lua_state(), "after CScriptObject::save");
}

void CScriptBinderObjectWrapper::save_static			(CScriptBinderObject *script_binder_object, NET_Packet *output_packet)
{
	script_binder_object->CScriptBinderObject::save		(output_packet);
}

void CScriptBinderObjectWrapper::load					(IReader *input_packet)
{
	m_timer.Start();
	remove_flood(m_object->lua_state(), "before CScriptObject::load");
	luabind::call_member<void>		(this,"load",input_packet);
	remove_flood(m_object->lua_state(), "after CScriptObject::load");
	float elps = m_timer.GetElapsed_sec() * 1000.f;
	if (m_object && elps >= 5.f)
		MsgCB("##PERF: load      processed for %25s, work_time = %.1f ms ", m_object->Name(), elps);

}

void CScriptBinderObjectWrapper::load_static			(CScriptBinderObject *script_binder_object, IReader *input_packet)
{
	script_binder_object->CScriptBinderObject::load		(input_packet);
}

bool CScriptBinderObjectWrapper::net_SaveRelevant		()
{
	return							(luabind::call_member<bool>(this,"net_save_relevant"));
}

bool CScriptBinderObjectWrapper::net_SaveRelevant_static(CScriptBinderObject *script_binder_object)
{
	return							(script_binder_object->CScriptBinderObject::net_SaveRelevant());
}

void CScriptBinderObjectWrapper::net_Relcase			(CScriptGameObject *object)
{
	m_timer.Start();
	remove_flood(m_object->lua_state(), "before CScriptObject::net_Relcase");
	luabind::call_member<void>		(this,"net_Relcase",object);
	remove_flood(m_object->lua_state(), "after CScriptObject::net_Relcase");
	float elps = m_timer.GetElapsed_sec() * 1000.f;
	if (m_object && elps >= 5.f)
		MsgCB("##PERF: net_Relcase processed for %25s, work_time = %.1f ms ", m_object->Name(), elps);

}

void CScriptBinderObjectWrapper::net_Relcase_static		(CScriptBinderObject *script_binder_object, CScriptGameObject *object)
{
	script_binder_object->CScriptBinderObject::net_Relcase	(object);
}