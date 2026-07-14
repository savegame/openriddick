
#include "../Platform/Platform.h"

#ifdef PLATFORM_LINUX

/*
	Entry point for the Linux/SDL2 port, modeled on MMain_PS3.cpp.
	MACRO_MAIN in MMain.h routes main() here.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MOSMain_ShowError(Err) fprintf(stderr, "%s\n", (const char*)(Err))

int Linux_Main(int _argc, char** _argv, const char* _pAppClassName)
{
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
