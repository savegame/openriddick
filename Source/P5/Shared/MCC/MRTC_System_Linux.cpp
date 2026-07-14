
/*
	MRTC system layer for Linux (x86_64 / ARM / Android), used by the
	GLES3 + SDL2 port. POSIX implementation modeled on
	MRTC_System_Win32.cpp / MRTC_System_PS3.cpp.

	Included from MRTC_System.cpp (which is itself included from Mrtc.cpp),
	so all engine types are available.

	Notes:
	 - Clocks are CLOCK_MONOTONIC nanoseconds (frequency 1e9), see
	   MRTC_System_Linux.h for the inline clock/atomic implementations.
	 - Async file I/O is currently implemented synchronously: the operation
	   completes inside OS_FileAsyncRead/Write and the handle only carries
	   the result. Good enough for bring-up; can be moved to a worker
	   thread later (the PS3 version does exactly that).
	 - Networking (MRTC_SystemInfo::CNetwork) is stubbed out for now.
*/

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

/*************************************************************************************************\
| Internal data
\*************************************************************************************************/

class MRTC_SystemInfoInternal
{
public:
};

MRTC_SystemInfo::MRTC_SystemInfo()
{
#ifdef M_STATIC
	MRTC_ObjectManager::m_pSystemInfo = this;
#endif
	long nCPU = sysconf(_SC_NPROCESSORS_ONLN);
	m_nCPU = (nCPU > 0) ? (uint32)nCPU : 1;
	m_CPUFrequencyu = 1;
	m_CPUFrequencyfp = 1;
	m_CPUFrequencyRecp = 1;
	m_CPUFeatures = 0;
	m_CPUFeaturesEnabled = 0xffffffff;
	m_CPUName[0] = 0;
	m_CPUNameWithFeatures[0] = 0;

	static mint SysInfoData[(sizeof(MRTC_SystemInfoInternal) + sizeof(mint) - 1) / sizeof(mint)];
	m_pInternalData = new(SysInfoData) MRTC_SystemInfoInternal;

	m_pThreadContext = NULL;

	CPU_MeasureFrequency();
}

MRTC_SystemInfo::~MRTC_SystemInfo()
{
	m_pInternalData->~MRTC_SystemInfoInternal();

	if (m_pThreadContext)
		m_pThreadContext->~MRTC_ThreadContext();
}

void MRTC_SystemInfo::PostCreate()
{
}

/*************************************************************************************************\
| Clock
\*************************************************************************************************/

void MRTC_SystemInfo::CPU_CreateNames()
{
	const char* pName = "Unknown CPU";
#if defined(__x86_64__)
	pName = "x86-64";
#elif defined(__aarch64__)
	pName = "ARM64";
#elif defined(__arm__)
	pName = "ARM";
#endif
	strcpy(m_CPUName, pName);
	strcpy(m_CPUNameWithFeatures, pName);
}

void MRTC_SystemInfo::CPU_MeasureFrequency()
{
	// CPU_Clock/OS_Clock return CLOCK_MONOTONIC nanoseconds
	m_CPUFrequencyu = 1000000000;
	m_CPUFrequencyfp = (fp32)m_CPUFrequencyu;
	m_CPUFrequencyRecp = 1.0f / m_CPUFrequencyfp;
	m_OSFrequencyu = m_CPUFrequencyu;
	m_OSFrequencyfp = m_CPUFrequencyfp;
	m_OSFrequencyRecp = m_CPUFrequencyRecp;
}

uint64 MRTC_SystemInfo::CPU_ClockFrequencyInt() const
{
	return m_CPUFrequencyu;
}

fp32 MRTC_SystemInfo::CPU_ClockFrequencyFloat() const
{
	return m_CPUFrequencyfp;
}

fp32 MRTC_SystemInfo::CPU_ClockFrequencyRecp() const
{
	return m_CPUFrequencyRecp;
}

fp32 MRTC_SystemInfo::OS_ClockFrequencyRecp() const
{
	return m_CPUFrequencyRecp;
}

uint64 MRTC_SystemInfo::OS_ClockFrequencyInt() const
{
	return m_CPUFrequencyu;
}

fp32 MRTC_SystemInfo::OS_ClockFrequencyFloat() const
{
	return m_CPUFrequencyfp;
}

void MRTC_SystemInfo::OS_NamedEvent_Begin(const char* _pName, uint32 _Color)
{
}

void MRTC_SystemInfo::OS_NamedEvent_End()
{
}

/*************************************************************************************************\
| Memory management
\*************************************************************************************************/

void* MRTC_SystemInfo::OS_HeapAlloc(uint32 _Size)
{
	return malloc(_Size);
}

