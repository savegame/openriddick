// SDL2 sound backend for the Linux port.
// See Docs/Sound_SDL2.md. Creates a real CSoundContext_Mixer instance
// (CPU mixer graph included).
// M0: context stub - Platform_* no-ops, all voices silent.
// M1: SDL audio device + master output. The SDL callback pulls finished
//     mixer frames via CSC_Mixer::StartNewFrame() and copies interleaved
//     stereo fp32 to the SDL stream. Voice streaming arrives in M2, so
//     the output is mixer silence for now.

#include "PCH.h"
#include "MSound_SDL2.h"

#ifdef PLATFORM_LINUX

MRTC_IMPLEMENT_DYNAMIC(CSoundContext_SDL2, CSoundContext_Mixer);

CSoundContext_SDL2::CSoundContext_SDL2()
{
	m_AudioDevice = 0;
	m_bSDLInit = false;
	m_bMixerReady = false;
	m_bFormatOK = false;
	m_pFrameRead = NULL;
	m_FrameSamplesLeft = 0;
	m_FrameStride = 0;
	m_CbCount = 0;
	m_CbUnderruns = 0;
}

CSoundContext_SDL2::~CSoundContext_SDL2()
{
	// Stop the callback before the mixer base class goes away
	m_bMixerReady = false;
	if (m_AudioDevice)
	{
		SDL_CloseAudioDevice(m_AudioDevice); // waits for a running callback
		m_AudioDevice = 0;
	}
	if (m_bSDLInit)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		m_bSDLInit = false;
	}
}

void CSoundContext_SDL2::Create(CStr _Params)
{
	fprintf(stderr, "[SND-SDL2] CSoundContext_SDL2::Create\n");
	CSuper::Create(_Params);

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
	{
		m_bSDLInit = true;
		fprintf(stderr, "[SND-SDL2] SDL_InitSubSystem(AUDIO) ok\n");
	}
	else
	{
		fprintf(stderr, "[SND-SDL2] SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
	}
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
	fprintf(stderr, "[SND-SDL2] Platform_Init: %u mixer voices, %d channels, %.0f Hz\n",
		_MaxMixerVoices, m_PlatformInfo.m_nChannels, m_PlatformInfo.m_SampleRate);

	if (!m_bSDLInit)
		return;

	// The mixer is already created here (base Init calls Platform_Init
	// after m_Mixer.Create), so the callback may start pulling frames
	// right after unpause.
	SDL_AudioSpec Want, Have;
	memset(&Want, 0, sizeof(Want));
	memset(&Have, 0, sizeof(Have));
	Want.freq = (int)m_PlatformInfo.m_SampleRate;
	Want.format = AUDIO_F32SYS;			// mixer outputs fp32
	Want.channels = (Uint8)m_PlatformInfo.m_nChannels;
	Want.samples = 512;					// two 256-sample mixer frames
	Want.callback = &CSoundContext_SDL2::AudioCallback;
	Want.userdata = this;

	m_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &Want, &Have, 0);
	if (!m_AudioDevice)
	{
		fprintf(stderr, "[SND-SDL2] SDL_OpenAudioDevice failed: %s (running silent)\n", SDL_GetError());
		return;
	}

	fprintf(stderr, "[SND-SDL2] device opened: freq=%d format=0x%x channels=%d samples=%d\n",
		Have.freq, Have.format, Have.channels, Have.samples);

	m_bFormatOK = (Have.format == AUDIO_F32SYS && Have.channels == m_PlatformInfo.m_nChannels);
	if (!m_bFormatOK)
		fprintf(stderr, "[SND-SDL2] unsupported device format, output will be silence\n");

	m_bMixerReady = true;
	SDL_PauseAudioDevice(m_AudioDevice, 0);
}

void SDLCALL CSoundContext_SDL2::AudioCallback(void *_pUserData, Uint8 *_pStream, int _Len)
{
	((CSoundContext_SDL2 *)_pUserData)->AudioCallbackImpl(_pStream, _Len);
}

void CSoundContext_SDL2::AudioCallbackImpl(Uint8 *_pStream, int _Len)
{
	++m_CbCount;

	if (!m_bMixerReady || !m_bFormatOK)
	{
		memset(_pStream, 0, _Len);
	}
	else
	{
		fp32 *pOut = (fp32 *)_pStream;
		uint32 nSamplesWanted = _Len / (2 * sizeof(fp32));
		uint32 Written = 0;

		while (Written < nSamplesWanted)
		{
			if (!m_FrameSamplesLeft)
			{
				// Lock-free (atomic exchange + event signal), same as the
				// PS3 audio thread - safe to call from the SDL audio thread.
				CSC_Mixer_OuputFrame *pFrame = m_Mixer.StartNewFrame();
				if (pFrame && pFrame->m_nSamples)
				{
					m_pFrameRead = (const fp32 *)pFrame->m_pData;
					m_FrameSamplesLeft = pFrame->m_nSamples;
					m_FrameStride = ((pFrame->m_nChannels + 3) >> 2) * 4;
				}
				else
				{
					// No finished frame yet - fill the rest with silence
					++m_CbUnderruns;
					memset(pOut, 0, (nSamplesWanted - Written) * 2 * sizeof(fp32));
					break;
				}
			}

			uint32 nCopy = Min(m_FrameSamplesLeft, nSamplesWanted - Written);
			for (uint32 i = 0; i < nCopy; ++i)
			{
				pOut[0] = m_pFrameRead[0]; // L
				pOut[1] = m_pFrameRead[1]; // R
				pOut += 2;
				m_pFrameRead += m_FrameStride;
			}
			Written += nCopy;
			m_FrameSamplesLeft -= nCopy;
		}
	}

	// Sparse diagnostics: first 3 callbacks, then every 3000th (~32 s)
	if (m_CbCount <= 3 || (m_CbCount % 3000) == 0)
		fprintf(stderr, "[SND-SDL2] cb #%u len=%d underruns=%u\n", m_CbCount, _Len, m_CbUnderruns);
}

void CSoundContext_SDL2::Platform_StartStreamingToMixer(uint32 _MixerVoice, CVoice *_pVoice, uint32 _WaveID, fp32 _SampleRate)
{
	// M1: no wave data is submitted, the voice stays paused on
	// EPauseSlot_Delayed and produces silence. (M2)
}

void CSoundContext_SDL2::Platform_StopStreamingToMixer(uint32 _MixerVoice)
{
	// M1: nothing was started
}

void CSoundContext_SDL2::Refresh()
{
	// M1: no worker threads to pump
}

#endif // PLATFORM_LINUX
