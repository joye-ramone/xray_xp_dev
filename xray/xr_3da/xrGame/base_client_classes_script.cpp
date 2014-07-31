////////////////////////////////////////////////////////////////////////////
//	Module 		: base_client_classes_script.cpp
//	Created 	: 20.12.2004
//  Modified 	: 20.12.2004
//	Author		: Dmitriy Iassenev
//	Description : XRay base client classes script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "base_client_classes.h"
#include "base_client_classes_wrappers.h"
#include "../feel_sound.h"
#include "../fbasicvisual.h"
#include "../skeletonanimated.h"
#include "ai/stalker/ai_stalker.h"
#include "../../xrNetServer/net_utils.h"
#include "../ResourceManager.h"
#include "../device.h"
#include "../Render.h"
#include "script_game_object.h"
#include "Actor.h"
#include "../CameraBase.h"


using namespace luabind;

#pragma optimize("s",on)
void DLL_PureScript::script_register	(lua_State *L)
{
	module(L)
	[
		class_<DLL_Pure,CDLL_PureWrapper>("DLL_Pure")
			.def(constructor<>())
			.def("_construct",&DLL_Pure::_construct,&CDLL_PureWrapper::_construct_static)
	];
}

/*
void ISpatialScript::script_register	(lua_State *L)
{
	module(L)
	[
		class_<ISpatial,CISpatialWrapper>("ISpatial")
			.def(constructor<>())
			.def("spatial_register",	&ISpatial::spatial_register,	&CISpatialWrapper::spatial_register_static)
			.def("spatial_unregister",	&ISpatial::spatial_unregister,	&CISpatialWrapper::spatial_unregister_static)
			.def("spatial_move",		&ISpatial::spatial_move,		&CISpatialWrapper::spatial_move_static)
			.def("spatial_sector_point",&ISpatial::spatial_sector_point,&CISpatialWrapper::spatial_sector_point_static)
			.def("dcast_CObject",		&ISpatial::dcast_CObject,		&CISpatialWrapper::dcast_CObject_static)
			.def("dcast_FeelSound",		&ISpatial::dcast_FeelSound,		&CISpatialWrapper::dcast_FeelSound_static)
			.def("dcast_Renderable",	&ISpatial::dcast_Renderable,	&CISpatialWrapper::dcast_Renderable_static)
			.def("dcast_Light",			&ISpatial::dcast_Light,			&CISpatialWrapper::dcast_Light_static)
	];
}
*/

void ISheduledScript::script_register	(lua_State *L)
{
	module(L)
	[
		class_<ISheduled,CISheduledWrapper>("ISheduled")
//			.def(constructor<>())
//			.def("shedule_Scale",		&ISheduled::shedule_Scale,		&CISheduledWrapper::shedule_Scale_static)
//			.def("shedule_Update",		&ISheduled::shedule_Update,		&CISheduledWrapper::shedule_Update_static)
	];
}

void IRenderableScript::script_register	(lua_State *L)
{
	module(L)
	[
		class_<IRenderable,CIRenderableWrapper>("IRenderable")
//			.def(constructor<>())
//			.def("renderable_Render",&IRenderable::renderable_Render,&CIRenderableWrapper::renderable_Render_static)
//			.def("renderable_ShadowGenerate",&IRenderable::renderable_ShadowGenerate,&CIRenderableWrapper::renderable_ShadowGenerate_static)
//			.def("renderable_ShadowReceive",&IRenderable::renderable_ShadowReceive,&CIRenderableWrapper::renderable_ShadowReceive_static)
	];
}

void ICollidableScript::script_register	(lua_State *L)
{
	module(L)
	[
		class_<ICollidable>("ICollidable")
			.def(constructor<>())
	];
}


