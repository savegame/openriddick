
#include "../Platform/Platform.h"

#ifdef PLATFORM_LINUX

/*
	Entry point for the Linux/SDL2 port, modeled on MMain_PS3.cpp.
	MACRO_MAIN in MMain.h routes main() here.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

// Fatal-signal handler: print a backtrace to stderr before dying, so
// run logs pinpoint silent crashes (SIGSEGV etc.) without gdb.
static void Linux_FatalSignal(int _Sig)
{
	// Re-raise default on recursion
	signal(_Sig, SIG_DFL);
	fprintf(stderr, "\n[FATAL] signal %d (%s), backtrace:\n", _Sig, strsignal(_Sig));
	void* lBT[64];
	int n = backtrace(lBT, 64);
	backtrace_symbols_fd(lBT, n, 2);
	fflush(stderr);
	raise(_Sig);
}

static void Linux_InstallCrashHandler()
{
	signal(SIGSEGV, Linux_FatalSignal);
	signal(SIGBUS,  Linux_FatalSignal);
	signal(SIGFPE,  Linux_FatalSignal);
	signal(SIGILL,  Linux_FatalSignal);
	signal(SIGABRT, Linux_FatalSignal);
}

#define MOSMain_ShowError(Err) fprintf(stderr, "%s\n", (const char*)(Err))

int Linux_Main(int _argc, char** _argv, const char* _pAppClassName)
{
	Linux_InstallCrashHandler();
	// -datapath <dir>: run the engine from the game-resource directory
	// (the engine loads everything relative to the working directory,
	// e.g. Content\, Environment.cfg, Sbz1/...)
	static char CmdLine[4096];
	CmdLine[0] = 0;
	for (int i = 1; i < _argc; i++)
	{
		if (strcmp(_argv[i], "-datapath") == 0 && i + 1 < _argc)
		{
			if (chdir(_argv[i + 1]) != 0)
			{
				fprintf(stderr, "openriddick: -datapath: cannot chdir to '%s'\n", _argv[i + 1]);
				return 1;
			}
			i++;
			continue;
		}
		// Presentation options (consumed by CDisplayContextSDL2 via env,
		// see MDisplayPresent.h):
		//   -rotate 0|90|180|270   rotate the presented image clockwise
		//   -winsize WxH           physical window size (default 1280x720)
		//   -fbosize WxH           logical engine resolution override
		//                          (default: window size, swapped at 90/270)
		if (strcmp(_argv[i], "-rotate") == 0 && i + 1 < _argc)
		{
			setenv("RIDDICK_ROTATE", _argv[i + 1], 1);
			i++;
			continue;
		}
		if (strcmp(_argv[i], "-winsize") == 0 && i + 1 < _argc)
		{
			setenv("RIDDICK_WINSIZE", _argv[i + 1], 1);
			i++;
			continue;
		}
		if (strcmp(_argv[i], "-fbosize") == 0 && i + 1 < _argc)
		{
			setenv("RIDDICK_FBOSIZE", _argv[i + 1], 1);
			i++;
			continue;
		}
		if (CmdLine[0])
			strncat(CmdLine, " ", sizeof(CmdLine) - strlen(CmdLine) - 1);
		strncat(CmdLine, _argv[i], sizeof(CmdLine) - strlen(CmdLine) - 1);
	}

	MRTC_GetObjectManager()->SetDllLoading(false);

	MRTC_ObjectManager* pObjMgr = MRTC_GetObjectManager();

	// -------------------------------------------------------------------
	// Create exception-log
	M_TRY
	{
		spCReferenceCount spObj = pObjMgr->CreateObject("CCExceptionLog", "SYSTEM.EXCEPTIONLOG");
		if (spObj != NULL)
		{
			MACRO_GetRegisterObject(CCExceptionLog, pLog, "SYSTEM.EXCEPTIONLOG");
			if (!pLog) Error_static("Linux_Main", "Unable to create exception-log object.");
		}
	}
	M_CATCH(
	catch(CCException _Ex)
	{
		MOSMain_ShowError(_Ex.GetExceptionInfo().GetString());
		exit(1);
	}
	);

	// -------------------------------------------------------------------
	// Create logfile object
	M_TRY
	{
		spCReferenceCount spObj = pObjMgr->CreateObject("CLogFile", "SYSTEM.LOG");
		if (spObj != NULL)
		{
			ILogFile* pLog = TDynamicCast<ILogFile>((CReferenceCount*)spObj);
			if (!pLog) Error_static("Linux_Main", "Unable to create logfile object.");
		}
	}
	M_CATCH(
	catch(CCException _Ex)
	{
		MOSMain_ShowError(_Ex.GetExceptionInfo().GetString());
		exit(1);
	}
	);

	M_TRY
	{
		M_TRY
		{
			// -------------------------------------------------------------------
			//  Create CSystem
			spCReferenceCount spObj;

			spObj = (CReferenceCount*)pObjMgr->CreateObject("CSystemLinux");
			if (!spObj) Error_static("Linux_Main", "Unable to instance CSystemLinux object.");

			TPtr<CSystem> spSys = safe_cast<CSystem>((CReferenceCount*)spObj);
			if (!spSys) Error_static("Linux_Main", "CSystemLinux object was not a class of CSystem.");
			spObj = NULL;

			MRTC_GOM()->RegisterObject((CReferenceCount*)spSys, "SYSTEM");
			M_TRY
			{
				spSys->Create(NULL, NULL, CmdLine, 0, _pAppClassName);
				spSys->DoModal();
			}
			M_CATCH(
			catch(CCException)
			{
				spSys->Destroy();
				MRTC_GOM()->UnregisterObject((CReferenceCount*)spSys, "SYSTEM");
				throw;
			}
			);
			spSys->Destroy();
			MRTC_GOM()->UnregisterObject((CReferenceCount*)spSys, "SYSTEM");
		}
		M_CATCH(
		catch(CCException _Ex)
		{
			MACRO_GetRegisterObject(CCExceptionLog, pLog, "SYSTEM.EXCEPTIONLOG");
			if (pLog)
				pLog->DisplayFatal();
			else
				MOSMain_ShowError(_Ex.GetExceptionInfo().GetString());
		}
		);
	}
	M_CATCH(
	catch(CCException _Ex)
	{
		MOSMain_ShowError(_Ex.GetExceptionInfo().GetString());
	}
	);

	MRTC_GOM()->UnregisterObject(NULL, "SYSTEM.LOG");
	MRTC_GOM()->UnregisterObject(NULL, "SYSTEM.EXCEPTIONLOG");

	return 0;
}

#endif // PLATFORM_LINUX
