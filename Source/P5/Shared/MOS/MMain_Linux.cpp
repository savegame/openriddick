
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

// Every RIDDICK_* switch this build knows about, printed at startup so a
// log always states which diagnostics were actually compiled in and armed.
// Reason this exists: a run whose flag produced no output is ambiguous --
// it can mean "the flag did nothing" or "this binary predates the flag" --
// and telling those apart cost two round-trips with the person running the
// game. Now the banner answers it before the first frame.
static void Linux_LogActiveDebugFlags()
{
	static const char* s_lNames[] =
	{
		// render
		"RIDDICK_DBG_GL", "RIDDICK_DBG_VIEW", "RIDDICK_DBG_VPCALC", "RIDDICK_DBG_MVP",
		"RIDDICK_DBG_SHADER", "RIDDICK_DBG_NDS", "RIDDICK_DBG_LFM", "RIDDICK_DBG_MODELS",
		"RIDDICK_DBG_SURF", "RIDDICK_DBG_XTC", "RIDDICK_DBG_VP",
		"RIDDICK_FP20", "RIDDICK_NDS", "RIDDICK_LFM", "RIDDICK_LFM_SCALE",
		"RIDDICK_DIFFUSE_ONLY", "RIDDICK_NO_FOG", "RIDDICK_NO_SCISSOR", "RIDDICK_NO_LIGHT",
		"RIDDICK_NO_VBCACHE", "RIDDICK_DIRECT_RENDER", "RIDDICK_CULL_MODE",
		"RIDDICK_SKINNING", "RIDDICK_HWSKIN", "RIDDICK_SKIN_DROPNOPALETTE", "RIDDICK_IDXCHECK",
		"RIDDICK_SKIP_SKINNED", "RIDDICK_SKIP_SHADOWVOL",
		"RIDDICK_SKIP_CHARS", "RIDDICK_SKIP_PROPS", "RIDDICK_SKIP_SPRITES",
		"RIDDICK_SKIP_SPOTVOL", "RIDDICK_SKIP_SKY", "RIDDICK_SKIP_PARTICLES",
		"RIDDICK_ONLY_BSP", "RIDDICK_FORCE_TEX", "RIDDICK_ZPREPASS_COLOR",
		"RIDDICK_NO_CLAMP",
		// gameplay / scripts
		"RIDDICK_DBG_USE", "RIDDICK_DBG_MSG", "RIDDICK_LOG_MSG", "RIDDICK_DBG_AG2FX",
		"RIDDICK_DBG_PHYS", "RIDDICK_DBG_PATH", "RIDDICK_DBG_SKEL",
		"RIDDICK_DBG_AG2FMT", "RIDDICK_DBG_ITEM", "RIDDICK_DBG_RATE", "RIDDICK_DBG_MOVE",
		"RIDDICK_DBG_BONES", "RIDDICK_DBG_AG2",
		"RIDDICK_AG2_DEBUGFLAGS", "RIDDICK_AG2_DEBUGOBJ", "RIDDICK_DBG_DLGLEN",
		"RIDDICK_DBG_ANIMTIME", "RIDDICK_DBG_SEQ", "RIDDICK_DBG_ANIMV",
		// misc
		"RIDDICK_STARTMAP", "RIDDICK_AUTOSTART", "RIDDICK_AUTOSTART_MODE",
		"RIDDICK_VBHEAP", "RIDDICK_DBG_VBM", "RIDDICK_DBG_PALETTE",
	};
	const int nNames = (int)(sizeof(s_lNames) / sizeof(s_lNames[0]));

	fprintf(stderr, "[FLAGS] active:");
	int nOn = 0;
	for (int i = 0; i < nNames; i++)
	{
		const char* v = getenv(s_lNames[i]);
		if (v && *v && !(v[0] == '0' && v[1] == 0))
		{
			fprintf(stderr, " %s=%s", s_lNames[i], v);
			++nOn;
		}
	}
	if (!nOn)
		fprintf(stderr, " (none)");
	fprintf(stderr, "\n");
	fflush(stderr);
}

int Linux_Main(int _argc, char** _argv, const char* _pAppClassName)
{
	Linux_InstallCrashHandler();
	Linux_LogActiveDebugFlags();
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
