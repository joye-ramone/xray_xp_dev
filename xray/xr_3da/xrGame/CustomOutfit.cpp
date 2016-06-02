#include "stdafx.h"

#include "customoutfit.h"
#include "PhysicsShell.h"
#include "inventory_space.h"
#include "Inventory.h"
#include "Actor.h"
#include "game_cl_base.h"
#include "Level.h"
#include "BoneProtections.h"
#include "clsid_game.h"
#include "script_engine.h"
#include "../lua_tools.h"
#include "migration.h"
#include "xrserver_objects_alife_items.h"
#include "string_table.h"

#pragma optimize("gyts", off)

CCustomOutfit::CCustomOutfit()
{
	SetSlot (OUTFIT_SLOT);

	m_flags.set(FUsingCondition, TRUE);

	m_HitTypeProtection.resize(ALife::eHitTypeMax);
	for(int i=0; i<ALife::eHitTypeMax; i++)
		m_HitTypeProtection[i] = 1.0f;

	m_boneProtection = xr_new<SBoneProtections>();
	m_bAlwaysProcessing = TRUE;
	m_NightVisionDevice = NULL;
	m_dwPacketTag		= 0;
	m_wPacketVersion	= 0;
	m_wImportVersion	= 0;
	m_fImmunityCoef		= 1.f;
	m_fProtectionCoef	= 1.f;
}

CCustomOutfit::~CCustomOutfit() 
{
	xr_delete(m_boneProtection);
	xr_delete(m_NightVisionDevice);
}

void CCustomOutfit::net_Export(NET_Packet& P)
{
	inherited::net_Export	(P);
	P.w_float_q8			(m_fCondition, 0.0f, 1.0f);
	auto obj = alife_object();
	if ( NULL == obj || obj ->m_wVersion < 120 ) return;	// m_wImportVersion < 120  
	// сохранение кондиции отдельно для каждого элемента броника
	auto list = m_boneProtection->m_bones_koeff;
	P.w_u32(MIG_PACKET_120);
	P.w_u8((u8)list.size());
	for (auto it = list.begin(); it != list.end(); it++)
	{
		P.w_u16(it->first);
		P.w_float_q16 (it->second.condition, 0.f, 1.f);
	}	

}

void CCustomOutfit::net_Import(NET_Packet& P)
{
	inherited::net_Import(P);
	P.r_float_q8(m_fCondition, 0.0f, 1.0f);
	if (m_wPacketVersion < 120 || P.r_elapsed() < 5) return;
	u32 tag = P.r_u32();
	if (tag != MIG_PACKET_120)
	{
		P.r_seek(P.r_tell() - 4);
		return;
	}
	m_dwPacketTag = tag;

	m_wImportVersion = m_wPacketVersion;
	// загрузка кондиции отдельно для каждого элемента броника
	auto &list = m_boneProtection->m_bones_koeff;
	int pairs = P.r_u8();
	for (int i = 0; i < pairs; i ++)
	{
		s16 element = P.r_s16();
		auto it = list.find(element);
		if (it != list.end())
		{
			auto &BP = it->second;
			P.r_float_q16(BP.condition, 0.f, 1.f);
			if (BP.condition < 1.f)
				__asm nop;
		}
	}	

}

BOOL CCustomOutfit::net_Spawn(CSE_Abstract* DC)
{
	inherited::net_Spawn(DC);
	CSE_ALifeItemCustomOutfit		*outfit	= smart_cast<CSE_ALifeItemCustomOutfit*>(DC);
	if (!outfit) return FALSE;

	m_wPacketVersion = outfit->m_wVersion;

	auto &list = m_boneProtection->m_bones_koeff;

	const auto src_list = outfit->m_detailed_condition;

	if (src_list.size() > 0 && m_wPacketVersion >= 120)
		m_wImportVersion = m_wPacketVersion;

	for (auto it = list.begin(); it != list.end(); it++)
	{
		s16 element = it->first;
		auto src = src_list.find ((u16)element);
		if (src != src_list.end())
			it->second.condition = src->second;
	}
	return TRUE;
}

