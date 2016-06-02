///////////////////////////////////////////////////////////////
// ExoOutfit.h
// ExoOutfit - защитный костюм с усилением
///////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "exooutfit.h"
#include "Level.h"
#include "migration.h"
#include "xrserver_objects_alife_items.h"

CExoOutfit::CExoOutfit()
{
	m_bPowered = 0;
	m_fBatteryCharge = 1.0f;
	m_fBatteryCritical = 0.05f;
}

CExoOutfit::~CExoOutfit() 
{
}

float CExoOutfit::GetPowerLoss() 
{
	if (m_fPowerLoss < 1.0f && 
		(  GetCondition() <= 0.35 || m_bPowered && m_fBatteryCharge <= m_fBatteryCritical ) )
	{
		return 1.0f;			
	};
	return __min (1.25f, m_fPowerLoss / (GetCondition() + 0.001f));
};

float CExoOutfit::GetAdditionalWeight(int index) const
{
	float w = (index == 1 ? m_additional_weight : m_additional_weight2);
	if (m_bPowered && m_fBatteryCharge <= m_fBatteryCritical) w = 0;
	return w;
}


void CExoOutfit::Load(LPCSTR section)
{

	inherited::Load(section);
	m_bPowered =	   !!READ_IF_EXISTS(pSettings, r_bool,  section, "is_powered", true);	
	m_fBatteryCritical = READ_IF_EXISTS(pSettings, r_float, section, "battery_critical", m_fBatteryCritical);	
}

void CExoOutfit::net_Export(NET_Packet& P)
{
	inherited::net_Export	(P);	
	if ( alife_object()->m_wVersion < 120 ) return;
	P.w_float_q8(m_fBatteryCharge, 0.f, 1.0f);
}

void CExoOutfit::net_Import(NET_Packet& P)
{
	inherited::net_Import	(P);	
	
	if (m_wPacketVersion >= 120 && m_dwPacketTag == MIG_PACKET_120)
	{
		P.r_float_q8(m_fBatteryCharge, 0.f, 1.0f);
	}

}