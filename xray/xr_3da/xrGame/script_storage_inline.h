////////////////////////////////////////////////////////////////////////////
//	Module 		: script_storage_inline.h
//	Created 	: 01.04.2004
//  Modified 	: 17.02.2016
//	Author		: Dmitriy Iassenev
//	Description : XRay Script Storage inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef LUA_STACK_LEAK_FIND
IC lua_State *CScriptStorage::game_lua					()
{
	return				(m_virtual_machine);
}
#endif

IC	void CScriptStorage::current_thread				(CScriptThread *thread)
{
	VERIFY					((thread && !m_current_thread) || !thread);
	m_current_thread		= thread;
}

IC	CScriptThread *CScriptStorage::current_thread	() const
{
	return					(m_current_thread);
}
