////////////////////////////////////////////////////////////////////////////
//	Module 		: nvd.cpp
//	Created 	: 03.01.2016
//  Modified 	: 03.01.2016
//	Author		: Alexander Petrov
//	Description : Night vision device extracted from CTorch 
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "pch_script.h"
#include "nvd.h"
#include "HUDManager.h"
#include "level.h"
#include "Actor.h"
#include "actorEffector.h"
#include "ai_sounds.h"
#include "inventory.h"
#include "../LightAnimLibrary.h"
#include "xrserver_objects_alife_items.h"

#pragma optimize("gyts", off)

CNightVisionDevice::CNightVisionDevice()
{

	/*m_RechargeTime	= 6.f;
	m_RechargeTimeMin= 2.f;
	m_DischargeTime	= 10.f;
	m_ChargeTime		= 0.f;*/


}

CNightVisionDevice::~CNightVisionDevice()
{
	HUD_SOUND::DestroySound	(m_OnSnd);
	HUD_SOUND::DestroySound	(m_OffSnd);
	HUD_SOUND::DestroySound	(m_IdleSnd);
	HUD_SOUND::DestroySound	(m_BrokenSnd);
}

void CNightVisionDevice::InitDevice(LPCSTR section)
{
	if (!pSettings->line_exist(section, "night_vision_device")) return;
	m_bEnabled = true;
	m_DeviceSect = pSettings->r_string(section, "night_vision_device");  // section of device config
	LoadConfig(*m_DeviceSect);
}

void CNightVisionDevice::LoadConfig(LPCSTR section)
{
	R_ASSERT2 (pSettings->section_exist(section), section);

	m_DeviceSect = section;
	m_EffectorSect = pSettings->r_string(section, "effector");

	if(pSettings->line_exist(section, "sound_on") )
	{		
		HUD_SOUND::LoadSound(section, "sound_on",     m_OnSnd,     SOUND_TYPE_ITEM_USING);
		HUD_SOUND::LoadSound(section, "sound_off",    m_OffSnd,    SOUND_TYPE_ITEM_USING);
		HUD_SOUND::LoadSound(section, "sound_idle",   m_IdleSnd,   SOUND_TYPE_ITEM_USING);
		HUD_SOUND::LoadSound(section, "sound_broken", m_BrokenSnd, SOUND_TYPE_ITEM_USING);

	
		/*m_RechargeTime		= pSettings->r_float(section, "recharge_time");
		m_RechargeTimeMin	= pSettings->r_float(section, "recharge_time_min");
		m_DischargeTime		= pSettings->r_float(section, "discharge_time");
		m_ChargeTime			= m_RechargeTime;*/
	}
}

void CNightVisionDevice::SwitchNightVision()
{
	if (OnClient()) return;
	SwitchNightVision(!m_bPowered);	
}

void CNightVisionDevice::SwitchNightVision(bool vision_on)
{
	if (m_bPowered == vision_on) return;

	
	if(vision_on /*&& (m_ChargeTime > m_RechargeTimeMin || OnClient())*/)
	{
		//m_ChargeTime = m_DischargeTime*m_ChargeTime/m_RechargeTime;
		m_bPowered = m_bEnabled;
	}
	else
	{
		m_bPowered = false;
	}

	//CActor *pA = smart_cast<CActor *>(H_Parent());

	CActor *pA = Actor();

	if(!pA)					return;	
	bool bPlaySoundFirstPerson = (pA == Level().CurrentViewEntity());

	// проверка на запретные локации
	LPCSTR disabled_names	= pSettings->r_string(*m_DeviceSect,"disabled_maps");
	LPCSTR curr_map			= *Level().name();
	u32 cnt					= _GetItemCount(disabled_names);
	bool b_allow			= true;
	string512				tmp;
	for(u32 i=0; i<cnt; ++i) {
		_GetItem(disabled_names, i, tmp);
		if(0==stricmp(tmp, curr_map)){
			b_allow = false;
			break;
		}
	}
		
	if (m_EffectorSect.size() && !b_allow) {
		HUD_SOUND::PlaySound(m_BrokenSnd, pA->Position(), pA, bPlaySoundFirstPerson);
		return;
	}

	if(m_bPowered) {
		CEffectorPP* pp = pA->Cameras().GetPPEffector((EEffectorPPType)effNightvision);
		if(!pp) {
			if (m_EffectorSect.size())
			{
				AddEffector(pA,effNightvision, m_EffectorSect);
				HUD_SOUND::PlaySound(m_OnSnd, pA->Position(), pA, bPlaySoundFirstPerson);
				HUD_SOUND::PlaySound(m_IdleSnd, pA->Position(), pA, bPlaySoundFirstPerson, true);
			}
		}
	} else {
 		CEffectorPP* pp = pA->Cameras().GetPPEffector((EEffectorPPType)effNightvision);
		if(pp){
			pp->Stop			(1.0f);
			HUD_SOUND::PlaySound(m_OffSnd, pA->Position(), pA, bPlaySoundFirstPerson);
			HUD_SOUND::StopSound(m_IdleSnd);
		}
	}
}