void CCustomOutfit::Load(LPCSTR section) 
{
	m_fProtectionCoef								= READ_IF_EXISTS(pSettings, r_float, section, "protection_coef", 1.0f);
	m_fImmunityCoef									= READ_IF_EXISTS(pSettings, r_float, section, "immunity_coef", 1.0f);

	inherited::Load(section);
	LPCSTR protection_names[] =
				{ "burn_protection", "strike_protection", "shock_protection", "wound_protection",
				  "radiation_protection", "telepatic_protection", "chemical_burn_protection",
				  "explosion_protection", "fire_wound_protection", "wound2_protection",
				  "physic_strike_protection", "pregnancy_protection" };

	for (int i = ALife::eHitTypeBurn; i < ALife::eHitTypeMax; i++)
	{
		LPCSTR name = protection_names[i];
		float coef = m_fProtectionCoef;
		if (strstr(name, "telepatic") || strstr(name, "chemical") || strstr(name, "radiation"))	coef = 1.0f;	
		float value = READ_IF_EXISTS(pSettings, r_float, section, name, 0.1f) * coef;
		if (coef > 1.f)
			clamp<float> (value, 0.f, 0.95f);  // абсолютной защиты не бывает в реальности
		m_HitTypeProtection[i] = value;
	}		

	if (pSettings->line_exist(section, "actor_visual"))
		m_ActorVisual = pSettings->r_string(section, "actor_visual");
	else
		m_ActorVisual = NULL;

	m_ef_equipment_type		= pSettings->r_u32(section,"ef_equipment_type");
	if (pSettings->line_exist(section, "power_loss"))
		m_fPowerLoss = pSettings->r_float(section, "power_loss");
	else
		m_fPowerLoss = 1.0f;	

	m_additional_weight				= pSettings->r_float(section,"additional_inventory_weight");
	m_additional_weight2			= pSettings->r_float(section,"additional_inventory_weight2");

	LPCSTR nvd_sect = NULL;
	if (pSettings->line_exist(section, "night_vision_device"))
		nvd_sect = pSettings->r_string(section, "night_vision_device");

	if (nvd_sect)
	{
		if (xr_strlen(nvd_sect) && pSettings->section_exist(nvd_sect))
		{
			m_NightVisionDevice = xr_new<CNightVisionDevice>();
			m_NightVisionDevice->LoadConfig(nvd_sect);
		}
		else
			Msg("!#ERROR: invalid night_vision_device '%s' for outfit '%s' ", nvd_sect, section);

	}	

	m_full_icon_name								= pSettings->r_string(section,"full_icon_name");

	CActor  *pActor = Actor();
	if(pActor && pSettings->section_exist("actor_damage"))
		m_boneProtection->reload("actor_damage", smart_cast<CKinematics*>(pActor->Visual()));				
}

float CCustomOutfit::GetConditionEx(u32 selection) const
{
	auto list = m_boneProtection->getBoneSet(selection);
	float coef = 0;
	float result = 0.f;
	for (auto it = list.begin(); it != list.end(); it++)
	 	coef += it->second.importance; // суммарная важность выбранных элементов

	if (coef > 0)
	for (auto it = list.begin(); it != list.end(); it++)
	{
		const auto &BP = it->second;
		result += BP.condition * BP.importance / coef;
	}

	return result;
}

s16	CCustomOutfit::GetRandomElement(ALife::EHitType hit_type, const SBoneProtections::storage_type &list)
{
	if (0 == list.size()) return -1;
	// LPCSTR from = "nobody";
	// if (who) from = who->cNameSect().c_str();


	u64	clk = CPU::GetCLK();
	CRandom rnd;
	rnd.seed((u32)clk);
	

	if (1)
	{   // активируется схема случайного хита
		// TODO: в зависимости от ранга и рост who надо выбирать кости по уязвимости (шея или жопаs)
		int i = rnd.randI(0, list.size());
		auto it = list.begin();
		while (i-- > 0 && it != list.end()) it++;
		if (it != list.end())
			return it->first;

	}
	return -1;

}

