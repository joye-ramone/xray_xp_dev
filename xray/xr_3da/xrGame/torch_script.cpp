#include "pch_script.h"
#include "torch.h"
#include "script_game_object.h"

using namespace luabind;


#define  CLASS_IMPL		CTorch
#define  target_0		light_render
#define  target_1		light_omni
#define	 target_2		glow_render
#include "light_ext.inc"
#undef   target_0
#undef	 target_1
#undef	 target_2 
#undef   CLASS_IMPL


CTorch *get_torch(CScriptGameObject *script_obj)
{
	CGameObject *obj = &script_obj->object();
	return smart_cast<CTorch*> (obj);
}

#pragma optimize("s",on)
void CTorch::script_register	(lua_State *L)
{
	module(L)
	[
		class_<CTorch,CGameObject>("CTorch")
			.def(constructor<>())
			// alpet: управление параметрами света
			.def("set_angle",	&CTorch::SetAngle)
			.def("set_color",	&CTorch::SetColor)
			.def("set_rgb",		&CTorch::SetRGB)
			.def("set_range",	&CTorch::SetRange)
			.def("set_texture", &CTorch::SetTexture),
			def("get_torch_obj", &get_torch) 
	];
}
