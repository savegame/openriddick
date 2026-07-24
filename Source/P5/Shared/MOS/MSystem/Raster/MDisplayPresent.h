#ifndef _INC_MDISPLAYPRESENT
#define _INC_MDISPLAYPRESENT

/*
	Present/rotation configuration shared between the SDL2 display
	(which owns the window and the screen FBO), the GLES3 renderer
	(which composites the FBO into the window) and the SDL2 input
	layer (which must translate window coordinates back into FBO
	coordinates).

	The engine only ever sees the FBO resolution (m_FBOW x m_FBOH);
	the physical window (m_WinW x m_WinH) is a presentation detail.
	m_Rotate is the clockwise rotation applied when compositing the
	FBO into the window: 0, 90, 180 or 270 degrees. At 90/270 the
	FBO dimensions are the window dimensions swapped (unless
	overridden), so portrait content fills a landscape window.
*/

struct SRiddickPresent
{
	int m_Rotate;		// 0 / 90 / 180 / 270 (clockwise on screen)
	int m_WinW, m_WinH;	// physical window size, pixels
	int m_FBOW, m_FBOH;	// logical engine-visible size, pixels

	SRiddickPresent()
	{
		m_Rotate = 0;
		m_WinW = 1280; m_WinH = 720;
		m_FBOW = 1280; m_FBOH = 720;
	}

	// Absolute window pixel (top-left origin) -> FBO pixel.
	void WindowToFBO(int _wx, int _wy, int& _fx, int& _fy) const;

	// Relative mouse delta in window space -> delta in FBO space
	// (inverse of the present rotation; scale ignored on purpose,
	// relative deltas feed look/cursor speed, not positions).
	void RotateDelta(int _dx, int _dy, int& _ox, int& _oy) const;
};

extern SRiddickPresent g_RiddickPresent;

#endif // _INC_MDISPLAYPRESENT
