// SDL2 sound backend for the Linux port - milestone M0 (context stub).
// See Docs/Sound_SDL2.md. Creates a real CSoundContext_Mixer instance
// (CPU mixer graph included) but performs no audio output and no voice
// streaming yet: the Platform_* methods are no-ops, so all voices stay
// silent. SDL audio device output arrives in M1, voice streaming in M2.

#include "PCH.h"
#include "MSound_SDL2.h"

#ifdef PLATFORM_LINUX

MRTC_IMPLEMENT_DYNAMIC(CSoundContext_SDL2, CSoundContext_Mixer);

CSoundContext_SDL2::CSoundContext_SDL2()
{
}

CSoundContext_SDL2::~CSoundContext_SDL2()
{
}

void CSoundContext_SDL2::Create(CStr _Params)
{
	fprintf(stderr, "[SND-SDL2] CSoundContext_SDL2::Create (M0 stub, no audio output)\n");
	CSuper::Create(_Params);
}

void CSoundContext_SDL2::Platform_GetInfo(CPlatformInfo &_Info)
{
	// Fixed 48 kHz stereo, 256-sample mixer frames (same as the PS3 backend)
	_Info.m_nProssingThreads = 1;
	_Info.m_nChannels = 2;
	_Info.m_FrameLength = 256;
	_Info.m_SampleRate = 48000;
	_Info.m_Speakers[0].m_Position = CVec3Dfp32(-1.0f, -1.0f, 0.0f); // Front Left
	_Info.m_Speakers[1].m_Position = CVec3Dfp32(-1.0f, 1.0f, 0.0f); // Front Right
}

void CSoundContext_SDL2::Platform_Init(uint32 _MaxMixerVoices)
{
	fprintf(stderr, "[SND-SDL2] Platform_Init: %u mixer voices, %d channels, %.0f Hz (M0 stub)\n",
		_MaxMixerVoices, m_PlatformInfo.m_nChannels, m_PlatformInfo.m_SampleRate);
}

void CSoundContext_SDL2::Platform_StartStreamingToMixer(uint32 _MixerVoice, CVoice *_pVoice, uint32 _WaveID, fp32 _SampleRate)
{
	// M0: no wave data is submitted, the voice stays paused on
	// EPauseSlot_Delayed and produces silence.
}

void CSoundContext_SDL2::Platform_StopStreamingToMixer(uint32 _MixerVoice)
{
	// M0: nothing was started
}

void CSoundContext_SDL2::Refresh()
{
	// M0: no worker threads to pump
}

#endif // PLATFORM_LINUX
