////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 16.08.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////
#include "script_export_space.h"

// alpet : в этом файле при добавлении экспортеров с зависимост€ми наследовани€, необходимо соблюдать пор€док - сначала экспортируютс€ базовые классы
typedef class_exporter<CInventoryItem>	CInventoryItemScript;
add_to_type_list(CInventoryItemScript)
#undef script_type_list
#define script_type_list save_type_list(CInventoryItemScript)



class	CEatableItemScript // требуетс€ именно класс вместо структуры, чтобы объ€вить его френдом 
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEatableItemScript)
#undef script_type_list
#define script_type_list save_type_list(CEatableItemScript)


class	CEntityScript // требуетс€ именно класс вместо структуры, чтобы объ€вить его френдом 
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CEntityScript)
#undef script_type_list
#define script_type_list save_type_list(CEntityScript)


class	CWeaponScript
{
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponScript)
#undef script_type_list
#define script_type_list save_type_list(CWeaponScript)