void* MRTC_SystemInfo::OS_HeapAllocAlign(uint32 _Size, uint32 _Align)
{
	if (_Align < sizeof(void*))
		_Align = sizeof(void*);
	void* pMem = NULL;
	if (posix_memalign(&pMem, _Align, _Size) != 0)
		return NULL;
	return pMem;
}

void* MRTC_SystemInfo::OS_HeapRealloc(void *_pMem, uint32 _Size)
{
	return realloc(_pMem, _Size);
}

void* MRTC_SystemInfo::OS_HeapReallocAlign(void *_pMem, uint32 _Size, uint32 _Align)
{
	// glibc has no aligned realloc; emulate it
	void* pNew = OS_HeapAllocAlign(_Size, _Align);
	if (!pNew)
		return NULL;
	if (_pMem)
	{
		size_t OldSize = malloc_usable_size(_pMem);
		memcpy(pNew, _pMem, (OldSize < _Size) ? OldSize : _Size);
		free(_pMem);
	}
	return pNew;
}

void MRTC_SystemInfo::OS_HeapFree(void *_pMem)
{
	free(_pMem);
}

uint32 MRTC_SystemInfo::OS_HeapSize(const void *_pMem)
{
	return (uint32)malloc_usable_size((void*)_pMem);
}

mint MRTC_SystemInfo::OS_MemSize(void *_pBlock)
{
	return (mint)malloc_usable_size(_pBlock);
}

void* MRTC_SystemInfo::OS_Alloc(uint32 _Size, uint32 _Alignment)
{
	// Like VirtualAlloc on Win32, callers rely on this memory being zeroed
	void* pMem = OS_HeapAllocAlign(_Size, _Alignment);
	if (pMem)
		memset(pMem, 0, _Size);
	return pMem;
}

void MRTC_SystemInfo::OS_Free(void* _pMem)
{
	free(_pMem);
}

void* MRTC_SystemInfo::OS_AllocGPU(uint32 _Size, bool _bCached)
{
	// No dedicated GPU memory on this platform; the GLES3 renderer
	// uploads through GL buffer objects instead.
	return OS_HeapAllocAlign(_Size, 128);
}

void MRTC_SystemInfo::OS_FreeGPU(void *_pMem)
{
	free(_pMem);
}

bool MRTC_SystemInfo::OS_Commit(void *_pMem, uint32 _Size, bool _bCommited)
{
	// Regular heap memory is always committed
	return true;
}

uint32 MRTC_SystemInfo::OS_CommitGranularity()
{
	return (uint32)sysconf(_SC_PAGESIZE);
}

uint32 MRTC_SystemInfo::OS_PhysicalMemorySize()
{
	struct sysinfo Info;
	if (sysinfo(&Info) != 0)
		return 0;
	uint64 Total = (uint64)Info.totalram * Info.mem_unit;
	return (Total > 0xffffffffull) ? 0xffffffff : (uint32)Total;
}

uint32 MRTC_SystemInfo::OS_PhysicalMemoryFree()
{
	struct sysinfo Info;
	if (sysinfo(&Info) != 0)
		return 0;
	uint64 Free = (uint64)Info.freeram * Info.mem_unit;
	return (Free > 0xffffffffull) ? 0xffffffff : (uint32)Free;
}

uint32 MRTC_SystemInfo::OS_PhysicalMemoryUsed()
{
	return OS_PhysicalMemorySize() - OS_PhysicalMemoryFree();
}

uint32 MRTC_SystemInfo::OS_PhysicalMemoryLowestFree()
{
	return OS_PhysicalMemoryFree();
}

uint32 MRTC_SystemInfo::OS_PhysicalMemoryLargestFree()
{
	return OS_PhysicalMemoryFree();
}

/*************************************************************************************************\
| Process / threads
\*************************************************************************************************/

void* MRTC_SystemInfo::OS_GetProcessID()
{
	return (void*)(mint)getpid();
}

void* MRTC_SystemInfo::OS_GetThreadID()
{
	return (void*)(mint)syscall(SYS_gettid);
}

uint32 MRTC_SystemInfo::Thread_GetCurrentID()
{
	return (uint32)syscall(SYS_gettid);
}

void MRTC_SystemInfo::OS_Sleep(int _Milliseconds)
{
	usleep((useconds_t)_Milliseconds * 1000);
}

void MRTC_SystemInfo::OS_Yeild()
{
	sched_yield();
}

class CLinuxThread
{
public:
	pthread_t m_Thread;
	uint32 (M_STDCALL *m_pfnEntry)(void*);
	void* m_pContext;
	volatile int32 m_ExitCode;
	volatile int32 m_bRunning;

