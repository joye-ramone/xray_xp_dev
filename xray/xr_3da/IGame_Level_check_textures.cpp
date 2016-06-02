#include "stdafx.h"
#include "resourcemanager.h"
#include "igame_level.h"

void IGame_Level::LL_CheckTextures()
{
	u32	m_base,c_base,m_lmaps,c_lmaps;
	Msg("##PERF: IGame_Level::LL_CheckTextures() ");
	Device.Resources->_GetMemoryUsage		(m_base,c_base,m_lmaps,c_lmaps);

	Msg	("* t-report - base: %d, %d K",	c_base,		m_base/1024);
	Msg	("* t-report - lmap: %d, %d K",	c_lmaps,	m_lmaps/1024);
	BOOL	bError	= FALSE;
	if (m_base > 1024 * 1024 * 1024 || c_base > 2000)
	{
		LPCSTR msg	= "Too many base-textures (limit: 2000 textures or 1024M).\n        Reduce number of textures (better) or their resolution (worse).";
		Msg		("***WARNING***: %s",msg);
		bError		= TRUE;
	}
	if (m_lmaps > 32 * 1024 * 1024 || c_lmaps > 16)
	{
		LPCSTR msg	= "Too many lmap-textures (limit: 16 textures or 32M).\n        Reduce pixel density (worse) or use more vertex lighting (better).";
		Msg			("***WARNING***: %s",msg);
		bError		= TRUE;
	}
}