void CCustomOutfit::HitSpread(float hit_power)
{   // распределение хита равномерно по костям (!)
	auto &list = m_boneProtection->m_bones_koeff;
	for (auto it = list.begin(); it != list.end(); ++it)
	if (it->second.importance > 0)
	{	
		it->second.condition -= hit_power;
		list[it->first].condition = it->second.condition;
	}
}


void CCustomOutfit::Hit(SHit* pHDS)
{
	m_bActorOutfit = (Actor()->GetOutfit() == this);

	float hit_power = pHDS->power;
	ALife::EHitType hit_type = pHDS->hit_type;
	s16 element = pHDS->boneID;
	if (hit_power <= 0) return;

	bool  actor_outfit = (Actor()->GetOutfit() == this);

	float self_imm = immunities()[hit_type]; // защита самого костюма
	hit_power *= self_imm * m_fImmunityCoef;

	if (hit_power <= 0.00001) return;  // ничтожные хиты считать не стоит.

	float new_condition = 0.f;
	CLASS_ID clsid = 0;

	u32 selection = 0xFF;
	LPCSTR from = "nobody";
	if (pHDS->who)
	{
		from = pHDS->who->Name_script();
		clsid = pHDS->who->CLS_ID;	
	}

	lua_State *L = AuxLua(); // независимое выполнение на всякий случай
	if (L && element < 0 && m_bActorOutfit)
	{		 
		int top = lua_gettop(L);
		int errf = 0;
		lua_getglobal(L, "AtPanicHandler");
		if (lua_iscfunction(L, -1))
			errf = lua_gettop(L);
		else
			lua_pop(L, 1);

		lua_getglobal	(L, "actor_hit_target");
		MsgCB("$#CONTEXT: actor_hit_target type = %s ", lua_typename(L, -1));
		if (lua_isfunction(L, -1))
		{
			lua_pushinteger (L, ID());
			lua_pushinteger (L, pHDS->whoID);		
			lua_pushinteger (L, hit_type);
			lua_pushnumber  (L, hit_power);		
			if (0 == lua_pcall(L, 4, LUA_MULTRET, errf))
				selection = lua_tointeger(L, -1);
		}
		else
			Log("!#ERROR: global script callback not defined 'actor_hit_target', hit will be send to random element.");

		lua_settop(L, top);
	}


	auto list = m_boneProtection->getBoneSet(selection);

	if (element < 0) element = GetRandomElement(hit_type, list);
	pHDS->boneID = element;
	
	int bones = 0;
 	auto &full_list = m_boneProtection->m_bones_koeff;

	auto it = full_list.find(element);

	if (it != full_list.end())
	{
		auto &BP = full_list[element];		
		float hp = hit_power;
		if (BP.importance > 0.01)
			hp /= BP.importance;		// вместо всего костюма страдает один элемент!

		float cond = BP.condition - hp; // повреждение определенного элемента брони
		clamp <float>(cond, 0.0, 1.0f);		
		BP.condition = cond;
		if (actor_outfit)
			MsgV("5HIT", "# outfit hit received, bone = %d, hit_power = %.5f, real hit = %.4f, condition = %.5f ", element, hit_power, hp, cond);		
	}
	else
		HitSpread(hit_power);

	string1024 buff = { 0 };

	for (it = full_list.begin(); it != full_list.end(); ++it)
	if (it->second.importance > 0)
	{
		int bone = it->first;
		const auto &BP = it->second;		
		float cond = BP.condition * BP.importance;
		
		bones++;
		new_condition += cond;
		string32 tmp;
		if (BP.importance > 0.001)
		{
		   if (cond > 0) 
			   sprintf_s(tmp, 32, " %.3f", cond);
		   else
			   sprintf_s(tmp, 32, " [%d]", bone);
		}
		//	strcpy_s(tmp, 32, " [0]");
		strcat_s(buff, 1024, tmp);
	}


	if (actor_outfit)
		MsgV("5HIT", "# outfit condition recalculated from %.5f to %.5f:\n\t%s\n\t damage from = %s ", m_fCondition, new_condition, buff, from);

	clamp<float> ( new_condition, 0.0f, 1.0f );

	if (bones)
	{
		m_fCondition = new_condition;
		ChangeCondition(0);
	}
	else	
		ChangeCondition(-hit_power); // отсутствует защита в костях
		
}


