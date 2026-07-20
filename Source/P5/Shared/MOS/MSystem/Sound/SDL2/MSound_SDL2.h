
#ifndef DInc_MSound_SDL2_h
#define DInc_MSound_SDL2_h

#include "../MSound.h"
#include "../MSound_Mixer.h"
#include "../SCMixer/MSound_SCMixer.h"

// -------------------------------------------------------------------
// SDL2 sound context for the Linux port - milestone M0 (context stub).
// The class exists so MCreateSoundContext("CSoundContext_SDL2") succeeds
// and game code stops taking NULL sound-context branches. All Platform_*
// methods are no-ops: no SDL audio device is opened and no voice data is
// streamed, so every voice stays silent. See Docs/Sound_SDL2.md.
class CSoundContext_SDL2 : public CSoundContext_Mixer
{
	typedef CSoundContext_Mixer CSuper;

	MRTC_DECLARE;

public:

	CSoundContext_SDL2();
	~CSoundContext_SDL2();

	virtual void Create(CStr _Params);

	// Platform interface (CSoundContext_Mixer)
	virtual void Platform_GetInfo(CPlatformInfo &_Info);
	virtual void Platform_Init(uint32 _MaxMixerVoices);
	virtual void Platform_StartStreamingToMixer(uint32 _MixerVoice, CVoice *_pVoice, uint32 _WaveID, fp32 _SampleRate);
	virtual void Platform_StopStreamingToMixer(uint32 _MixerVoice);

	virtual void Refresh();
};

typedef TPtr<CSoundContext_SDL2> spCSoundContext_SDL2;

#endif // DInc_MSound_SDL2_h
