#ifndef _DOLPHIN_OS_OSUTIL_H
#define _DOLPHIN_OS_OSUTIL_H

#include "types.h"

BEGIN_SCOPE_EXTERN_C

///// USEFUL MACROS/DEFINES //////

// Defines for cached and uncached memory.
#define OS_BASE_CACHED   (0x80000000)
#define OS_BASE_UNCACHED (0xC0000000)

//////////////////////////////////

// Macros for rounding to 32-alignment.
//
// Applied to both sizes and pointers, so the intermediate type has to be wide
// enough to hold an address. u32 was on the console; off it, rounding a pointer
// would truncate. sptr is pointer-sized on any host and 32-bit under the
// PowerPC EABI. MetroWerks keeps the original spelling so the matching build is
// byte-for-byte unaffected.
#if defined(__MWERKS__)
#define OSRoundUp32B(x)   (((u32)(x) + 0x1F) & ~(0x1F))
#define OSRoundDown32B(x) (((u32)(x)) & ~(0x1F))
#else
#define OSRoundUp32B(x)   (((sptr)(x) + 0x1F) & ~((sptr)0x1F))
#define OSRoundDown32B(x) (((sptr)(x)) & ~((sptr)0x1F))
#endif

// Address conversions.
#define OSPhysicalToCached(paddr)    ((void*)((u32)(paddr) + OS_BASE_CACHED))
#define OSPhysicalToUncached(paddr)  ((void*)((u32)(paddr) + OS_BASE_UNCACHED))
#define OSCachedToPhysical(caddr)    ((u32)((u8*)(caddr) - OS_BASE_CACHED))
#define OSUncachedToPhysical(ucaddr) ((u32)((u8*)(ucaddr) - OS_BASE_UNCACHED))
#define OSCachedToUncached(caddr)    ((void*)((u8*)(caddr) + (OS_BASE_UNCACHED - OS_BASE_CACHED)))
#define OSUncachedToCached(ucaddr)   ((void*)((u8*)(ucaddr) - (OS_BASE_UNCACHED - OS_BASE_CACHED)))

//////////////////////////////////

END_SCOPE_EXTERN_C

#endif
