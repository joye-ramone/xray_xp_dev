#pragma once

#include "inventory_item_object.h"
#include "nvd.h"
#include "BoneProtections.h"

// struct SBoneProtections;

typedef xr_map<u32, float>  BoneHitProbability;  // вероятность поражения элемента

class CCustomOutfit: public CInventoryItemObject {
	friend class COutfitScript;
	friend class CUIOutfitInfo;
private:
    typedef	CInventoryItemObject inherited;
public:
									CCustomOutfit		(void);
	virtual							~CCustomOutfit		(void);

	virtual void					Load				(LPCSTR section);
	
	// полная версия хита
	virtual void					Hit					(SHit* pHDS);

	virtual	float					GetCondition		() const  { return GetConditionEx(255); };
	virtual float					GetConditionEx		(u32 selection) const; // кондиция по отдельным частям/костям
	//коэффициенты на которые домножается хит
	//при соответствующем типе воздействия
	//если на персонаже надет костюм
	float							GetHitTypeProtection(SHit *pHDS);
	float							GetDefHitTypeProtection(ALife::EHitType hit_type);

	float							HitThruArmour		(SHit *pHDS);
	//коэффициент на который домножается потеря силы
	//если на персонаже надет костюм
	virtual float					GetPowerLoss		();


	virtual void					OnMoveToSlot		();
	virtual void					OnMoveToRuck		();
	virtual void					UpdateCL			();

protected:
	HitImmunity::HitTypeSVec		m_HitTypeProtection;


	WORD							m_wPacketVersion;
	WORD							m_wImportVersion;
	DWORD							m_dwPacketTag;

	shared_str						m_ActorVisual;
	shared_str						m_full_icon_name;
	SBoneProtections*				m_boneProtection;	
	float							m_fImmunityCoef;
	float							m_fProtectionCoef;

	bool							m_bActorOutfit;
	
	void							HitSpread				(float hit_power);
	s16								GetRandomElement		(ALife::EHitType hit_type, const SBoneProtections::storage_type &list);
protected:
	u32								m_ef_equipment_type;
	CNightVisionDevice*				m_NightVisionDevice;
	float							m_fPowerLoss;
	float							m_additional_weight;
	float							m_additional_weight2;

public:
	virtual u32						ef_equipment_type		() const;
	virtual	BOOL					BonePassBullet			(int boneID);
	const shared_str&				GetFullIconName			() const	{return m_full_icon_name;};
	virtual shared_str				ItemDescription		    () const;
	CNightVisionDevice*				NightVisionDevice		() { return m_NightVisionDevice; };
	virtual float					GetAdditionalWeight		(int index = 1) const { return (index == 1 ?  m_additional_weight : m_additional_weight2); };
	virtual void					SetAdditionalWeight		(int index, float value) { if(1 == index) m_additional_weight = value; else m_additional_weight2 = value; };
	virtual float					GetBatteryCharge		()		const { return 0.f; };
	virtual void					SetBatteryCharge		(float value) { };
	virtual void					SetCondition			(float fNewCondition);
	
	virtual BOOL			net_Spawn			(CSE_Abstract* DC);
	virtual void			net_Export			(NET_Packet& P);
	virtual void			net_Import			(NET_Packet& P);
};
