#pragma once

// you must define ENGINE_BUILD then building the engine itself
// and not define it if you are about to build DLL
#ifndef NO_ENGINE_API
	#ifdef	ENGINE_BUILD
		#define DLL_API			__declspec(dllimport)
		#define ENGINE_API		__declspec(dllexport)
	#else
		#define DLL_API			__declspec(dllexport)
		#define ENGINE_API		__declspec(dllimport)
	#endif
#else
	#define ENGINE_API
	#define DLL_API
#endif // NO_ENGINE_API


#ifdef XRTEXTURES_EXPORTS
	#define XRTEXTURES_API __declspec(dllexport)
#else
	#define XRTEXTURES_API __declspec(dllimport)
#endif

#define ECORE_API
