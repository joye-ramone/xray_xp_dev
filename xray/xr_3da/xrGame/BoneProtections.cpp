#include "stdafx.h"
#include "BoneProtections.h"
#include "../skeletonanimated.h"

#pragma optimize("gyts", off)




xr_map<const shared_str, SBoneProtections::BoneProtection> default_config;

void SetDefaultConfig (LPCSTR bone_name, float importance, u32 flags)
{
	SBoneProtections::BoneProtection BP;
	BP.importance = importance;
	BP.flags	  = flags;

	default_config[shared_str(bone_name)] = BP;
}

const SBoneProtections::BoneProtection GetDefaultConfig(const shared_str &bone_name)
{
	SBoneProtections::BoneProtection BP = { 0 };
	BP.importance = 0.0001f; // для всяких там пальцев и прочего


	auto it = default_config.find(bone_name);
	if (it != default_config.end())
		return it->second;
	else
		return BP;

}

SBoneProtections::SBoneProtections()
{  
   m_default.koeff = 1.0f; 
   m_default.armour = 0; 
   m_default.condition = 1.0f; 
   m_default.importance = 0.0f; 
   m_default.flags = eBodyElement;

   if (default_config.size() == 0)
   {
	   SetDefaultConfig ("bip01_head",	0.03, eHeadElement | eUpperHalf);
	   SetDefaultConfig ("bip01_neck",	0.03, eHeadElement | eUpperHalf);
	   SetDefaultConfig ("eyelid_1",	0.01, eHeadElement | eUpperHalf);
	   SetDefaultConfig ("eye_left",	0.02, eHeadElement | eUpperHalf);
	   SetDefaultConfig ("eye_right",	0.02, eHeadElement | eUpperHalf);
	   SetDefaultConfig ("jaw_1",		0.02, eHeadElement | eUpperHalf);
	   // 13%


	   SetDefaultConfig ("bip01_pelvis",	0.04, eBodyElement | eLowerHalf );
	   SetDefaultConfig ("bip01_spine",		0.07, eBodyElement | eUpperHalf );
	   SetDefaultConfig ("bip01_spine1",	0.04, eBodyElement | eUpperHalf );
	   SetDefaultConfig ("bip01_spine2",	0.04, eBodyElement | eUpperHalf );
	   // +19%
		
	   SetDefaultConfig("bip01_l_clavicle", 0.030, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_l_upperarm", 0.040, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_l_forearm",  0.050, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_l_hand",		0.050, eHandElement | eUpperHalf);
	   // +17%

   	   SetDefaultConfig("bip01_r_clavicle", 0.030, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_r_upperarm", 0.040, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_r_forearm",  0.050, eHandElement | eUpperHalf);
	   SetDefaultConfig("bip01_r_hand",		0.050, eHandElement | eUpperHalf);
	   // +17%

	   SetDefaultConfig("bip01_l_thig", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_l_calf", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_l_foot", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_l_toe0", 0.02, eLegElement | eLowerHalf);
	   // +17%
	   SetDefaultConfig("bip01_r_thig", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_r_calf", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_r_foot", 0.05, eLegElement | eLowerHalf);
	   SetDefaultConfig("bip01_r_toe0", 0.02, eLegElement | eLowerHalf);
	   // +17%
   }
}

float SBoneProtections::getBoneProtection	(s16 bone_id)
{
	storage_it it = m_bones_koeff.find(bone_id);
	if( it != m_bones_koeff.end() )
		return it->second.koeff * it->second.condition;
	else
		return m_default.koeff * m_default.condition;
}

float SBoneProtections::getBoneArmour	(s16 bone_id)
{
	storage_it it = m_bones_koeff.find(bone_id);
	if( it != m_bones_koeff.end() )
		return it->second.armour * it->second.condition;
	else
		return m_default.armour * m_default.condition;
}

BOOL SBoneProtections::getBonePassBullet(s16 bone_id)
{
	storage_it it = m_bones_koeff.find(bone_id);
	if( it != m_bones_koeff.end() )
		return it->second.BonePassBullet || it->second.condition <= 0.01;
	else
		return m_default.BonePassBullet || m_default.condition <= 0.01; 
}

SBoneProtections::storage_type SBoneProtections::getBoneSet(u32 selection) const
{
	storage_type result;	
	for (auto it = m_bones_koeff.begin(); it != m_bones_koeff.end(); ++it)
	{
		const BoneProtection	&BP = it->second;
		if (  (BP.flags & selection) > 0)
			result.insert(mk_pair(it->first, BP));
	}
	return result;
}

void SBoneProtections::reload(const shared_str& bone_sect, CKinematics* kinematics)
{
	VERIFY(kinematics);

	float save_conditions[256] = { 1.0f };
	for (int i = 0; i < 256; i++) save_conditions[i] = 1.f;

	// чтобы не затереть состояние защиты (используется в CCustomOutfit)
	for (auto it = m_bones_koeff.begin(); it != m_bones_koeff.end(); it++)	
		save_conditions[it->first] = it->second.condition;
	

	m_bones_koeff.clear();

	m_fHitFrac = READ_IF_EXISTS(pSettings, r_float, bone_sect, "hit_fraction",	0.1f);

	m_default.importance = 0.01f;
	m_default.koeff		= 1.0f;
	m_default.armour	= 0.0f;
	m_default.condition = 1.0f;
	m_default.BonePassBullet = FALSE;

	float saldo_imp = 0.f;


	CInifile::Sect	&protections = pSettings->r_section(bone_sect);
	for (CInifile::SectCIt i=protections.Data.begin(); protections.Data.end() != i; ++i) {
		string256 buffer;
		const shared_str& key = i->first;
		LPCSTR sz_key = *key;
		LPCSTR line = *(*i).second;
		int count = _GetItemCount(line);

		float Koeff = (float)atof( _GetItem(line, 0, buffer) );
		float Armour = (float)atof( _GetItem(line, 1, buffer) );
		BOOL BonePassBullet = (BOOL) (atoi( _GetItem(line, 2, buffer) ) > 0.5f);
		
		BoneProtection	BP;		
		BoneProtection	def_BP = GetDefaultConfig(key);
		BP.condition  = 1.0f; // full condition

		BP.koeff = Koeff;
		BP.armour = Armour;		
		BP.BonePassBullet = BonePassBullet;
		BP.importance = def_BP.importance;
		BP.flags = def_BP.flags;

		if (count > 3)
			BP.importance = (float)atof(_GetItem(line, 3, buffer));		


		

		if (!xr_strcmp(sz_key,"default"))
		{
			m_default = BP;
		}
		else 
		{
			if (!xr_strcmp(sz_key,"hit_fraction")) continue;
			s16	bone_id				= kinematics->LL_BoneID(key);
			R_ASSERT2				(BI_NONE != bone_id, sz_key);						
			BP.condition = save_conditions[bone_id];		
			m_bones_koeff.insert (mk_pair(bone_id,BP));
			saldo_imp += BP.importance;
		}
	}

	if (saldo_imp >= 0 && saldo_imp != 1.0f)
	{
		float coef = 1.f / saldo_imp;
		for (auto it = m_bones_koeff.begin(); it != m_bones_koeff.end(); it++)
		{
			BoneProtection &BP = it->second;
			BP.importance *= coef;    // коррекция веса, к общей единица
			BP.condition = save_conditions[it->first];
		}

	}

}