	static void* EntryPoint(void* _pThis)
	{
		CLinuxThread* pThis = (CLinuxThread*)_pThis;
		uint32 Ret = pThis->m_pfnEntry(pThis->m_pContext);
		pThis->m_ExitCode = (int32)Ret;
		__atomic_store_n(&pThis->m_bRunning, 0, __ATOMIC_SEQ_CST);
		return (void*)(mint)Ret;
	}
};

void* MRTC_SystemInfo::OS_ThreadCreate(uint32(M_STDCALL*_pfnEntryPoint)(void*), int _StackSize, void* _pContext, int _ThreadPriority, const char* _pName)
{
	CLinuxThread* pThread = (CLinuxThread*)malloc(sizeof(CLinuxThread));
	pThread->m_pfnEntry = _pfnEntryPoint;
	pThread->m_pContext = _pContext;
	pThread->m_ExitCode = 0;
	pThread->m_bRunning = 1;

	pthread_attr_t Attr;
	pthread_attr_init(&Attr);
	if (_StackSize > 0)
	{
		size_t StackSize = (_StackSize < PTHREAD_STACK_MIN) ? PTHREAD_STACK_MIN : (size_t)_StackSize;
		pthread_attr_setstacksize(&Attr, StackSize);
	}

	if (pthread_create(&pThread->m_Thread, &Attr, CLinuxThread::EntryPoint, pThread) != 0)
	{
		pthread_attr_destroy(&Attr);
		free(pThread);
		return NULL;
	}
	pthread_attr_destroy(&Attr);

	if (_pName)
		pthread_setname_np(pThread->m_Thread, _pName);

	return pThread;
}

int MRTC_SystemInfo::OS_ThreadDestroy(void* _hThread)
{
	CLinuxThread* pThread = (CLinuxThread*)_hThread;
	void* pRet = NULL;
	pthread_join(pThread->m_Thread, &pRet);
	int ExitCode = pThread->m_ExitCode;
	free(pThread);
	return ExitCode;
}

void MRTC_SystemInfo::OS_ThreadExit(int _ExitCode)
{
	pthread_exit((void*)(mint)_ExitCode);
}

int MRTC_SystemInfo::OS_ThreadGetExitCode(void* _hThread)
{
	return ((CLinuxThread*)_hThread)->m_ExitCode;
}

bool MRTC_SystemInfo::OS_ThreadIsRunning(void* _hThread)
{
	return __atomic_load_n(&((CLinuxThread*)_hThread)->m_bRunning, __ATOMIC_SEQ_CST) != 0;
}

void MRTC_SystemInfo::OS_ThreadTerminate(void* _hThread, int _ExitCode)
{
	CLinuxThread* pThread = (CLinuxThread*)_hThread;
	pthread_cancel(pThread->m_Thread);
	pThread->m_ExitCode = _ExitCode;
	__atomic_store_n(&pThread->m_bRunning, 0, __ATOMIC_SEQ_CST);
}

void MRTC_SystemInfo::Thread_SetName(const char *_pName)
{
	char Name[16];
	strncpy(Name, _pName, 15);
	Name[15] = 0;
	prctl(PR_SET_NAME, (unsigned long)Name, 0, 0, 0);
}

void MRTC_SystemInfo::Thread_SetProcessor(uint32 _Processor)
{
	// Thread affinity is a scheduling hint on the original consoles;
	// let the Linux scheduler decide.
}

void MRTC_SystemInfo::Thread_SetProcessor(uint32 _ThreadID, uint32 _Processor)
{
}

#ifndef M_THREADSPINCOUNT
int MRTC_SystemInfo::OS_ThreadSpinCount()
{
	return 400;
}
#endif

/*************************************************************************************************\
| Thread local storage
\*************************************************************************************************/

aint MRTC_SystemInfo::Thread_LocalAlloc()
{
	pthread_key_t Key;
	if (pthread_key_create(&Key, NULL) != 0)
		return -1;
	return (aint)Key;
}

void MRTC_SystemInfo::Thread_LocalFree(aint _Index)
{
	pthread_key_delete((pthread_key_t)_Index);
}

void MRTC_SystemInfo::Thread_LocalSetValue(aint _Index, mint _Value)
{
	pthread_setspecific((pthread_key_t)_Index, (void*)_Value);
}

mint MRTC_SystemInfo::Thread_LocalGetValue(aint _Index)
{
	return (mint)pthread_getspecific((pthread_key_t)_Index);
}

/*************************************************************************************************\
| Mutex
\*************************************************************************************************/

void* MRTC_SystemInfo::OS_MutexOpen(const char* _pName)
{
	// Named cross-process mutexes are not needed; a recursive
	// process-local mutex covers the engine's usage.
	pthread_mutex_t* pMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_t Attr;
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(pMutex, &Attr);
	pthread_mutexattr_destroy(&Attr);
	return pMutex;
}

