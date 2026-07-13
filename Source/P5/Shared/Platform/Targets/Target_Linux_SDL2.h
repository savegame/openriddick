
/*���������������������������������������������������������������������������������������������*\
	File:			Target header for the GLES3 + SDL2 port.

	Contents:		Compile settings for Linux (x86_64, ARM/aarch64) and Android.
					Modeled after Target_PS3.h / the (removed) Win64 target.
\*_____________________________________________________________________________________________*/

#ifndef __INC_TARGET_LINUX_SDL2
#define __INC_TARGET_LINUX_SDL2

#ifndef	TARGET_LINUX_SDL2
#define TARGET_LINUX_SDL2
#endif

// -------------------------------------------------------------------
//  COMPILE SETTINGS
// -------------------------------------------------------------------
#define CPU_LITTLEENDIAN 0x0001

#if defined(__x86_64__)
	#define CPU_X86_64
	#define CPU_SSE
	#define CPU_SSE2
	#include <emmintrin.h>
	#include <pmmintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
	#define CPU_ARM
	#define CPU_VEC128EMU
	#include <arm_neon.h>
#else
	#define CPU_VEC128EMU
#endif

#define PLATFORM_LINUX
#define PLATFORM_SDL2
#ifdef __ANDROID__
	#define PLATFORM_ANDROID
#endif

#define COMPILER_GNU					// gcc / clang
#define COMPILER_GNU_3
#define COMPILER_CPP_EXCEPTIONS			// C++ exception handling is supported
#define COMPILER_NEEDOPERATORDELETE
#define __PLACEMENT_NEW_INLINE

#define CPU_INT32						// An 'int' is 32 bits
#if defined(__LP64__) || defined(_LP64)
	#define CPU_PTR64
#else
	#define CPU_PTR32
#endif

#define CPU_SUPPORT_FP64				// CPU supports double

#define	NO_INLINE_FILESYSTEM

#define	UNICODE_WORKAROUND

// -------------------------------------------------------------------
//  DEBUG COMPILER SETTINGS
// -------------------------------------------------------------------
#ifndef M_RTM
#define DEBUG_LOGFILEENBLE
#define DEBUG_ERRORFILEENBLE
#endif

#define	dllvirtual

#define M_FAKEDYNAMICCAST
#define M_EXCEPTIONS 0
#define M_FILEERROR M_BREAKPOINT

#define IMAGE_IO_NOJPG
#define IMAGE_IO_NOPCX
#define IMAGE_IO_NOGIF
#define	IMAGE_IO_NOS3TC
#define IMAGE_IO_NOVORBIS
#define SOUND_IO_NOCOMPRESS
#define	SOUND_IO_NODECOMPRESS
#define IMAGE_IO_PNG

#define M_DISABLE_CURRENTPROJECT
#define M_DISABLE_TODELETE
#define M_DISABLE_CWOBJECT_TRIGGER_TELEPORT
#define M_DISABLE_CWOBJECT_FUNC_LOD
#define M_DISABLE_CWOBJECT_FUNC_MIRROR
#define M_DISABLE_CWOBJECT_FUNC_PORTAL
#define M_DISABLE_CWOBJECT_SYSTEM

#define M_STATICINIT
#define M_STATIC
#define M_STATIC_RENDERER				// Renderer (GLES3) is linked statically

#define M_ARGLISTCALL
#define M_INLINE	inline
#define M_CDECL
#define M_STDCALL
#define M_FUNCTION __func__
#define M_BREAKPOINT __builtin_trap()
#define M_FORCEINLINE inline __attribute__((always_inline))
#define M_NOINLINE __attribute__((noinline))

#define M_RESTRICT __restrict__
#define M_ALIGN(_Align) __attribute__((aligned(_Align)))

#define M_ATOMICALIGNMENT 64

#define M_IMPORTBARRIER __sync_synchronize();
#define M_EXPORTBARRIER __sync_synchronize();
#define M_THREADSPINCOUNT 400

#define MRTC_THREADLOCAL __thread
#define M_PREZERO128(a, b) __builtin_prefetch((const char*)(a) + (b), 1)
#define M_ZERO128(_x, _y) memset((char*)(_x) + (_y), 0, 128)
#define M_PRECACHE128(_x, _y) __builtin_prefetch((const char*)(_x) + (_y))

#define M_TRY
#define M_CATCH(_ToCatch)

#define M_OFFSET(_Class, _Member, _Dest) aint _Dest;\
			{\
				const _Class *pPtr = 0;\
				_Dest = (aint)(void *)(&((pPtr)->_Member));\
			}
#ifndef _MAX_PATH
#define _MAX_PATH 256
#endif

/*************************************************************************************************\
|��������������������������������������������������������������������������������������������������
| STANDARD TYPES
|__________________________________________________________________________________________________
\*************************************************************************************************/

typedef signed char				int8;
typedef unsigned char			uint8;
typedef signed short			int16;
typedef unsigned short			uint16;
typedef signed int				int32;
typedef unsigned int			uint32;
typedef float					fp32;
typedef double					fp64;
typedef int						bint;

#if defined(__x86_64__)
	typedef __m128				vec128;
#elif defined(__aarch64__) || defined(__ARM_NEON)
	typedef float32x4_t			vec128;
#else
	typedef struct { fp32 k[4]; } M_ALIGN(16) vec128;
#endif

#if defined(__LP64__) || defined(_LP64)
	typedef signed long			int64;
	typedef unsigned long		uint64;
	typedef signed long			aint;
	typedef unsigned long		auint;
	typedef unsigned long		mint;
	typedef	signed long			smint;
#else
	typedef signed long long	int64;
	typedef unsigned long long	uint64;
	typedef signed int			aint;
	typedef unsigned long		auint;
	typedef unsigned int		mint;
	typedef	signed int			smint;
#endif

typedef unsigned int			uint;
typedef int64					fint;

typedef uint16					wchar;
typedef char ch8;

#ifndef NULL
#define NULL 0
#endif

#endif	// __INC_TARGET_LINUX_SDL2
