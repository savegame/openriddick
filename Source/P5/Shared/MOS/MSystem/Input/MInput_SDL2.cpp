// SDL2 keyboard + mouse input backend for the Linux/SDL2 port.
//
// Update() pumps the SDL event queue every engine tick and feeds the
// CInputContextCore hooks:
//   * keyboard  -> DownKey(SKEY_*, wchar, time) / UpKey(SKEY_*, time, ...)
//   * mouse move-> DownKey(SKEY_MOUSEMOVEREL, 0, time, 0, dx, dy)
//   * buttons   -> DownKey/UpKey(SKEY_MOUSE1..8, ...)
//   * wheel     -> DownKey(SKEY_MOUSEWHEELUP/DOWN, ...)
//   * SDL_QUIT  -> exit(0) (game has no clean shutdown path yet)
//   * gamepad   -> SDL_GameController, mirroring the PS3 pad layout:
//     axes 0..3 = LX / LY(inverted) / RX / RY as SKEY_JOY_AXISnn_POS/NEG
//     pairs (value 0..255 after deadzone+power curve), buttons
//     0=A(cross) 1=B(circle) 2=X(square) 3=Y(triangle) 4=start 5=back
//     6=L3 7=R3 8=LB 9=RB 10=LT 11=RT, dpad = SKEY_JOY_POV00..03.

#include "PCH.h"
#include "MInputCore.h"
#include "MInputScankey.h"
#include "../Raster/MDisplayPresent.h"

#ifdef PLATFORM_LINUX

#include <SDL.h>

// -- SDL_Scancode -> engine SKEY_* table. -------------------------------
// SDL_Scancode values are up to ~SDL_NUM_SCANCODES (~512); we build a
// static lookup table sized at construction. Missing entries stay 0
// which the engine treats as "no key".
static int gSDLKeyToSKEY[SDL_NUM_SCANCODES];