float CCustomOutfit::GetDefHitTypeProtection(ALife::EHitType hit_type) // коэффициент умножения хита
{
	float cond = GetCondition();
	float result = 1.0f - m_HitTypeProtection[hit_type] * cond;
	return result > 0.001f ? result : 0.001f;
}

float CCustomOutfit::GetHitTypeProtection(SHit *pHDS)
{
	ALife::EHitType hit_type = pHDS->hit_type;
	s16 element = pHDS->boneID;

	float fBase = m_HitTypeProtection[hit_type] * GetCondition();
	m_bActorOutfit = (Actor()->GetOutfit() == this);

	// if (element < 0) element = GetRandomElement(hit_type, m_boneProtection->m_bones_koeff);


#ifdef NLC_EXTENSIONS 
	float bone = 1.0f;
	if (element >= 0) 
	     bone = m_boneProtection->getBoneProtection(element);	
#else	
	float bone = m_boneProtection->getBoneProtection(element);	
#endif
	float result = 1.0f - fBase * bone;
	if (m_bActorOutfit)
		MsgV("5HIT", "# element = %2d, base = %.3f, bone = %.3f,  hit ratio = %.3f ", element, fBase, bone, result);

	// pHDS->boneID = element;
	// должна вернуть коэффициент от 0 до 1 (от макс. до минимальной защиты)
	return result > 0 ? result : 0;
}

float	CCustomOutfit::HitThruArmour(SHit *pHDS)
{
	ALife::EHitType hit_type = pHDS->hit_type;
	s16 element = pHDS->boneID;
	float hit_power = pHDS->power;

	/*if (element < 0)
	{
		element = GetRandomElement(ALife::eHitTypeFireWound, m_boneProtection->m_bones_koeff);
		pHDS->boneID = element;
	}*/


#ifdef NLC_EXTENSIONS 
	hit_power *= GetDefHitTypeProtection(ALife::eHitTypeFireWound);
#endif
	float BoneArmour = m_boneProtection->getBoneArmour(element) * ( 1 - pHDS->ap );	
	float NewHitPower = hit_power - BoneArmour;
	
	if (NewHitPower < hit_power * m_boneProtection->m_fHitFrac) 
		NewHitPower = hit_power * m_boneProtection->m_fHitFrac;

	if (Actor()->GetOutfit() == this)
		MsgV("5HIT", "# hit_power = %7.3f, element = %2d, BoneArmor = %.3f, result = %.3f ", hit_power, element, BoneArmour, NewHitPower);
	return NewHitPower > 0 ? NewHitPower : 0;
};

extern void str_replace(std::string &s, LPCSTR what, LPCSTR alt);

LPCSTR format_protection(LPCSTR name, float htp)
{
	if (NULL == name || htp <= 0) return NULL;
	static string64 temp;	
	clamp<float>(htp, 0.f, 0.9999f);
	// alpet: 0.1 = 1.1(1) 0.5 = 2, 0.7 = 3.3(3)
	// orig: какой процент хита пропускается до тушки носителя
	/*
	htp = 1.f / (1.f - htp); 
	float prot = 100.f * ( htp - 1.f );
	*/
	float prot = htp * 100.f;
	LPCSTR _color = prot > 0 ? "$cl_green" : "$cl_yellow";
	sprintf_s(temp, 512, "$cl_aqua%s: %s +%.0f%%\\n", *CStringTable().translate(name), _color, prot);
	if (strstr(temp, "strike")) // нет перевода
		return NULL; 
	else
		return temp;
}