void CObjectScript::script_register		(lua_State *L)
{
	module(L)
	[
//		class_<CObject,bases<DLL_Pure,ISheduled,ICollidable,IRenderable>,CObjectWrapper>("CObject")
//			.def(constructor<>())
//			.def("_construct",			&CObject::_construct,&CObjectWrapper::_construct_static)
/*			
			.def("spatial_register",	&CObject::spatial_register,	&CObjectWrapper::spatial_register_static)
			.def("spatial_unregister",	&CObject::spatial_unregister,	&CObjectWrapper::spatial_unregister_static)
			.def("spatial_move",		&CObject::spatial_move,		&CObjectWrapper::spatial_move_static)
			.def("spatial_sector_point",&CObject::spatial_sector_point,&CObjectWrapper::spatial_sector_point_static)
			.def("dcast_FeelSound",		&CObject::dcast_FeelSound,		&CObjectWrapper::dcast_FeelSound_static)
			.def("dcast_Light",			&CObject::dcast_Light,			&CObjectWrapper::dcast_Light_static)
*/			
//			.def("shedule_Scale",		&CObject::shedule_Scale,		&CObjectWrapper::shedule_Scale_static)
//			.def("shedule_Update",		&CObject::shedule_Update,		&CObjectWrapper::shedule_Update_static)

//			.def("renderable_Render"		,&CObject::renderable_Render,&CObjectWrapper::renderable_Render_static)
//			.def("renderable_ShadowGenerate",&CObject::renderable_ShadowGenerate,&CObjectWrapper::renderable_ShadowGenerate_static)
//			.def("renderable_ShadowReceive",&CObject::renderable_ShadowReceive,&CObjectWrapper::renderable_ShadowReceive_static)
//			.def("Visual",					&CObject::Visual)

		class_<CGameObject,bases<DLL_Pure,ISheduled,ICollidable,IRenderable>,CGameObjectWrapper>("CGameObject")
			.def(constructor<>())
			.def("_construct",			&CGameObject::_construct,&CGameObjectWrapper::_construct_static)
			.def("Visual",				&CGameObject::Visual)			
/*
			.def("spatial_register",	&CGameObject::spatial_register,	&CGameObjectWrapper::spatial_register_static)
			.def("spatial_unregister",	&CGameObject::spatial_unregister,	&CGameObjectWrapper::spatial_unregister_static)
			.def("spatial_move",		&CGameObject::spatial_move,		&CGameObjectWrapper::spatial_move_static)
			.def("spatial_sector_point",&CGameObject::spatial_sector_point,&CGameObjectWrapper::spatial_sector_point_static)
			.def("dcast_FeelSound",		&CGameObject::dcast_FeelSound,		&CGameObjectWrapper::dcast_FeelSound_static)
			.def("dcast_Light",			&CGameObject::dcast_Light,			&CGameObjectWrapper::dcast_Light_static)
*/
//			.def("shedule_Scale",		&CGameObject::shedule_Scale,		&CGameObjectWrapper::shedule_Scale_static)
//			.def("shedule_Update",		&CGameObject::shedule_Update,		&CGameObjectWrapper::shedule_Update_static)

//			.def("renderable_Render"		,&CGameObject::renderable_Render,&CGameObjectWrapper::renderable_Render_static)
//			.def("renderable_ShadowGenerate",&CGameObject::renderable_ShadowGenerate,&CGameObjectWrapper::renderable_ShadowGenerate_static)
//			.def("renderable_ShadowReceive",&CGameObject::renderable_ShadowReceive,&CGameObjectWrapper::renderable_ShadowReceive_static)

			.def("net_Export",			&CGameObject::net_Export,		&CGameObjectWrapper::net_Export_static)
			.def("net_Import",			&CGameObject::net_Import,		&CGameObjectWrapper::net_Import_static)
			.def("net_Spawn",			&CGameObject::net_Spawn,	&CGameObjectWrapper::net_Spawn_static)

			.def("use",					&CGameObject::use,	&CGameObjectWrapper::use_static)

//			.def("setVisible",			&CGameObject::setVisible)
			.def("getVisible",			&CGameObject::getVisible)
			.def("getEnabled",			&CGameObject::getEnabled)
//			.def("setEnabled",			&CGameObject::setEnabled)

//		,class_<CPhysicsShellHolder,CGameObject>("CPhysicsShellHolder")
//			.def(constructor<>())

//		,class_<CEntity,CPhysicsShellHolder,CEntityWrapper>("CEntity")
//			.def(constructor<>())
//			.def("HitSignal",&CEntity::HitSignal,&CEntityWrapper::HitSignal_static)
//			.def("HitImpulse",&CEntity::HitImpulse,&CEntityWrapper::HitImpulse_static)

//		,class_<CEntityAlive,CEntity>("CEntityAlive")
//			.def(constructor<>())

//		,class_<CCustomMonster,CEntityAlive>("CCustomMonster")
//			.def(constructor<>())

//		,class_<CAI_Stalker,CCustomMonster>("CAI_Stalker")
	];
}

// alpet ======================== SCRIPT_TEXTURE_CONTROL BEGIN =========== 


IRender_Visual	*visual_get_child(IRender_Visual	*v, u32 n_child)
{

	if (!v) return NULL; // not have visual
	CKinematics *k = dynamic_cast<CKinematics*> (v);
	if (!k) return NULL;
	if (n_child >= k->children.size()) 		return NULL;
	return k->children.at(n_child);
}


