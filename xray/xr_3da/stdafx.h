#ifndef	STDAFX_3DA
#define STDAFX_3DA

#pragma once

#ifdef _EDITOR
	#include "..\editors\ECore\stdafx.h"
#else

#include "../xrCore/xrCore.h"

#ifdef _DEBUG
	#define D3D_DEBUG_INFO
#endif
#include "api_defines.h"

// Our headers
#include "engine.h"
#include "defines.h"
#ifndef NO_XRLOG
#include "../xrCore/log.h"
#endif
#include "device.h"


#include "xrXRC.h"

#include "../xrSound/sound.h"

extern ENGINE_API CInifile *pGameIni;

#pragma comment( lib, "xrCore.lib"	)
#pragma comment( lib, "xrCDB.lib"	)
#pragma comment( lib, "xrSound.lib"	)
#pragma comment( lib, "lua5.1.lib"	)
#pragma comment( lib, "luabind.lib"	)

#pragma comment( lib, "winmm.lib"		)

#pragma comment( lib, "d3d9.lib"		)
#pragma comment( lib, "dinput8.lib"		)
#pragma comment( lib, "dxguid.lib"		)

#ifndef DEBUG
#define LUABIND_NO_ERROR_CHECKING2
#endif




#if	!defined(DEBUG) || defined(FORCE_NO_EXCEPTIONS)
	// release: no error checking, no exceptions
	#define LUABIND_NO_EXCEPTIONS
	#define BOOST_THROW_EXCEPTION_HPP_INCLUDED
	namespace std	{	class exception; }
	namespace boost {	ENGINE_API	void throw_exception(const std::exception &A);	};
#endif
#define LUABIND_DONT_COPY_STRINGS

#endif // !M_BORLAND
#endif // !defined STDAFX_3DA