extern LPCSTR _imm_st_names[];


shared_str CCustomOutfit::ItemDescription() const
{
	CStringTable st;
	std::string result = *m_Description;
	LPCSTR source = *m_Description;
	string512 temp;
	if (strstr (source, "$outfit_general_info"))
	{
		std::string info;
		float cond = GetCondition();
		LPCSTR line = "";
		for (int hit_type = ALife::eHitTypeBurn; hit_type < ALife::eHitTypeMax; hit_type++)
		{
			line = format_protection(_imm_st_names[hit_type], m_HitTypeProtection[hit_type] * cond);
			if (line)
				info += line;
		}
		
		LPCSTR  _coef_name = *st.translate("st_effective_strength");
		if (m_fImmunityCoef != 1.f)
		{
			float strength = m_fImmunityCoef;   // чем ниже этот коэффициент, тем сильнее защищается костюм от износа
			clamp<float>(strength, 0.01f, 10.f);
			strength = 100.f * (1.f / strength - 1.f); // 0.25 =>> 3.0 =>> 300%, 2.f =>> -0.5f =>> -50%
			sprintf_s(temp, sizeof(temp), "$cl_fuchsia%s%s %3.0f%%\\n", _coef_name, (strength > 0.f) ? "$cl_green" : "$cl_red", strength);
			info += temp;
		}
		
		if (!strstr(source, "$carrying") && m_additional_weight > 0)
		{
			sprintf_s(temp, 512, "$cl_yellow%s: +%0.f Kg\\n", *CStringTable().translate("st_carrying"), m_additional_weight);
			info += temp;
		}


		info += "%c[default]\n";
		// 0.1 = 90% хита +10%
		// 0.5 = 50% хита +100%
		// 0.9 = 10% хита +90%
		// 1 = абсолютная защита

		str_replace(result, "$outfit_general_info", info.c_str());
	}


	// $cl_aquaУдар:$cl_green +5%\n$cl_aquaРазрыв:$cl_green +5%\n$cl_aquaВзрыв:$cl_green +10%\n$cl_aquaПулестойкость:$cl_green +11%\n$cl_aquaОжог:$cl_green +10%\n$cl_aquaХим.ожог:$cl_green +10%\n$cl_aquaЭлектрошок:$cl_green +5%\n$cl_aquaГрузоподъёмность:$cl_green +$carrying кг\n%c[default]
	
	sprintf_s(temp, 64, "%.1f", m_additional_weight);

	str_replace(result, "$carrying", temp);	
	str_replace(result, "$cl_aqua",		 "%c[255,1,255,255]");	
	str_replace(result, "$cl_blue",		 "%c[255,1,1,255]");
	str_replace(result, "$cl_fuchsia",	 "%c[255,255,1,255]");
	str_replace(result, "$cl_green",	 "%c[255,1,255,1]");		
	str_replace(result, "$cl_red",		 "%c[255,255,1,1]");
	str_replace(result, "$cl_yellow",	 "%c[255,255,255,1]");

	return shared_str (result.c_str());
}

BOOL	CCustomOutfit::BonePassBullet					(int boneID)
{
	return m_boneProtection->getBonePassBullet(s16(boneID));
};

