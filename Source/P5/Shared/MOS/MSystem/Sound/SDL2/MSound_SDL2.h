
#ifndef DInc_MSound_SDL2_h
#define DInc_MSound_SDL2_h

#include "../MSound.h"
#include "../MSound_Mixer.h"
#include "../SCMixer/MSound_SCMixer.h"

#ifdef PLATFORM_LINUX
#include <SDL.h>
#endif

// -------------------------------------------------------------------
// SDL2 sound context for the Linux port.
// M0: context stub, all Platform_* no-ops.
// M1: SDL audio device + master output. The SDL callback pulls finished
//     mixer frames via CSC_Mixer::StartNewFrame() (lock-free, same as the
//     PS3 audio thread does) and copies interleaved stereo fp32 to the
//     SDL stream. No voice streaming yet - output is mixer silence.
//     See Docs/Sound_SDL2.md.
class CSoundContext_SDL2 : public CSoundContext_Mixer
{
	typedef CSoundContext_Mixer CSuper;

	MRTC_DECLARE;

#ifdef PLATFORM_LINUX
	SDL_AudioDeviceID m_AudioDevice;	// 0 = no device
	bint m_bSDLInit;
	bint m_bMixerReady;		// set at the end of Platform_Init, gates the callback
	bint m_bFormatOK;		// SDL gave us AUDIO_F32SYS stereo

	// Remainder of the current mixer output frame between callbacks.
	// Frame data is fp32, channels packed into vec128 per sample:
	// stride = ((nChannels + 3) >> 2) * 4 floats, L at [0], R at [1].
	const fp32 *m_pFrameRead;
	uint32 m_FrameSamplesLeft;
	uint32 m_FrameStride;

	// Diagnostics
	uint32 m_CbCount;
	uint32 m_CbUnderruns;

	static void SDLCALL AudioCallback(void *_pUserData, Uint8 *_pStream, int _Len);
	void AudioCallbackImpl(Uint8 *_pStream, int _Len);
#endif

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
