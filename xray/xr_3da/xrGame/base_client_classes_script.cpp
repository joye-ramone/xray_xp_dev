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



void IRender_VisualScript::script_register		(lua_State *L)
{
	module(L)
	[
		class_<IRender_Visual>("IRender_Visual")
			.def(constructor<>())
			.def("dcast_PKinematicsAnimated", &IRender_Visual::dcast_PKinematicsAnimated)			
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

// alpet ======================== SCRIPT_TEXTURE_CONTROL BEGIN =========== 

// alpet: выборка текстуры из визуала по индексу
CTexture* script_visual_getset_texture(CScriptGameObject *script_obj, int n, LPCSTR replace)
{
	IRender_Visual* parent_v = script_obj->object().Visual();
	if (!parent_v)
		return NULL; // not have visual

	CKinematics *k = dynamic_cast<CKinematics*> (parent_v);
	if (!k)
		return NULL;


	CTexture* list[256] = { 0 };
	int tex_count = 0;

	list[n] = NULL;

	n = (n > 255) ? 255 : n;

	u32 verbose = 1;

	// визуал выстраивается иерархически - есть потомки и предки
	for (u32 cn = 0; cn < k->children.size(); cn++)
	{
		IRender_Visual*  child_v = k->children.at(cn);

		Shader* s = child_v->shader_ref._get();
		if (verbose > 5) Msg("# Shader *S = 0x%p name = <%s>, child_num = %d ", s, child_v->shader_name, cn); // shader расшарен для всех визуалов с одинаковыми текстурами и моделью

		if (s->E[0]._get()) // обычно в первом элементе находится исчерпывающий список текстур 
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

				for (u32 t = 0; t < tlist->size() && tex_count <= n; t++)
					list[tex_count++] = tlist->at(t).second._get();
			}
		}

		if (tex_count && replace && strlen(child_v->shader_name))
		{
			// здесь производится замена текстуры визуала, через фактическое пересоздание шейдера 
			s32 last_skinning = Render->m_skinning;

			Render->shader_option_skinning(1); // черт значит зачем нужно заменять флаг, но при дефолтном -1 вместо наложения текстуры будет покраска визуала  серым 
			child_v->shader_ref.destroy();
			child_v->shader_ref.create(child_v->shader_name, replace);

			Render->shader_option_skinning(last_skinning);
		}

	}

	return list[n];
}

CTexture* script_visual_get_texture(CScriptGameObject *script_obj, int n)
{
	return script_visual_getset_texture(script_obj, n, NULL);
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



void CTextureScript::script_register(lua_State *L)
{
	// added by alpet 10.07.14
	module(L)
		[
			class_<CTexture>("CTexture")			
			.def("delete",				&script_texture_delete)
			.def("find",				&script_texture_find)
			.def("load",				&script_texture_load)
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
		def("texture_from_visual",  &script_visual_get_texture),
		def("texture_to_visual",	&script_visual_getset_texture),
		def("texture_load",			&script_texture_load),
		def("texture_unload",		&script_texture_unload),
		def("texture_get_name",		&script_texture_getname),
		def("texture_set_name",		&script_texture_setname)
	];
}

// alpet ======================== SCRIPT_TEXTURE_CONTROL END =========== 

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