void MRTC_SystemInfo::OS_MutexClose(void* _pMutex)
{
	pthread_mutex_destroy((pthread_mutex_t*)_pMutex);
	free(_pMutex);
}

void MRTC_SystemInfo::OS_MutexLock(void* _pMutex)
{
	pthread_mutex_lock((pthread_mutex_t*)_pMutex);
}

void MRTC_SystemInfo::OS_MutexUnlock(void* _pMutex)
{
	pthread_mutex_unlock((pthread_mutex_t*)_pMutex);
}

bool MRTC_SystemInfo::OS_MutexTryLock(void* _pMutex)
{
	return pthread_mutex_trylock((pthread_mutex_t*)_pMutex) == 0;
}

/*************************************************************************************************\
| Semaphore
\*************************************************************************************************/

static void Linux_AbsTimeout(struct timespec& _Ts, fp64 _TimeoutSeconds)
{
	clock_gettime(CLOCK_REALTIME, &_Ts);
	int64 ns = _Ts.tv_nsec + (int64)(_TimeoutSeconds * 1000000000.0);
	_Ts.tv_sec += ns / 1000000000;
	_Ts.tv_nsec = ns % 1000000000;
}

void* MRTC_SystemInfo::Semaphore_Alloc(mint _InitialCount, mint _MaximumCount)
{
	sem_t* pSem = (sem_t*)malloc(sizeof(sem_t));
	sem_init(pSem, 0, (unsigned)_InitialCount);
	return pSem;
}

void MRTC_SystemInfo::Semaphore_Free(void *_pSemaphore)
{
	sem_destroy((sem_t*)_pSemaphore);
	free(_pSemaphore);
}

void MRTC_SystemInfo::Semaphore_Increase(void * _pSemaphore, mint _Count)
{
	for (mint i = 0; i < _Count; ++i)
		sem_post((sem_t*)_pSemaphore);
}

void MRTC_SystemInfo::Semaphore_Wait(void * _pSemaphore)
{
	while (sem_wait((sem_t*)_pSemaphore) != 0 && errno == EINTR)
		;
}

bint MRTC_SystemInfo::Semaphore_TryWait(void * _pSemaphore)
{
	return sem_trywait((sem_t*)_pSemaphore) == 0;
}

bint MRTC_SystemInfo::Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	struct timespec Ts;
	Linux_AbsTimeout(Ts, _Timeout);
	while (sem_timedwait((sem_t*)_pSemaphore, &Ts) != 0)
	{
		if (errno != EINTR)
			return false;
	}
	return true;
}

/*************************************************************************************************\
| Event
\*************************************************************************************************/

class CLinuxEvent
{
public:
	pthread_mutex_t m_Mutex;
	pthread_cond_t m_Cond;
	bint m_bSignaled;
	bint m_bAutoReset;
};

void* MRTC_SystemInfo::Event_Alloc(bint _InitialSignal, bint _bAutoReset)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)malloc(sizeof(CLinuxEvent));
	pthread_mutex_init(&pEvent->m_Mutex, NULL);
	pthread_cond_init(&pEvent->m_Cond, NULL);
	pEvent->m_bSignaled = _InitialSignal;
	pEvent->m_bAutoReset = _bAutoReset;
	return pEvent;
}

void MRTC_SystemInfo::Event_Free(void *_pEvent)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	pthread_cond_destroy(&pEvent->m_Cond);
	pthread_mutex_destroy(&pEvent->m_Mutex);
	free(pEvent);
}

void MRTC_SystemInfo::Event_SetSignaled(void * _pEvent)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	pthread_mutex_lock(&pEvent->m_Mutex);
	pEvent->m_bSignaled = true;
	if (pEvent->m_bAutoReset)
		pthread_cond_signal(&pEvent->m_Cond);
	else
		pthread_cond_broadcast(&pEvent->m_Cond);
	pthread_mutex_unlock(&pEvent->m_Mutex);
}

void MRTC_SystemInfo::Event_ResetSignaled(void * _pEvent)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	pthread_mutex_lock(&pEvent->m_Mutex);
	pEvent->m_bSignaled = false;
	pthread_mutex_unlock(&pEvent->m_Mutex);
}

void MRTC_SystemInfo::Event_Wait(void * _pEvent)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	pthread_mutex_lock(&pEvent->m_Mutex);
	while (!pEvent->m_bSignaled)
		pthread_cond_wait(&pEvent->m_Cond, &pEvent->m_Mutex);
	if (pEvent->m_bAutoReset)
		pEvent->m_bSignaled = false;
	pthread_mutex_unlock(&pEvent->m_Mutex);
}

