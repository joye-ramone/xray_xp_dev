////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_storage_manager_inline.h
//	Created 	: 25.12.2002
//  Modified 	: 23.10.2014
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator storage manager inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once

IC	CALifeStorageManager::CALifeStorageManager			(xrServer *server, LPCSTR section) :
	inherited	(server,section)
{
	m_section				= section;
	strcpy					(m_save_name,"");
	strcpy					(m_loaded_save,"");
}


IC		LPCSTR	CALifeStorageManager::save_name			(BOOL bLoaded) const
{ 
	if (!xr_strlen(m_loaded_save)) 
		 bLoaded = FALSE;
	return bLoaded ? m_loaded_save : m_save_name; 
}