// #include "torch.h"
void	CCustomOutfit::OnMoveToSlot		()
{
	if (m_pCurrentInventory)
	{
		CActor* pActor = smart_cast<CActor*> (m_pCurrentInventory->GetOwner());
		if (pActor)
		{
			if (m_ActorVisual.size())
			{
				shared_str NewVisual = NULL;
				char* TeamSection = Game().getTeamSection(pActor->g_Team());
				if (TeamSection)
				{
					if (pSettings->line_exist(TeamSection, *cNameSect()))
					{
						NewVisual = pSettings->r_string(TeamSection, *cNameSect());
						string256 SkinName;
						strcpy_s(SkinName, pSettings->r_string("mp_skins_path", "skin_path"));
						strcat_s(SkinName, *NewVisual);
						strcat_s(SkinName, ".ogf");
						NewVisual._set(SkinName);
					}
				}
				
				if (!NewVisual.size())
					NewVisual = m_ActorVisual;

				pActor->ChangeVisual(NewVisual);
			}			

			if (pSettings->line_exist(cNameSect(), "bones_koeff_protection"))
				m_boneProtection->reload( pSettings->r_string(cNameSect(),"bones_koeff_protection"), smart_cast<CKinematics*>(pActor->Visual()));
		}
	}
	ChangeCondition(0);
};

void	CCustomOutfit::OnMoveToRuck		()
{	
	if (m_pCurrentInventory)
	{
		CActor* pActor = smart_cast<CActor*> (m_pCurrentInventory->GetOwner());
		if (pActor)
		{
			if (m_NightVisionDevice)
				m_NightVisionDevice->SwitchNightVision(false);

			if (m_ActorVisual.size())
			{
				shared_str DefVisual = pActor->GetDefaultVisualOutfit();
				if (DefVisual.size())
				{
					pActor->ChangeVisual(DefVisual);
				};
			}
			if(pActor && pSettings->line_exist(cNameSect(), "bones_koeff_protection"))
				m_boneProtection->reload( pSettings->r_string(cNameSect(),"bones_koeff_protection"), smart_cast<CKinematics*>(pActor->Visual()));
		}
	}
};


void	CCustomOutfit::UpdateCL()
{
	inherited::UpdateCL();
	if (H_Parent() && H_Parent()->ID() == Actor()->ID() && m_NightVisionDevice)
		m_NightVisionDevice->UpdateSwitchNightVision();
}
u32	CCustomOutfit::ef_equipment_type	() const
{
	return		(m_ef_equipment_type);
}

float CCustomOutfit::GetPowerLoss() 
{
	if (m_fPowerLoss < 1.0f && GetCondition() <= 0.35 )	
		return 1.0f;				
	return m_fPowerLoss;
};	

void CCustomOutfit::SetCondition(float fNewCondition) // TODO: лечит/калечит элементы защиты в порядке перебора костей!
{
	auto &list = m_boneProtection->m_bones_koeff;

	if (fNewCondition >= 1.0f)
	{		
		for (auto it = list.begin(); it != list.end(); it++)		
			it->second.condition = fNewCondition;	
	}
	else
	{
		float change = fNewCondition - GetCondition(); // если отрицательное значение, то убавлять
		MsgV("5HIT", "@#DBG: COutfit::SetCondition change = %.5f ", change);

		while ( abs(change) >= 0.00001f )
		for (auto it = list.begin(); it != list.end(); it++)
		{
			auto &BP = it->second;
			if (BP.importance < 0.01f) continue;
			float delta = change / BP.importance; // поглощение доли хита			
			delta = delta * Random.randF() * 0.5f;
			// ограничения воздействия на элемент
			if (delta > 0 && delta + BP.condition > 0) delta = 1.0f - BP.condition;
			if (delta < 0 && delta + BP.condition < 0) delta = -BP.condition;		// забрать кондицию полностью
			BP.condition += delta;
			MsgV("5HIT", "@  bone = %2d, delta = %.5f, condition = %.3f, change -= %.5f ", (int)it->first, delta, BP.condition, delta * BP.importance);
			// Учет поглощение части хита отдельным элементом
			delta *= BP.importance;

			if (abs(delta) < abs(change))
				change -= delta;
			else
				change = 0.f;
			if (abs(change) < 0.00001f) break;
		}

	}

	ChangeCondition(0);
}