bint MRTC_SystemInfo::Event_WaitTimeout(void * _pEvent, fp64 _Timeout)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	struct timespec Ts;
	Linux_AbsTimeout(Ts, _Timeout);
	pthread_mutex_lock(&pEvent->m_Mutex);
	while (!pEvent->m_bSignaled)
	{
		if (pthread_cond_timedwait(&pEvent->m_Cond, &pEvent->m_Mutex, &Ts) == ETIMEDOUT)
			break;
	}
	bint bSignaled = pEvent->m_bSignaled;
	if (bSignaled && pEvent->m_bAutoReset)
		pEvent->m_bSignaled = false;
	pthread_mutex_unlock(&pEvent->m_Mutex);
	return bSignaled;
}

bint MRTC_SystemInfo::Event_TryWait(void * _pEvent)
{
	CLinuxEvent* pEvent = (CLinuxEvent*)_pEvent;
	pthread_mutex_lock(&pEvent->m_Mutex);
	bint bSignaled = pEvent->m_bSignaled;
	if (bSignaled && pEvent->m_bAutoReset)
		pEvent->m_bSignaled = false;
	pthread_mutex_unlock(&pEvent->m_Mutex);
	return bSignaled;
}

/*************************************************************************************************\
| Debug output
\*************************************************************************************************/

void MRTC_SystemInfo::OS_Assert(const char* _pMsg, const char* _pFile, int _Line)
{
	fprintf(stderr, "ASSERT: %s (%s:%d)\n", _pMsg ? _pMsg : "", _pFile ? _pFile : "?", _Line);
	fflush(stderr);
	M_BREAKPOINT;
}

void M_ARGLISTCALL MRTC_SystemInfo::OS_Trace(const char *_pStr, ...)
{
	va_list Args;
	va_start(Args, _pStr);
	vfprintf(stdout, _pStr, Args);
	va_end(Args);
	fflush(stdout);
}

void MRTC_SystemInfo::OS_TraceRaw(const char *_pMsg)
{
	fputs(_pMsg, stdout);
	fflush(stdout);
}

void MRTC_SystemInfo::OS_EnableUnhandledException(bool _bEnabled)
{
}

void MRTC_SystemInfo::OS_EnableAutoCoredumpOnException(bool _bEnabled)
{
}

mint MRTC_SystemInfo::OS_TraceStack(mint *_pCallStack, int _MaxStack, mint _ebp)
{
	return 0;
}

void MRTC_SystemInfo::OS_SendProfilingSnapshot(uint32 _ID)
{
}

void MRTC_SystemInfo::RD_GetServerName(char *_pName)
{
	_pName[0] = 0;
}

void MRTC_SystemInfo::RD_ClientInit(void *_pPacket, mint &_Size)
{
	_Size = 0;
}

void MRTC_SystemInfo::RD_PeriodicUpdate()
{
}

/*************************************************************************************************\
| Path resolution
|
| Game data references files with backslashes and arbitrary case (the
| original filesystems were case-insensitive). Convert separators and,
| when the exact path does not exist, resolve each component
| case-insensitively against the actual directory contents.
\*************************************************************************************************/

static bool Linux_CaseResolve(char* _pPath)
{
	// _pPath uses '/' separators. Returns true if some existing path was
	// found (possibly rewriting the case of components in-place).
	if (access(_pPath, F_OK) == 0)
		return true;

	char Buf[2048];
	char* pOut = Buf;
	const char* pIn = _pPath;
	if (*pIn == '/')
		*pOut++ = *pIn++;
	*pOut = 0;

	while (*pIn)
	{
		const char* pSep = strchr(pIn, '/');
		size_t CompLen = pSep ? (size_t)(pSep - pIn) : strlen(pIn);
		char Comp[512];
		if (CompLen >= sizeof(Comp)) return false;
		memcpy(Comp, pIn, CompLen); Comp[CompLen] = 0;

		char Test[2048];
		snprintf(Test, sizeof(Test), "%s%s", Buf[0] ? Buf : "", Comp);
		if (access(Test, F_OK) != 0)
		{
			// search directory for a case-insensitive match
			const char* pDir = Buf[0] ? Buf : ".";
			DIR* pD = opendir(pDir);
			bool bFound = false;
			if (pD)
			{
				struct dirent* pE;
				while ((pE = readdir(pD)) != NULL)
				{
					if (strcasecmp(pE->d_name, Comp) == 0)
					{
						strcpy(Comp, pE->d_name);
						bFound = true;
						break;
					}
				}
				closedir(pD);
			}
			if (!bFound)
				return false;
		}
		size_t l = strlen(Comp);
		memcpy(pOut, Comp, l); pOut += l;
		if (pSep) { *pOut++ = '/'; pIn = pSep + 1; } else pIn += CompLen;
		*pOut = 0;
	}
	strcpy(_pPath, Buf);
	return true;
}

