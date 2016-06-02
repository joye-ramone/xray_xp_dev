#include "pch_script.h"
#include "UIScriptWnd.h"
#include "../HudManager.h"
#include "../object_broker.h"
#include "../callback_info.h"
#include <lua.h>
#include <luabind\luabind.hpp>

// #include "uiscriptwnd_script.h"

extern LPCSTR raw_lua_class_name(lua_State *L);
extern u32	  g_last_gc;

u32 last_dialog_destroy = 0;

struct event_comparer{
	shared_str			name;
	s16					event;

	event_comparer(shared_str n, s16 e):name(n),event(e){}
	bool operator ()(SCallbackInfo* i){
		return( (i->m_controlName==name) && (i->m_event==event) );
	}
};

CUIDialogWndEx::CUIDialogWndEx():inherited()
{
	Hide();	
	m_lua_vm  = NULL;
	m_self_ref = NULL;
	self_name = "new";
}

CUIDialogWndEx::~CUIDialogWndEx()
{
	g_last_gc = Device.dwTimeGlobal;  // чтобы отодвинуть очистку мусора
	last_dialog_destroy = g_last_gc;
	MsgCB("# #DBG: CUIDialogWndEx destructor for %s ", WindowName_script());
	if (m_self_ref)
		m_self_ref->m_dialog = NULL;

	try {
		delete_data(m_callbacks);
	}
	catch(...) {
	}
}
void CUIDialogWndEx::Register			(CUIWindow* pChild)
{
	pChild->SetMessageTarget(this);
}

void CUIDialogWndEx::Register(CUIWindow* pChild, LPCSTR name)
{
	pChild->SetWindowName(name);
	pChild->SetMessageTarget(this);
}

void CUIDialogWndEx::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
	event_comparer ec(pWnd->WindowName(),msg);

	CALLBACK_IT it = std::find_if(m_callbacks.begin(),m_callbacks.end(),ec);
	if(it==m_callbacks.end())
		return inherited::SendMessage(pWnd, msg, pData);

	((*it)->m_callback)();

//	if ( (*it)->m_cpp_callback )	
//		(*it)->m_cpp_callback(pData);
}

bool CUIDialogWndEx::Load(LPCSTR xml_name)
{
	return true;
}

SCallbackInfo*	CUIDialogWndEx::NewCallback ()
{
	m_callbacks.push_back( xr_new<SCallbackInfo>() );
	return m_callbacks.back();
}

void CUIDialogWndEx::AddCallback(LPCSTR control_id, s16 event, const luabind::functor<void> &lua_function)
{
	SCallbackInfo* c	= NewCallback ();
	c->m_callback.set	(lua_function);
	c->m_controlName	= control_id;
	c->m_event			= event;		
	
}


void CUIDialogWndEx::AddCallback (LPCSTR control_id, s16 event, const luabind::functor<void> &functor, const luabind::object &object)
{
	SCallbackInfo* c	= NewCallback ();
	c->m_callback.set	(functor,object);
	c->m_controlName	= control_id;
	c->m_event			= event;
	if (self_name == "new")
	{
		m_lua_vm = object.lua_state();
		if (m_lua_vm)
		{
			self_name = "";
			int top = lua_gettop(m_lua_vm);
			int err = luaL_loadstring(m_lua_vm, "return get_script_name()");
  		    if (0 == err)
	  		    err = lua_pcall(m_lua_vm, 0, 1, 0);
			if (0 == err && lua_isstring(m_lua_vm, -1))			
				self_name = xr_sprintf("%s.", lua_tostring(m_lua_vm, -1));							
			else
				Msg("! #WARN: get_script_name returned @%s for lua_State = %s", lua_typename(m_lua_vm, -1), get_lvm_name(m_lua_vm));	
			object.pushvalue();
			self_name += raw_lua_class_name(m_lua_vm);
			lua_settop(m_lua_vm, top);
			SetWindowName(self_name.c_str());
		}
	}
}


bool CUIDialogWndEx::OnKeyboard(int dik, EUIMessages keyboard_action)
{
	return inherited::OnKeyboard(dik,keyboard_action);
}
void CUIDialogWndEx::Update()
{
	inherited::Update();
}
