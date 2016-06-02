#include "pch_script.h"

//UI-controls
#include "UIScriptWnd.h"
#include "UIButton.h"
#include "UIMessageBox.h"
#include "UIPropertiesBox.h"
#include "UICheckButton.h"
#include "UIRadioButton.h"
#include "UIStatic.h"
#include "UIEditBox.h"
#include "UIFrameWindow.h"
#include "UIFrameLineWnd.h"
#include "UIProgressBar.h"
#include "UITabControl.h"
#include <lua.h>
#include <lstate.h>
#include "../script_storage.h"
#include "uiscriptwnd_script.h"

using namespace luabind;


extern u32 last_dialog_destroy;
u32 get_last_dialog_destroy() { return last_dialog_destroy; }

#pragma optimize("gyts", off)
void CUIDialogWndEx::Destroy()
{
	WrapType *self = smart_cast<WrapType*> (this);
	lua_State *L = this->m_lua_vm;
	if (self)
	{
		// weak_ref &ref = static_cast<weak_ref&> (detail::wrap_access::ref(*self));
		// ref.~weak_ref();  // Debug trace only 
		//  xr_delete(self);
		self->~CWrapperBase();
	}
	else
	{
		auto *wnd = this;
		xr_delete(wnd);
	}

	if (L)
	__try {
		lua_gc(L, LUA_GCCOLLECT, 0);
		lua_gc(L, LUA_GCCOLLECT, 0);
	}
	__except (SIMPLE_FILTER) {
		MsgCB("! #EXCEPTION: in CUIDialogWndEx::Destroy()");
	}

}


extern export_class &script_register_ui_window1(export_class &);
extern export_class &script_register_ui_window2(export_class &);

#pragma optimize("s",on)
void CUIDialogWndEx::ScriptInit(lua_State *L)
{
	if (!L) return;
	self_name = "unknown";
	int top = lua_gettop(L);
	if (top && lua_isstring(L, 1))
		self_name = lua_tostring(L, 1); // имя диалога в параметре	
	SetWindowName(self_name.c_str());
}


void CUIDialogWndEx::script_register(lua_State *L)
{
	export_class				instance("CUIScriptWnd");

	module(L)
	[
		script_register_ui_window2(
			script_register_ui_window1(
				instance
			)
		)
		.def("Load",			&BaseType::Load),
		def("last_dialog_dstr", &get_last_dialog_destroy)
	];
}


extern void destroy_dialog(CUIDialogWnd* pDialog);

export_class &script_register_ui_window1(export_class &instance)
{
	instance
		.def(					constructor<>())

		.def("AddCallback",		(void(BaseType::*)(LPCSTR, s16, const luabind::functor<void>&))&BaseType::AddCallback)
		.def("AddCallback",		(void(BaseType::*)(LPCSTR, s16, const luabind::functor<void>&, const luabind::object&))&BaseType::AddCallback)		
		.def("Destroy",			&destroy_dialog)	
		.def("Register",		(void (BaseType::*)(CUIWindow*))&BaseType::Register)
		.def("Register",		(void (BaseType::*)(CUIWindow*,LPCSTR))&BaseType::Register)

		.def("GetButton",		(CUIButton* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUIButton>)
		.def("GetMessageBox",	(CUIMessageBox* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUIMessageBox>)
		.def("GetPropertiesBox",(CUIPropertiesBox* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUIPropertiesBox>)
		.def("GetCheckButton",	(CUICheckButton* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUICheckButton>)
		.def("GetRadioButton",	(CUIRadioButton* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUIRadioButton>)
//		.def("GetRadioGroup",	(CUIRadioGroup* (BaseType::*)(LPCSTR)) &BaseType::GetControl<CUIRadioGroup>)

	;return	(instance);
}