// alpet: выборка текстуры из визуала по индексу
CTexture* visual_get_texture(IRender_Visual	*child_v, int n_texture)
{
	if (!child_v) return NULL; // not have visual
	
	CTexture* list[256] = { 0 };
	int tex_count = 0;

	n_texture = (n_texture > 255) ? 255 : n_texture;
	list[n_texture] = NULL;

	u32 verbose = 1;

	// визуал выстраивается иерархически - есть потомки и предки
	
	Shader* s = child_v->shader_ref._get();

	if (verbose > 5) Msg("# Shader *S = 0x%p name = <%s> ", s, child_v->shader_name); // shader расшарен для всех визуалов с одинаковыми текстурами и моделью

	if (s && s->E[0]._get()) // обычно в первом элементе находится исчерпывающий список текстур 
	{
		ShaderElement* E = s->E[0]._get();
		if (verbose > 5) Msg("#  ShaderElement *E = 0x%p", E);
		for (u32 p = 0; p < E->passes.size(); p++)
		if (E->passes[p]._get())
		{
			SPass* pass = E->passes[p]._get();
			if (verbose > 5) Msg("#   SPass *pass = 0x%p", pass);
			STextureList* tlist = pass->T._get();
			if (!tlist) continue;
			if (verbose > 5) Msg("#   STextureList *tlist = 0x%p, size = %d ", tlist, tlist->size());

			for (u32 t = 0; t < tlist->size() && tex_count <= n_texture; t++)
				list[tex_count++] = tlist->at(t).second._get();
		}
	}



	return list[n_texture];
}

void visual_configure (IRender_Visual	*child_v, LPCSTR new_shader, LPCSTR new_texture)
{
	if ((new_shader  && child_v->textures.size()) ||
		(new_texture && child_v->shader_name.size()))
	{
		// здесь производится замена текстуры и/или шейдера визуала
		s32 last_skinning = Render->m_skinning;

		Render->shader_option_skinning(1); // черт значит зачем нужно заменять флаг, но при дефолтном -1 вместо наложения текстуры будет покраска визуала  серым 
		child_v->shader_ref.destroy();

		LPCSTR sh_name = new_shader ? new_shader : *child_v->shader_name;
		LPCSTR tx_name = new_texture ? new_texture : *child_v->textures;

		child_v->shader_ref.create(sh_name, tx_name);
		child_v->shader_name = sh_name;
		child_v->textures = tx_name;

		Render->shader_option_skinning(last_skinning);
	}
}

void visual_set_texture(IRender_Visual *v, LPCSTR replace) { visual_configure(v, NULL, replace); }

void visual_set_shader(IRender_Visual *v,  LPCSTR replace) { visual_configure(v, replace, NULL); }

void IRender_VisualScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<IRender_Visual>("IRender_Visual")
			.def(constructor<>())
			.def("dcast_PKinematicsAnimated", &IRender_Visual::dcast_PKinematicsAnimated)
			.def("child",					  &visual_get_child)
			.def("configure",				  &visual_configure)
			
			.def("get_texture",				  &visual_get_texture)
			

			.def("get_shader_name",			  &IRender_Visual::get_shader_name)
			.def("get_texture_name",		  &IRender_Visual::get_texture_name)			
			.def("set_shader_name",			  &visual_set_shader)
			.def("set_texture_name",		  &visual_set_texture)

		];
}


void CKinematicsAnimated_PlayCycle(CKinematicsAnimated* sa, LPCSTR anim)
{
	sa->PlayCycle(anim);
}



void CKinematicsAnimatedScript::script_register		(lua_State *L)
{
	module(L)
	[
		class_<CKinematicsAnimated>("CKinematicsAnimated")
			.def("PlayCycle",		&CKinematicsAnimated_PlayCycle)
	];
}

void CBlendScript::script_register		(lua_State *L)
{
	module(L)
		[
			class_<CBlend>("CBlend")
			//			.def(constructor<>())
		];
}

// alpet ======================== SCRIPT_TEXTURE_CONTROL EXPORTS 2 =========== 

void child_visual_configure(IRender_Visual	*v, u32 n_child, LPCSTR new_shader, LPCSTR new_texture)
{

	if (!v)	return; // not have visual

	CKinematics *k = dynamic_cast<CKinematics*> (v);
	if (!k)	return;

	if (n_child >= k->children.size()) return;

	IRender_Visual*  child_v = k->children.at(n_child);

	visual_configure(child_v, new_shader, new_texture);
}


void script_object_set_texture (CScriptGameObject *script_obj, u32 nc, LPCSTR replace)
{
	IRender_Visual *v = script_obj->object().Visual();
	child_visual_configure (v, nc, NULL, replace);
}

void script_object_set_shader(CScriptGameObject *script_obj, u32 nc, LPCSTR replace)
{
	IRender_Visual *v = script_obj->object().Visual();
	child_visual_configure(v, nc, replace, NULL);
}

