#include "stdafx.h"
#include "UIDialogHolder.h"
#include "ui\UIDialogWnd.h"
#include "UICursor.h"
#include "level.h"
#include "actor.h"
#include "xr_level_controller.h"

dlgItem::dlgItem(CUIWindow* pWnd)
{
	wnd		= pWnd;
	enabled = true;
}

bool dlgItem::operator < (const dlgItem& itm) const
{
	return (int)enabled > (int)itm.enabled;
}

bool operator == (const dlgItem& i1, const dlgItem& i2)
{
	return i1.wnd == i2.wnd;
}

recvItem::recvItem(CUIDialogWnd* r)
{
	m_item			= r;
	m_flags.zero	();
}
bool operator == (const recvItem& i1, const recvItem& i2)
{
	return i1.m_item == i2.m_item;
}

CDialogHolder::CDialogHolder()
{
	shedule.t_min			= 5;
	shedule.t_max			= 20;
	shedule_register		();
	Device.seqFrame.Add		(this,REG_PRIORITY_LOW-1000);
}

CDialogHolder::~CDialogHolder()
{
	shedule_unregister();
	Device.seqFrame.Remove		(this);
}
#include "HUDManager.h"

void CDialogHolder::StartMenu (CUIDialogWnd* pDialog, bool bDoHideIndicators)
{
	R_ASSERT						( !pDialog->IsShown() );

	AddDialogToRender				(pDialog);
	SetMainInputReceiver			(pDialog, false);

	if(UseIndicators())
	{
		bool b							= !!psHUD_Flags.test(HUD_CROSSHAIR_RT);
		m_input_receivers.back().m_flags.set(recvItem::eCrosshair, b);

		b								= HUD().GetUI()->GameIndicatorsShown();
		m_input_receivers.back().m_flags.set(recvItem::eIndicators, b);
		
		if(bDoHideIndicators){
			psHUD_Flags.set				(HUD_CROSSHAIR_RT, FALSE);
			HUD().GetUI					()->HideGameIndicators();
		}
	}
	pDialog->SetHolder				(this);
	pDialog->Show					();

	if( pDialog->NeedCursor() )
		GetUICursor()->Show();

	if(g_pGameLevel)
	{
		CActor* A	= smart_cast<CActor*>( Level().CurrentViewEntity() );
		if ( A && pDialog->StopAnyMove() )
		{
			A->StopAnyMove				();
			A->PickupModeOff			();
		};
		if(A)
		{	
			A->IR_OnKeyboardRelease		(kWPN_ZOOM);
			A->IR_OnKeyboardRelease		(kWPN_FIRE);
		}
	}
}


void CDialogHolder::StopMenu (CUIDialogWnd* pDialog)
{
	R_ASSERT( pDialog->IsShown() );

	RemoveDialogToRender	(pDialog);
	if( MainInputReceiver() == pDialog )
	{
		if(UseIndicators()) // после удаление главного приемника, восстановить элементы худа
		{
			bool b					= !!m_input_receivers.back().m_flags.test(recvItem::eCrosshair);
			psHUD_Flags.set			(HUD_CROSSHAIR_RT, b);
			b						= !!m_input_receivers.back().m_flags.test(recvItem::eIndicators);
			if(b)					HUD().GetUI()->ShowGameIndicators();
			else					HUD().GetUI()->HideGameIndicators();
		}
		
		SetMainInputReceiver	(NULL,false);		
	}	
	else {				
		SetMainInputReceiver	(pDialog, true); // не установка, а поиск-удаление из рессиверов		
	}

	pDialog->SetHolder		(NULL);
	pDialog->Hide			();

	if(!MainInputReceiver() || !MainInputReceiver()->NeedCursor() )
		GetUICursor()->Hide();
}

void CDialogHolder::AddDialogToRender(CUIWindow* pDialog)
{
	dlgItem itm(pDialog);
	xr_vector<dlgItem>::iterator it = std::find(m_dialogsToRender.begin(),m_dialogsToRender.end(),itm);
	if( (it == m_dialogsToRender.end()) || ( it != m_dialogsToRender.end() && (*it).enabled==false)  )
	{
		m_dialogsToRender.push_back(itm);
		pDialog->Show(true);
	}
}