static const char* Linux_ResolvePath(const char* _pPath, char* _pBuf, int _BufSize)
{
	// Fix separators
	int i = 0;
	for (; _pPath[i] && i < _BufSize - 1; i++)
		_pBuf[i] = (_pPath[i] == '\\') ? '/' : _pPath[i];
	_pBuf[i] = 0;
	Linux_CaseResolve(_pBuf);
	return _pBuf;
}

/*************************************************************************************************\
| Directories
\*************************************************************************************************/

char* MRTC_SystemInfo::OS_DirectoryGetCurrent(char* _pBuf, int _MaxLength)
{
	return getcwd(_pBuf, _MaxLength);
}

bool MRTC_SystemInfo::OS_DirectoryChange(const char* _pPath)
{
	char Path[2048];
	Linux_ResolvePath(_pPath, Path, sizeof(Path));
	return chdir(Path) == 0;
}

bool MRTC_SystemInfo::OS_DirectoryCreate(const char* _pPath)
{
	return mkdir(_pPath, 0755) == 0 || errno == EEXIST;
}

bool MRTC_SystemInfo::OS_DirectoryRemove(const char* _pPath)
{
	return rmdir(_pPath) == 0;
}

bool MRTC_SystemInfo::OS_DirectoryExists(const char *_pPath)
{
	char Path[2048];
	Linux_ResolvePath(_pPath, Path, sizeof(Path));
	struct stat St;
	return stat(Path, &St) == 0 && S_ISDIR(St.st_mode);
}

const char* MRTC_SystemInfo::OS_DirectorySeparator()
{
	return "/";
}

/*************************************************************************************************\
| Files
|
| File handles are the POSIX fd stored as (fd + 1) cast to void* so a valid
| handle is never NULL.
\*************************************************************************************************/

static M_INLINE int Linux_FD(void* _pFile)
{
	return (int)(mint)_pFile - 1;
}

void* MRTC_SystemInfo::OS_FileOpen(const char *_pFileName, bool _bRead, bool _bWrite, bool _bCreate, bool _bTruncate, bool _bDeferClose)
{
	int Flags = 0;
	if (_bRead && _bWrite)
		Flags = O_RDWR;
	else if (_bWrite)
		Flags = O_WRONLY;
	else
		Flags = O_RDONLY;
	if (_bCreate)
		Flags |= O_CREAT;
	if (_bTruncate)
		Flags |= O_TRUNC;

	char Path[2048];
	Linux_ResolvePath(_pFileName, Path, sizeof(Path));
	int fd = open(Path, Flags, 0644);
	if (fd < 0)
		return NULL;
	return (void*)(mint)(fd + 1);
}

void MRTC_SystemInfo::OS_FileClose(void *_pFile)
{
	close(Linux_FD(_pFile));
}

void MRTC_SystemInfo::OS_FileGetDrive(const char *_pFileName, char *_pDriveName)
{
	_pDriveName[0] = 0;
}

fint MRTC_SystemInfo::OS_FileSize(void *_pFile)
{
	struct stat St;
	if (fstat(Linux_FD(_pFile), &St) != 0)
		return 0;
	return (fint)St.st_size;
}

bool MRTC_SystemInfo::OS_FileExists(const char *_pPath)
{
	char Path[2048];
	Linux_ResolvePath(_pPath, Path, sizeof(Path));
	struct stat St;
	return stat(Path, &St) == 0 && S_ISREG(St.st_mode);
}

fint MRTC_SystemInfo::OS_FilePosition(const char *_pFileName)
{
	// Physical position on disc; only meaningful for optical media
	return 0;
}

bool MRTC_SystemInfo::OS_FileSetFileSize(const char *_pFileName, fint _FileSize)
{
	return truncate(_pFileName, (off_t)_FileSize) == 0;
}

int MRTC_SystemInfo::OS_FileOperationGranularity(const char *_pPath)
{
	return (int)sysconf(_SC_PAGESIZE);
}

bool MRTC_SystemInfo::OS_FileRemove(const char* _pPath)
{
	return unlink(_pPath) == 0;
}

bool MRTC_SystemInfo::OS_FileRename(const char* _pFrom, const char* _pTo)
{
	return rename(_pFrom, _pTo) == 0;
}

// Windows FILETIME epoch conversion (engine stores int64 file times)
static M_INLINE int64 Linux_TimeT2FileTime(time_t _Time)
{
	return ((int64)_Time * 10000000) + 116444736000000000ll;
}

