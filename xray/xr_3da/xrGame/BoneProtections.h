#pragma once

struct SBoneProtections{
	struct BoneProtection {
		float		koeff;
		float		armour;
		float		condition;
		float		importance; // значимость кости в общей кондиции
		BOOL		BonePassBullet;
		u32			flags;		// к чему относится элемент
	};
	enum EBodyElements {
		eHeadElement = 1,
		eBodyElement = 2,
		eHandElement = 4,
		eLegElement  = 8,
		eUpperHalf	 = 16,
		eLowerHalf	 = 32		
	};

	float				m_fHitFrac;
	typedef xr_map<s16,BoneProtection>		storage_type;
	typedef storage_type::iterator	storage_it;
						SBoneProtections();								
	BoneProtection		m_default;
	storage_type		m_bones_koeff;
	void				reload				(const shared_str& outfit_section, CKinematics* kinematics);
	float				getBoneProtection	(s16 bone_id);
	float				getBoneArmour		(s16 bone_id);
	BOOL				getBonePassBullet	(s16 bone_id);
	storage_type		getBoneSet			(u32 selection) const;
};