void CNightVisionDevice::UpdateSwitchNightVision   ()
{
	if (!m_bEnabled)
	{
		if (m_bPowered)	TurnOff();
		return;
	}
	if (OnClient()) return;

	// TODO: add optionally battery reference
	/*if(m_bPowered)
	{
		m_ChargeTime			-= Device.fTimeDelta;

		if(m_ChargeTime<0.f)
			SwitchNightVision(false);
	}
	else
	{
		m_ChargeTime			+= Device.fTimeDelta;
		clamp(m_ChargeTime, 0.f, m_RechargeTime);
	}*/
}

CPortableNVD::CPortableNVD()
{
	SetSlot	   (TORCH_SLOT);
	need_slot = true;
}

CPortableNVD::~CPortableNVD()
{
	//	CNightVisionDevice::~CNightVisionDevice();
}

BOOL CPortableNVD::net_Spawn(CSE_Abstract* DC)
{
	SetSlot					(TORCH_SLOT);
	CSE_ALifeItemNVD		*nvd	= smart_cast<CSE_ALifeItemNVD*>(DC);
	cNameVisual_set			(nvd->get_visual());

	R_ASSERT				(!CFORM());
	R_ASSERT				(smart_cast<CKinematics*>(Visual()));
	collidable.model		= xr_new<CCF_Skeleton>	(this);

	if (!inherited::net_Spawn(DC))
		return				(FALSE);
	
	m_bEnabled = nvd->m_enabled;
	SwitchNightVision (nvd->m_active);
	// bool b_r2				= !!psDeviceFlags.test(rsR2);

	// CKinematics* K			= smart_cast<CKinematics*>(Visual());
	// CInifile* pUserData		= K->LL_UserData(); 
	// R_ASSERT3				(pUserData,"Empty NVD user data!",nvd->get_visual());
	// lanim					= LALib.FindItem(pUserData->r_string("torch_definition","color_animator"));
	// guid_bone				= K->LL_BoneID	(pUserData->r_string("torch_definition","guide_bone"));	VERIFY(guid_bone!=BI_NONE);

	return TRUE;
}
void CPortableNVD::net_Destroy() 
{
	TurnOff();
}
void CPortableNVD::net_Export(NET_Packet& P) 
{
	inherited::net_Export		(P);
	u8 flags = 0;
	if (m_bEnabled) flags |= eEnabled;
	if (m_bPowered)	   flags |= eActive;
	const CActor *pA = smart_cast<const CActor *>(H_Parent());
	if (pA)
	{
		if (pA->attached(this))
			flags |= eAttached;
	}
	P.w_u8(flags);
}				
void CPortableNVD::net_Import(NET_Packet& P)
{
	inherited::net_Import		(P);
	u8 flags = P.r_u8();
	m_bEnabled = !!(flags & eEnabled);
	SwitchNightVision ( !!(flags & eActive) );
}			

void CPortableNVD::OnH_A_Chield()
{


}
void CPortableNVD::OnH_B_Independent(bool just_before_destroy)
{
	TurnOff	();
	HUD_SOUND::StopSound		(m_OnSnd);
	HUD_SOUND::StopSound		(m_OffSnd);
	HUD_SOUND::StopSound		(m_IdleSnd);

	// m_ChargeTime		= m_RechargeTime;
}

void CPortableNVD::UpdateCL()
{
	UpdateSwitchNightVision		();
}

#define ATTACHABLE_ITEM CPortableNVD  // вместо шаблона - инклуд
#include "can_be_attached.inc"
#undef ATTACHABLE_ITEM
