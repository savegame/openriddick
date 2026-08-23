
/*���������������������������������������������������������������������������������������������*\
	File:			Target header for the GLES3 + SDL2 port.

	Contents:		Compile settings for Linux (x86_64, ARM/aarch64) and Android.
					Modeled after Target_PS3.h / the (removed) Win64 target.
\*_____________________________________________________________________________________________*/

#ifndef __INC_TARGET_LINUX_SDL2
#define __INC_TARGET_LINUX_SDL2

#include <string.h>	// memset for M_ZERO128

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
#define __w64						// MSVC 32/64 portability annotation, no-op here

#define M_FAKEDYNAMICCAST

// ИСКЛЮЧЕНИЯ ВКЛЮЧЕНЫ (2026-08-08). Было `0`, унаследовано от Target_PS3.h.
//
// Почему это важнее, чем кажется. В движке ошибка -- это `throw
// CCException`, который наверху ловится и превращается в сообщение: «файла
// нет», «неподдерживаемый класс», «доступ не закрыт» -- всё это штатные,
// переживаемые ситуации. При `M_EXCEPTIONS 0` каждый `Error(...)` и
// `FileError(...)` разворачивается в `M_BREAKPOINT`, то есть `ud2` и SIGILL.
// Любая мелочь, которую ретейл пережил бы строкой в консоли, убивала
// процесс -- и именно это, а не «плохие данные», давало серию падений на
// PS3-наборе: по одному прогону на каждую мелочь.
//
// Что меняется: `M_TRY`/`M_CATCH` перестают быть пустышками и становятся
// настоящими `try`/`catch`, а `Error`/`FileError`/`MemError` начинают
// бросать. Обработчики в коде УЖЕ НАПИСАНЫ (214 блоков `M_TRY`) -- они
// просто никогда не выполнялись.
//
// Откат -- вернуть `0` здесь, ничего больше.
#define M_EXCEPTIONS 1
#define M_FILEERROR M_BREAKPOINT	// используется только при M_EXCEPTIONS 0

#define IMAGE_IO_NOJPG
#define IMAGE_IO_NOPCX
#define IMAGE_IO_NOGIF
#define	IMAGE_IO_NOS3TC
// IMAGE_IO_NOVORBIS removed: the Vorbis wave codec (CMSound_Codec_VORB)
// is required for sound playback (SDL2 sound backend, phase 7 M2)
#define SOUND_IO_NOCOMPRESS
#define	SOUND_IO_NODECOMPRESS
#define IMAGE_IO_PNG
#define LIPSYNC_NOANALYSER				// FaceFX/TalkBack SDKs are not available

#define M_DISABLE_CURRENTPROJECT
#define M_DISABLE_TODELETE
#define M_DISABLE_CWOBJECT_TRIGGER_TELEPORT
#define M_DISABLE_CWOBJECT_FUNC_LOD
#define M_DISABLE_CWOBJECT_FUNC_MIRROR
#define M_DISABLE_CWOBJECT_FUNC_PORTAL
#define M_DISABLE_CWOBJECT_SYSTEM

#define M_STATICINIT
#define M_STATIC
//#define M_STATIC_RENDERER				// Bring-up uses the virtual CRenderContext interface; the GLES3 backend can go static later

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
// Like the PPC dcbz/dcbt originals these take (pointer, offset) in either
// order; the effective address is a + b.
#define M_PREZERO128(a, b) __builtin_prefetch((const void*)((auint)(a) + (auint)(b)), 1)
#define M_ZERO128(_x, _y) memset((void*)((auint)(_x) + (auint)(_y)), 0, 128)
#define M_PRECACHE128(_x, _y) __builtin_prefetch((const void*)((auint)(_x) + (auint)(_y)))

// Форма использования в коде:
//     M_TRY { ... } M_CATCH( catch (CCException) { ... } );
// поэтому `M_TRY` -> `try`, а `M_CATCH(x)` просто раскрывает свой аргумент.
// За одним `M_TRY` бывает НЕСКОЛЬКО `M_CATCH` (иногда под `#ifdef`) -- это
// законно, у одного `try` может быть несколько обработчиков.
#define M_TRY try
#define M_CATCH(_ToCatch) _ToCatch

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
	#define M_SEPARATETYPE_smint		// smint (long) is a distinct type from int32 (int)
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
