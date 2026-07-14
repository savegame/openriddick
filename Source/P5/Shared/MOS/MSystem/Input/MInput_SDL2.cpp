// No-op SDL2 input backend for the Linux/SDL2 port.
//
// CInputContextCore already implements every method of CInputContext
// (keyboard state, mouse polling, envelopes, memcard stubs), so this
// backend just registers itself with the RTC object factory under the
// name "CInputContext_SDL2" -- the string MCreateInputContext() asks
// for on PLATFORM_LINUX -- and forwards Create/Update to the core.
//
// No SDL_PollEvent wiring yet: keys and mouse deltas stay zero, which
// is enough to boot past CSystemCore::CreateInput and let the render
// path come up. A real SDL2 input pump lives in a later phase (see
// CLAUDE.md, Phase 3 "Ввод").

#include "PCH.h"
#include "MInputCore.h"

class CInputContext_SDL2 : public CInputContextCore
{
	MRTC_DECLARE;
public:
	virtual void Create(const char* _pParams)
	{
		CInputContextCore::Create(_pParams);
	}

	virtual void Update()
	{
		CInputContextCore::Update();
	}
};

MRTC_IMPLEMENT_DYNAMIC(CInputContext_SDL2, CInputContextCore);
