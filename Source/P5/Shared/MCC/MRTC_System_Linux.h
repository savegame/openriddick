#ifdef PLATFORM_LINUX

/*
	Inline clock and atomic implementations for the Linux/SDL2 port.
	Included from MRTC_System.h next to the other platform headers.

	Clocks are CLOCK_MONOTONIC in nanoseconds; the corresponding
	frequency reported by MRTC_SystemInfo is 1e9.
*/

#include <time.h>

M_INLINE int64 MRTC_SystemInfo::CPU_Clock()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

M_INLINE int64 MRTC_SystemInfo::OS_Clock()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

// Atomic operations return the PREVIOUS value (InterlockedExchangeAdd
// semantics, matching the Win32 and PS3 implementations).

M_INLINE int32 MRTC_SystemInfo::Atomic_Increase(volatile int32 *_pDest)
{
	return __atomic_fetch_add(_pDest, 1, __ATOMIC_SEQ_CST);
}

M_INLINE int32 MRTC_SystemInfo::Atomic_Decrease(volatile int32 *_pDest)
{
	return __atomic_fetch_sub(_pDest, 1, __ATOMIC_SEQ_CST);
}

M_INLINE int32 MRTC_SystemInfo::Atomic_Add(volatile int32 *_pDest, int32 _Add)
{
	return __atomic_fetch_add(_pDest, _Add, __ATOMIC_SEQ_CST);
}

M_INLINE int32 MRTC_SystemInfo::Atomic_Exchange(volatile int32 *_pDest, int32 _SetTo)
{
	return __atomic_exchange_n(_pDest, _SetTo, __ATOMIC_SEQ_CST);
}

M_INLINE int32 MRTC_SystemInfo::Atomic_IfEqualExchange(volatile int32 *_pDest, int32 _CompareTo, int32 _SetTo)
{
	int32 Expected = _CompareTo;
	__atomic_compare_exchange_n(_pDest, &Expected, _SetTo, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return Expected;
}

#ifdef M_SEPARATETYPE_smint

M_INLINE smint MRTC_SystemInfo::Atomic_Increase(volatile smint *_pDest)
{
	return __atomic_fetch_add(_pDest, 1, __ATOMIC_SEQ_CST);
}

M_INLINE smint MRTC_SystemInfo::Atomic_Decrease(volatile smint *_pDest)
{
	return __atomic_fetch_sub(_pDest, 1, __ATOMIC_SEQ_CST);
}

M_INLINE smint MRTC_SystemInfo::Atomic_Add(volatile smint *_pDest, smint _Add)
{
	return __atomic_fetch_add(_pDest, _Add, __ATOMIC_SEQ_CST);
}

M_INLINE smint MRTC_SystemInfo::Atomic_Exchange(volatile smint *_pDest, smint _SetTo)
{
	return __atomic_exchange_n(_pDest, _SetTo, __ATOMIC_SEQ_CST);
}

M_INLINE smint MRTC_SystemInfo::Atomic_IfEqualExchange(volatile smint *_pDest, smint _CompareTo, smint _SetTo)
{
	smint Expected = _CompareTo;
	__atomic_compare_exchange_n(_pDest, &Expected, _SetTo, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return Expected;
}

#endif // M_SEPARATETYPE_smint

#endif // PLATFORM_LINUX