void CDialogHolder::RemoveDialogToRender(CUIWindow* pDialog)
{
	dlgItem itm(pDialog);
	xr_vector<dlgItem>::iterator it = std::find(m_dialogsToRender.begin(),m_dialogsToRender.end(),itm);
	if(it != m_dialogsToRender.end())
	{
		(*it).wnd->Show(false);
		(*it).wnd->Enable(false);
		(*it).enabled = false;
	}
}



void CDialogHolder::DoRenderDialogs()
{
	xr_vector<dlgItem>::iterator it = m_dialogsToRender.begin();
	for(; it!=m_dialogsToRender.end();++it){
		if( (*it).enabled && (*it).wnd->IsShown() )
			(*it).wnd->Draw();
	}
}

CUIDialogWnd* CDialogHolder::MainInputReceiver()
{ 
	if ( !m_input_receivers.empty() ) 
		return m_input_receivers.back().m_item; 
	return NULL; 
};

void CDialogHolder::SetMainInputReceiver	(CUIDialogWnd* ir, bool _find_remove)	
{ 
	if( MainInputReceiver() == ir ) return;

	if(!ir || _find_remove){
		if(m_input_receivers.empty())	return;

		if(!ir)
			m_input_receivers.pop_back();
		else{
			VERIFY(ir && _find_remove);

			u32 cnt = m_input_receivers.size();
			for(;cnt>0;--cnt)
				if( m_input_receivers[cnt-1].m_item == ir ){
					m_input_receivers[cnt].m_flags.set(recvItem::eCrosshair, m_input_receivers[cnt-1].m_flags.test(recvItem::eCrosshair) );
					m_input_receivers[cnt].m_flags.set(recvItem::eIndicators, m_input_receivers[cnt-1].m_flags.test(recvItem::eIndicators) );
					xr_vector<recvItem>::iterator it = m_input_receivers.begin();
					std::advance			(it,cnt-1);
					m_input_receivers.erase	(it);
					break;
				}
			
		}

	}else{
		m_input_receivers.push_back(recvItem(ir));
	}
};
//. #include "ai_space.h"
//. #include "script_engine.h"
void CDialogHolder::StartStopMenu(CUIDialogWnd* pDialog, bool bDoHideIndicators)
{
	//.	ai().script_engine().script_log	(eLuaMessageTypeError,"foo");
	if (pDialog->IsShown())
		StopMenu(pDialog);
	else
	{
		GetUICursor()->UpdateCursorPosition();
		Fvector2 mp = GetUICursor()->GetCursorPosition();
		if (mp.x >= UI_BASE_WIDTH - 1 || mp.y >= UI_BASE_HEIGHT - 1) 
			GetUICursor()->SetUICursorPosition		(Fvector2().set(512, 400));
		StartMenu(pDialog, bDoHideIndicators);
	}
	
}

void CDialogHolder::OnFrame	()
{
	// xr_vector<dlgItem>::iterator it = m_dialogsToRender.begin();
	CUIWindow *wnd = NULL;
	for (size_t it = 0; it < m_dialogsToRender.size(); ++it)
	__try
	{		
		auto &item = m_dialogsToRender[it];
		wnd = item.wnd;
		if (item.enabled && wnd && wnd->IsEnabled())	
			wnd->Update();
	}
	__except (SIMPLE_FILTER)
	{
		Msg("! #EXCEPTION: CDialogHolder::OnFrame() for item %d, wnd = 0x%p ", it, wnd);
	}

}

void CDialogHolder::shedule_Update(u32 dt)
{
	ISheduled::shedule_Update(dt);

	if (m_dialogsToRender.empty())
		return;

	std::sort			(m_dialogsToRender.begin(), m_dialogsToRender.end());

	while ((m_dialogsToRender.size()) && (!m_dialogsToRender[m_dialogsToRender.size() - 1].enabled))
	{
		MsgCB("# #DBG: m_dialogsToRender[%d] releasing.", m_dialogsToRender.size() - 1);
		m_dialogsToRender.pop_back();
	}
}

float CDialogHolder::shedule_Scale()
{
	return 0.5f;
}

void CDialogHolder::CleanInternals()
{
	while( !m_input_receivers.empty() )
		m_input_receivers.pop_back();

	m_dialogsToRender.clear	();
	GetUICursor()->Hide		();
}
