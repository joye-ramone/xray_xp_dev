#ifndef XRMEMORY_PURE_H
#define XRMEMORY_PURE_H

#ifdef XRCORE_STATIC
#	define PURE_ALLOC
#else // XRCORE_STATIC
#	ifdef DEBUG
#		define PURE_ALLOC
#	endif // DEBUG
#endif // XRCORE_STATIC

// #define ANGRY_MM
// #define SLOW_MM
#define EXT_MM

#ifdef EXT_MM
typedef void* (WINAPI *MEM_ALLOC_CALLBACK)		(size_t cb);
typedef void* (WINAPI *MEM_REALLOC_CALLBACK)	(void *_ptr, size_t cb);
typedef void  (WINAPI *MEM_FREE_CALLBACK)		(void *_ptr);
#endif

#endif // XRMEMORY_PURE_H