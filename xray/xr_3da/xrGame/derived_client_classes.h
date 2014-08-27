////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 16.08.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////
#include "script_export_space.h"
#include "CustomOutfit.h"
// alpet : в этом файле при добавлении экспортеров с зависимост€ми наследовани€, необходимо соблюдать пор€док - сначала экспортируютс€ базовые классы
// NOTE  : требуетс€ именно класс вместо структуры, чтобы объ€вить его френдом 

class CInventoryScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CInventoryScript)
#undef script_type_list
#define script_type_list save_type_list(CInventoryScript)



class	CEatableItemScript 
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEatableItemScript)
#undef script_type_list
#define script_type_list save_type_list(CEatableItemScript)


class	CEntityScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEntityScript)
#undef script_type_list
#define script_type_list save_type_list(CEntityScript)

class	CMonsterScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CMonsterScript)
#undef script_type_list
#define script_type_list save_type_list(CMonsterScript)


class   COutfitScript
{
protected:
	template <typename T> 
	static	float get_protection(CCustomOutfit *O)
	{	
		u32 idx = sizeof(T) - 1;
		return O->m_HitTypeProtection[idx]; 
	}

	template <typename T> 
	static	void set_protection(CCustomOutfit *O, float value)
	{	
		u32 idx = sizeof(T) - 1;
		O->m_HitTypeProtection[idx] = value; 
	}


	DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(COutfitScript)
#undef script_type_list
#define script_type_list save_type_list(COutfitScript)


class	CWeaponScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponScript)
#undef script_type_list
#define script_type_list save_type_list(CWeaponScript)