static void BuildSDLKeyTable()
{
	static bool sBuilt = false;
	if (sBuilt) return;
	sBuilt = true;
	for (int i = 0; i < SDL_NUM_SCANCODES; ++i) gSDLKeyToSKEY[i] = 0;

	// Letters
	gSDLKeyToSKEY[SDL_SCANCODE_A] = SKEY_A;
	gSDLKeyToSKEY[SDL_SCANCODE_B] = SKEY_B;
	gSDLKeyToSKEY[SDL_SCANCODE_C] = SKEY_C;
	gSDLKeyToSKEY[SDL_SCANCODE_D] = SKEY_D;
	gSDLKeyToSKEY[SDL_SCANCODE_E] = SKEY_E;
	gSDLKeyToSKEY[SDL_SCANCODE_F] = SKEY_F;
	gSDLKeyToSKEY[SDL_SCANCODE_G] = SKEY_G;
	gSDLKeyToSKEY[SDL_SCANCODE_H] = SKEY_H;
	gSDLKeyToSKEY[SDL_SCANCODE_I] = SKEY_I;
	gSDLKeyToSKEY[SDL_SCANCODE_J] = SKEY_J;
	gSDLKeyToSKEY[SDL_SCANCODE_K] = SKEY_K;
	gSDLKeyToSKEY[SDL_SCANCODE_L] = SKEY_L;
	gSDLKeyToSKEY[SDL_SCANCODE_M] = SKEY_M;
	gSDLKeyToSKEY[SDL_SCANCODE_N] = SKEY_N;
	gSDLKeyToSKEY[SDL_SCANCODE_O] = SKEY_O;
	gSDLKeyToSKEY[SDL_SCANCODE_P] = SKEY_P;
	gSDLKeyToSKEY[SDL_SCANCODE_Q] = SKEY_Q;
	gSDLKeyToSKEY[SDL_SCANCODE_R] = SKEY_R;
	gSDLKeyToSKEY[SDL_SCANCODE_S] = SKEY_S;
	gSDLKeyToSKEY[SDL_SCANCODE_T] = SKEY_T;
	gSDLKeyToSKEY[SDL_SCANCODE_U] = SKEY_U;
	gSDLKeyToSKEY[SDL_SCANCODE_V] = SKEY_V;
	gSDLKeyToSKEY[SDL_SCANCODE_W] = SKEY_W;
	gSDLKeyToSKEY[SDL_SCANCODE_X] = SKEY_X;
	gSDLKeyToSKEY[SDL_SCANCODE_Y] = SKEY_Y;
	gSDLKeyToSKEY[SDL_SCANCODE_Z] = SKEY_Z;

	// Number row
	gSDLKeyToSKEY[SDL_SCANCODE_1] = SKEY_1;
	gSDLKeyToSKEY[SDL_SCANCODE_2] = SKEY_2;
	gSDLKeyToSKEY[SDL_SCANCODE_3] = SKEY_3;
	gSDLKeyToSKEY[SDL_SCANCODE_4] = SKEY_4;
	gSDLKeyToSKEY[SDL_SCANCODE_5] = SKEY_5;
	gSDLKeyToSKEY[SDL_SCANCODE_6] = SKEY_6;
	gSDLKeyToSKEY[SDL_SCANCODE_7] = SKEY_7;
	gSDLKeyToSKEY[SDL_SCANCODE_8] = SKEY_8;
	gSDLKeyToSKEY[SDL_SCANCODE_9] = SKEY_9;
	gSDLKeyToSKEY[SDL_SCANCODE_0] = SKEY_0;

	// Function keys
	gSDLKeyToSKEY[SDL_SCANCODE_F1]  = SKEY_F1;
	gSDLKeyToSKEY[SDL_SCANCODE_F2]  = SKEY_F2;
	gSDLKeyToSKEY[SDL_SCANCODE_F3]  = SKEY_F3;
	gSDLKeyToSKEY[SDL_SCANCODE_F4]  = SKEY_F4;
	gSDLKeyToSKEY[SDL_SCANCODE_F5]  = SKEY_F5;
	gSDLKeyToSKEY[SDL_SCANCODE_F6]  = SKEY_F6;
	gSDLKeyToSKEY[SDL_SCANCODE_F7]  = SKEY_F7;
	gSDLKeyToSKEY[SDL_SCANCODE_F8]  = SKEY_F8;
	gSDLKeyToSKEY[SDL_SCANCODE_F9]  = SKEY_F9;
	gSDLKeyToSKEY[SDL_SCANCODE_F10] = SKEY_F10;
	gSDLKeyToSKEY[SDL_SCANCODE_F11] = SKEY_F11;
	gSDLKeyToSKEY[SDL_SCANCODE_F12] = SKEY_F12;

	// Editing / navigation
	gSDLKeyToSKEY[SDL_SCANCODE_BACKSPACE] = SKEY_BACKSPACE;
	gSDLKeyToSKEY[SDL_SCANCODE_RETURN]    = SKEY_RETURN;
	gSDLKeyToSKEY[SDL_SCANCODE_KP_ENTER]  = SKEY_RETURN;
	gSDLKeyToSKEY[SDL_SCANCODE_TAB]       = SKEY_TAB;
	gSDLKeyToSKEY[SDL_SCANCODE_SPACE]     = SKEY_SPACE;
	gSDLKeyToSKEY[SDL_SCANCODE_ESCAPE]    = SKEY_ESC;

	gSDLKeyToSKEY[SDL_SCANCODE_LEFT]  = SKEY_CURSOR_LEFT;
	gSDLKeyToSKEY[SDL_SCANCODE_RIGHT] = SKEY_CURSOR_RIGHT;
	gSDLKeyToSKEY[SDL_SCANCODE_UP]    = SKEY_CURSOR_UP;
	gSDLKeyToSKEY[SDL_SCANCODE_DOWN]  = SKEY_CURSOR_DOWN;

	gSDLKeyToSKEY[SDL_SCANCODE_INSERT]   = SKEY_INSERT;
	gSDLKeyToSKEY[SDL_SCANCODE_DELETE]   = SKEY_DELETE;
	gSDLKeyToSKEY[SDL_SCANCODE_HOME]     = SKEY_HOME;
	gSDLKeyToSKEY[SDL_SCANCODE_END]      = SKEY_END;
	gSDLKeyToSKEY[SDL_SCANCODE_PAGEUP]   = SKEY_PAGEUP;
	gSDLKeyToSKEY[SDL_SCANCODE_PAGEDOWN] = SKEY_PAGEDOWN;

	gSDLKeyToSKEY[SDL_SCANCODE_LSHIFT] = SKEY_LEFT_SHIFT;
	gSDLKeyToSKEY[SDL_SCANCODE_RSHIFT] = SKEY_RIGHT_SHIFT;
	gSDLKeyToSKEY[SDL_SCANCODE_LCTRL]  = SKEY_LEFT_CONTROL;
	gSDLKeyToSKEY[SDL_SCANCODE_RCTRL]  = SKEY_RIGHT_CONTROL;
	gSDLKeyToSKEY[SDL_SCANCODE_LALT]   = SKEY_LEFT_ALT;
	gSDLKeyToSKEY[SDL_SCANCODE_RALT]   = SKEY_RIGHT_ALT;
	gSDLKeyToSKEY[SDL_SCANCODE_LGUI]   = SKEY_LWIN;
	gSDLKeyToSKEY[SDL_SCANCODE_RGUI]   = SKEY_RWIN;
	gSDLKeyToSKEY[SDL_SCANCODE_CAPSLOCK] = SKEY_CAPSLOCK;
}