bool MRTC_SystemInfo::OS_FileGetTime(void *_pFile, int64& _TimeCreate, int64& _TimeAccess, int64& _TimeWrite)
{
	struct stat St;
	if (fstat(Linux_FD(_pFile), &St) != 0)
		return false;
	_TimeCreate = Linux_TimeT2FileTime(St.st_ctime);
	_TimeAccess = Linux_TimeT2FileTime(St.st_atime);
	_TimeWrite = Linux_TimeT2FileTime(St.st_mtime);
	return true;
}

bool MRTC_SystemInfo::OS_FileSetTime(void *_pFile, const int64& _TimeCreate, const int64& _TimeAccess, const int64& _TimeWrite)
{
	return false;
}

/*************************************************************************************************\
| Async file I/O (synchronous bring-up implementation)
\*************************************************************************************************/

class CLinuxAsyncOp
{
public:
	fint m_BytesProcessed;
};

void* MRTC_SystemInfo::OS_FileAsyncRead(void *_pFile, void *_pData, fint _DataSize, fint _FileOffset)
{
	CLinuxAsyncOp* pOp = (CLinuxAsyncOp*)malloc(sizeof(CLinuxAsyncOp));
	ssize_t Bytes = pread(Linux_FD(_pFile), _pData, (size_t)_DataSize, (off_t)_FileOffset);
	pOp->m_BytesProcessed = (Bytes < 0) ? 0 : (fint)Bytes;
	return pOp;
}

void* MRTC_SystemInfo::OS_FileAsyncWrite(void *_pFile, const void *_pData, fint _DataSize, fint _FileOffset)
{
	CLinuxAsyncOp* pOp = (CLinuxAsyncOp*)malloc(sizeof(CLinuxAsyncOp));
	ssize_t Bytes = pwrite(Linux_FD(_pFile), _pData, (size_t)_DataSize, (off_t)_FileOffset);
	pOp->m_BytesProcessed = (Bytes < 0) ? 0 : (fint)Bytes;
	return pOp;
}

void MRTC_SystemInfo::OS_FileAsyncClose(void *_pAsyncInstance)
{
	free(_pAsyncInstance);
}

bool MRTC_SystemInfo::OS_FileAsyncIsFinished(void *_pAsyncInstance)
{
	return true;
}

fint MRTC_SystemInfo::OS_FileAsyncBytesProcessed(void *_pAsyncInstance)
{
	return ((CLinuxAsyncOp*)_pAsyncInstance)->m_BytesProcessed;
}

/*************************************************************************************************\
| Network (stubbed for bring-up; the engine handles a dead network gracefully)
\*************************************************************************************************/

bint MRTC_SystemInfo::CNetwork::gf_ResolveAddres(const ch8 *_pAddress, NNet::CNetAddressIPv4 &_Address)
{
	return false;
}

void *MRTC_SystemInfo::CNetwork::gf_Bind(const NNet::CNetAddressUDPv4 &_Address, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
	return NULL;
}

void *MRTC_SystemInfo::CNetwork::gf_Connect(const NNet::CNetAddressTCPv4 &_Address, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
	return NULL;
}

void *MRTC_SystemInfo::CNetwork::gf_Listen(const NNet::CNetAddressTCPv4 &_Address, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
	return NULL;
}

void *MRTC_SystemInfo::CNetwork::gf_Accept(void *_pSocket, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
	return NULL;
}

void MRTC_SystemInfo::CNetwork::gf_SetReportTo(void *_pSocket, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
}

void *MRTC_SystemInfo::CNetwork::gf_InheritHandle(void *_pSocket, NThread::CEventAutoResetReportableAggregate *_pReportTo)
{
	return NULL;
}

uint32 MRTC_SystemInfo::CNetwork::gf_GetState(void *_pSocket)
{
	return 0;
}

void MRTC_SystemInfo::CNetwork::gf_Close(void *_pSocket)
{
}

int MRTC_SystemInfo::CNetwork::gf_Receive(void *_pSocket, void *_pData, int _DataLen)
{
	return 0;
}

int MRTC_SystemInfo::CNetwork::gf_Send(void *_pSocket, const void *_pData, int _DataLen)
{
	return 0;
}

int MRTC_SystemInfo::CNetwork::gf_Receive(void *_pSocket, NNet::CNetAddressUDPv4 &_Address, void *_pData, int _DataLen)
{
	return 0;
}

int MRTC_SystemInfo::CNetwork::gf_Send(void *_pSocket, const NNet::CNetAddressUDPv4 &_Address, const void *_pData, int _DataLen)
{
	return 0;
}

/*************************************************************************************************\
| Directory enumeration (PS3File_* interface used by MFile_Misc.cpp on
| PLATFORM_PS3 and PLATFORM_LINUX)
\*************************************************************************************************/

#include <fnmatch.h>