CTexture* script_object_get_texture (CScriptGameObject *script_obj, u32 n_child, u32 n_texture) 
{
	IRender_Visual *v = script_obj->object().Visual();
	v = visual_get_child (v, n_child);
	return visual_get_texture (v, n_texture);
}


CTexture* script_texture_create(LPCSTR name)
{
	return Device.Resources->_CreateTexture(name);
}

CTexture* script_texture_find(LPCSTR name) 
{
	return Device.Resources->_FindTexture(name);
}

void script_texture_delete(CTexture *t)
{
	xr_delete(t);	
}

LPCSTR script_texture_getname(CTexture *t) 
{
	return t->cName.c_str();
}

void script_texture_setname (CTexture *t, LPCSTR name)
{
	t->set_name(name);
}


void script_texture_load(CTexture *t)
{
	t->Load();
}

void script_texture_unload(CTexture *t)
{
	t->Unload();
}

void script_texture_reload(CTexture *t)
{
	t->Unload();
	t->Load();
}


void CTextureScript::script_register(lua_State *L)
{
	// added by alpet 10.07.14
	module(L)
		[
			class_<CTexture>("CTexture")			
			.def("delete",				&script_texture_delete)
			.def("find",				&script_texture_find)
			.def("load",				&script_texture_load)
			.def("reload",				&script_texture_reload)
			.def("unload",				&script_texture_unload)
			.def("get_name",			&script_texture_getname)
			.def("set_name",			&script_texture_setname)
			.def("get_surface",			&CTexture::surface_get)
			.def_readonly("ref_count",  &CTexture::dwReference)
			
		];
}


void CResourceManagerScript::script_register(lua_State *L)
{
	// added by alpet 10.07.14
	module(L) [ 
		// added by alpet
		def("texture_create",		&script_texture_create),
		def("texture_delete",		&script_texture_delete),
		def("texture_find",			&script_texture_find),
		def("texture_from_object",  &script_object_get_texture),
		def("texture_from_visual",  &visual_get_texture),
		def("texture_to_object",	&script_object_set_texture),
		def("texture_to_visual",	&visual_set_texture),
		def("texture_load",			&script_texture_load),
		def("texture_unload",		&script_texture_unload),
		def("texture_get_name",		&script_texture_getname),
		def("texture_set_name",		&script_texture_setname)
	];
}

// alpet ======================== SCRIPT_TEXTURE_CONTROL END =========== 

// alpet ======================== CAMERA SCRIPT OBJECT =================

ENGINE_API extern float psHUD_FOV;

CCameraBase* actor_camera(u16 index)
{
	CActor *pA = smart_cast<CActor *>(Level().CurrentEntity());
	if (!pA) return NULL;
	return pA->cam_ByIndex(index);
}


float global_fov(float new_fov = 0)
{
	if (new_fov > 0.1)
		g_fov = new_fov;
	return g_fov;
}

float hud_fov(float new_fov = 0)
{
	if (new_fov > 0.1)
		psHUD_FOV = new_fov;
	return psHUD_FOV;

}

void CCameraBaseScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CCameraBase>("CCameraBase")
			.def_readwrite("aspect",		&CCameraBase::f_aspect)
			.def_readonly ("direction",		&CCameraBase::vDirection)
			.def_readwrite("fov",			&CCameraBase::f_fov)
			.def_readwrite("position",		&CCameraBase::vPosition)

			.def_readwrite("lim_yaw",		&CCameraBase::lim_yaw)
			.def_readwrite("lim_pitch",		&CCameraBase::lim_pitch)
			.def_readwrite("lim_roll",		&CCameraBase::lim_roll)

			.def_readwrite("yaw",			&CCameraBase::yaw)
			.def_readwrite("pitch",			&CCameraBase::pitch)
			.def_readwrite("roll",			&CCameraBase::roll),


			def("actor_camera",				&actor_camera),
			def("get_global_fov",			&global_fov),
			def("set_global_fov",			&global_fov),
			def("get_hud_fov",				&hud_fov),
			def("set_hud_fov",				&hud_fov)




		];
}
// alpet ======================== CAMERA SCRIPT OBJECT =================



/*
void CKinematicsScript::script_register		(lua_State *L)
{
	module(L)
		[
			class_<CKinematics, FHierrarhyVisual>("CKinematics")
			//			.def(constructor<>())
		];
}

void FHierrarhyVisualScript::script_register		(lua_State *L)
{
	module(L)
		[
			class_<FHierrarhyVisual, IRender_Visual>("FHierrarhyVisual")
			//			.def(constructor<>())
		];
}
*/