// Same response curve as the PS3 pad backend (MInput_PS3.cpp):
// 10% deadzone, ^1.5 power, scaled to 0..255.
static int PadTranslateAxis(int _Data0to65535)
{
	float fAxis = ((float)_Data0to65535 / 65535.0f) * 2.0f - 1.0f;
	const float SignV = (fAxis < 0.0f) ? -1.0f : 1.0f;
	fAxis = fabsf(fAxis);
	fAxis = (fAxis > 0.10f) ? (fAxis - 0.10f) / 0.90f : 0.0f;
	if (fAxis > 0.0f) fAxis = expf(logf(fAxis) * 1.5f);
	const float v = fAxis * SignV * 255.0f;
	return (int)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

class CInputContext_SDL2 : public CInputContextCore
{
	MRTC_DECLARE;
public:
	SDL_GameController* m_pPad;
	SDL_JoystickID m_PadID;

	CInputContext_SDL2()
	{
		m_pPad = NULL;
		m_PadID = -1;
	}

	~CInputContext_SDL2()
	{
		PadClose();
	}

	void PadClose()
	{
		if (m_pPad)
		{
			SDL_GameControllerClose(m_pPad);
			m_pPad = NULL;
			m_PadID = -1;
		}
	}

	void PadOpen(int _DeviceIndex)
	{
		if (m_pPad) return;
		SDL_GameController* p = SDL_GameControllerOpen(_DeviceIndex);
		if (!p) return;
		m_pPad = p;
		m_PadID = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p));
		ConOutL(CStrF("(CInputContext_SDL2) gamepad: %s", SDL_GameControllerName(p)));
	}

	// One engine axis: POS scancode at base+i*2, NEG at base+i*2+1,
	// exactly the PS3 OnAxisData event pattern.
	void PadFeedAxis(int _iAxis, int _Data0to65535)
	{
		int Vec = PadTranslateAxis(_Data0to65535);
		const int ScanPos = SKEY_JOY_START + _iAxis * 2;
		const int ScanNeg = ScanPos + 1;
		if (Vec < 0)
		{
			UpKey(ScanPos, 0, 0, 0, 0);
			Vec = -Vec;
			if (Vec > 128)
				DownKey(ScanNeg, 0, 0, 1, Vec, 0, 0);
			else
				UpKey(ScanNeg, 0, Vec, 0, 0);
		}
		else
		{
			UpKey(ScanNeg, 0, 0, 0, 0);
			if (Vec > 128)
				DownKey(ScanPos, 0, 0, 1, Vec, 0, 0);
			else
				UpKey(ScanPos, 0, Vec, 0, 0);
		}
	}

	void PadFeedButton(int _iButton, int _Data0to255)
	{
		if (_Data0to255 >= 128)
			DownKey(SKEY_JOY_BUTTON00 + _iButton, 0, 0, 1, _Data0to255, 0, 0);
		else
			UpKey(SKEY_JOY_BUTTON00 + _iButton, 0, _Data0to255, 0, 0);
	}

	void PadFeedPOV(int _iDir, bool _bDown)
	{
		if (_bDown)
			DownKey(SKEY_JOY_POV00 + _iDir, 0, 0, 1, 255, 0, 0);
		else
			UpKey(SKEY_JOY_POV00 + _iDir, 0, 0, 0, 0);
	}

	// SDL controller button -> engine button index (PS3 layout; see
	// the Lookup table in MInput_PS3.cpp OnButtonData). -1 = ignore,
	// -2..-5 = dpad (POV 0..3).
	static int PadMapButton(Uint8 _SDLButton)
	{
		switch (_SDLButton)
		{
		case SDL_CONTROLLER_BUTTON_A:             return 0;  // cross
		case SDL_CONTROLLER_BUTTON_B:             return 1;  // circle
		case SDL_CONTROLLER_BUTTON_X:             return 2;  // square
		case SDL_CONTROLLER_BUTTON_Y:             return 3;  // triangle
		case SDL_CONTROLLER_BUTTON_START:         return 4;
		case SDL_CONTROLLER_BUTTON_BACK:          return 5;  // select
		case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return 6;  // L3
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return 7;  // R3
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return 8;  // L1
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return 9;  // R1
		case SDL_CONTROLLER_BUTTON_DPAD_UP:       return -2;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return -3;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return -4;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return -5;
		default:                                  return -1;
		}
	}

	virtual void Create(const char* _pParams)
	{
		BuildSDLKeyTable();
		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0)
		{
			for (int i = 0; i < SDL_NumJoysticks(); ++i)
				if (SDL_IsGameController(i))
				{
					PadOpen(i);
					break;
				}
		}
		CInputContextCore::Create(_pParams);
	}

	virtual void Update()
	{
		// Drain SDL event queue and translate to engine input calls.
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				// No clean shutdown path in the engine yet; bail.
				exit(0);
				break;

			case SDL_KEYDOWN:
				if (e.key.repeat) break; // engine handles repeat itself
				{
					const int sk = gSDLKeyToSKEY[e.key.keysym.scancode];
					if (sk) DownKey(sk, 0, 0.0, 1, 0, 0, 0);
				}
				break;

			case SDL_KEYUP:
				{
					const int sk = gSDLKeyToSKEY[e.key.keysym.scancode];
					if (sk) UpKey(sk, 0.0, 0, 0, 0);
				}
				break;

			case SDL_TEXTINPUT:
				// SDL delivers UTF-8; take the first codepoint's low
				// byte for the wchar slot (game text input is Latin/
				// Cyrillic 8-bit-friendly for these menus).
				if (e.text.text[0])
				{
					const wchar ch = (wchar)(unsigned char)e.text.text[0];
					DownKey(0, ch, 0.0, 1, 0, 0, 0);
				}
				break;

			case SDL_MOUSEMOTION:
				if (e.motion.xrel || e.motion.yrel)
				{
					// Rotate the delta into FBO space when the presented
					// image is rotated (see MDisplayPresent.h).
					int dx, dy;
					g_RiddickPresent.RotateDelta(e.motion.xrel, e.motion.yrel, dx, dy);
					DownKey(SKEY_MOUSEMOVEREL, 0, 0.0, 1, dx, dy, 0);
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					int sk = 0;
					switch (e.button.button)
					{
					case SDL_BUTTON_LEFT:   sk = SKEY_MOUSE1; break;
					case SDL_BUTTON_RIGHT:  sk = SKEY_MOUSE2; break;
					case SDL_BUTTON_MIDDLE: sk = SKEY_MOUSE3; break;
					case SDL_BUTTON_X1:     sk = SKEY_MOUSE4; break;
					case SDL_BUTTON_X2:     sk = SKEY_MOUSE5; break;
					}
					if (sk)
					{
						if (e.type == SDL_MOUSEBUTTONDOWN)
							DownKey(sk, 0, 0.0, 1, 0, 0, 0);
						else
							UpKey(sk, 0.0, 0, 0, 0);
					}
				}
				break;

			case SDL_MOUSEWHEEL:
				{
					int sk = 0;
					if      (e.wheel.y > 0) sk = SKEY_MOUSEWHEELUP;
					else if (e.wheel.y < 0) sk = SKEY_MOUSEWHEELDOWN;
					if (sk) DownKey(sk, 0, 0.0, 1, 0, 0, 0);
				}
				break;

			case SDL_CONTROLLERDEVICEADDED:
				PadOpen(e.cdevice.which);
				break;

			case SDL_CONTROLLERDEVICEREMOVED:
				if (e.cdevice.which == m_PadID)
					PadClose();
				break;

			case SDL_CONTROLLERAXISMOTION:
				if (e.caxis.which == m_PadID)
				{
					const int Raw = (int)e.caxis.value + 32768; // 0..65535
					switch (e.caxis.axis)
					{
					case SDL_CONTROLLER_AXIS_LEFTX:  PadFeedAxis(0, Raw); break;
					// PS3 backend inverts left Y (65535 - y); mirror it so
					// the profile's axis bindings behave identically.
					case SDL_CONTROLLER_AXIS_LEFTY:  PadFeedAxis(1, 65535 - Raw); break;
					case SDL_CONTROLLER_AXIS_RIGHTX: PadFeedAxis(2, Raw); break;
					case SDL_CONTROLLER_AXIS_RIGHTY: PadFeedAxis(3, Raw); break;
					// Triggers are analogue buttons 10/11 on PS3 (L2/R2).
					case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
						PadFeedButton(10, (int)e.caxis.value * 255 / 32767); break;
					case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
						PadFeedButton(11, (int)e.caxis.value * 255 / 32767); break;
					}
				}
				break;

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				if (e.cbutton.which == m_PadID)
				{
					const bool bDown = (e.type == SDL_CONTROLLERBUTTONDOWN);
					const int Map = PadMapButton(e.cbutton.button);
					if (Map >= 0)
						PadFeedButton(Map, bDown ? 255 : 0);
					else if (Map <= -2)
						PadFeedPOV(-Map - 2, bDown);
				}
				break;

			default:
				break;
			}
		}

		CInputContextCore::Update();
	}
};

MRTC_IMPLEMENT_DYNAMIC(CInputContext_SDL2, CInputContextCore);

#endif // PLATFORM_LINUX