void PS3File_FixPath( char *_String )
{
	// Normalize to forward slashes, collapse duplicate separators and
	// strip a trailing separator
	for (char* p = _String; *p; ++p)
		if (*p == '\\')
			*p = '/';

	int iInsert = 0;
	int iChar = 0;
	while(_String[iChar])
	{
		while((_String[iChar] == '/') && (_String[iChar + 1] == '/'))
			iChar++;
		if(iInsert != iChar)
			_String[iInsert] = _String[iChar];
		iInsert++;
		iChar++;
	}

	if((iInsert > 0) && (_String[iInsert - 1] == '/'))
		_String[iInsert - 1] = 0;
	else
		_String[iInsert] = 0;
}

class CLinuxFindFile
{
public:
	DIR* m_pDir;
	char m_Pattern[256];
	char m_Directory[1024];
};

static bool Linux_FindReadNext(CLinuxFindFile* pFind, char *_pRet, int &_FileSize, bool &_bDir)
{
	struct dirent* pEntry;
	while ((pEntry = readdir(pFind->m_pDir)) != NULL)
	{
		if (strcmp(pEntry->d_name, ".") == 0 || strcmp(pEntry->d_name, "..") == 0)
			continue;
		if (fnmatch(pFind->m_Pattern, pEntry->d_name, FNM_CASEFOLD) != 0)
			continue;

		strcpy(_pRet, pEntry->d_name);

		char FullName[1400];
		snprintf(FullName, sizeof(FullName), "%s/%s", pFind->m_Directory, pEntry->d_name);
		struct stat St;
		if (stat(FullName, &St) == 0)
		{
			_FileSize = (int)St.st_size;
			_bDir = S_ISDIR(St.st_mode);
		}
		else
		{
			_FileSize = 0;
			_bDir = false;
		}
		return true;
	}
	return false;
}

aint PS3File_FindFirst( const char *_pPath, char *_pRet, int &_FileSize, bool &_bDir)
{
	char Path[1024];
	strncpy(Path, _pPath, sizeof(Path) - 1);
	Path[sizeof(Path) - 1] = 0;
	PS3File_FixPath(Path);

	const char* pSlash = strrchr(Path, '/');
	CLinuxFindFile* pFind = (CLinuxFindFile*)malloc(sizeof(CLinuxFindFile));
	if (pSlash)
	{
		strncpy(pFind->m_Pattern, pSlash + 1, sizeof(pFind->m_Pattern) - 1);
		pFind->m_Pattern[sizeof(pFind->m_Pattern) - 1] = 0;
		size_t DirLen = pSlash - Path;
		if (DirLen >= sizeof(pFind->m_Directory))
			DirLen = sizeof(pFind->m_Directory) - 1;
		memcpy(pFind->m_Directory, Path, DirLen);
		pFind->m_Directory[DirLen] = 0;
	}
	else
	{
		strncpy(pFind->m_Pattern, Path, sizeof(pFind->m_Pattern) - 1);
		pFind->m_Pattern[sizeof(pFind->m_Pattern) - 1] = 0;
		strcpy(pFind->m_Directory, ".");
	}

	pFind->m_pDir = opendir(pFind->m_Directory[0] ? pFind->m_Directory : "/");
	if (!pFind->m_pDir)
	{
		free(pFind);
		return 0;
	}

	if (!Linux_FindReadNext(pFind, _pRet, _FileSize, _bDir))
	{
		closedir(pFind->m_pDir);
		free(pFind);
		return 0;
	}
	return (aint)pFind;
}

aint PS3File_FindNext( aint _FindHandle, char *_pRet, int &_FileSize, bool &_bDir)
{
	return Linux_FindReadNext((CLinuxFindFile*)_FindHandle, _pRet, _FileSize, _bDir) ? 1 : 0;
}

void PS3File_FindClose( aint _handle )
{
	CLinuxFindFile* pFind = (CLinuxFindFile*)_handle;
	if (pFind)
	{
		closedir(pFind->m_pDir);
		free(pFind);
	}
}

// Referenced by the *_Dyn.cpp static-registration files
void MRTC_ReferenceSymbol(...)
{
}

void gf_ModuleAdd()
{
	// Module (DLL) tracking is not used on Linux — everything is
	// statically linked.
}

/*************************************************************************************************\
| Bootstrap: the object manager (and its memory manager) must exist before
| any other static initializer runs. Same trick as on PS3.
\*************************************************************************************************/

void MRTC_CreateObjectManager();
void MRTC_DestroyObjectManager();

class CInitFirst
{
public:
	CInitFirst()
	{
		MRTC_CreateObjectManager();
	}
	~CInitFirst()
	{
		MRTC_DestroyObjectManager();
	}
};

static CInitFirst __attribute__((init_priority(101))) g_LinuxInit;
