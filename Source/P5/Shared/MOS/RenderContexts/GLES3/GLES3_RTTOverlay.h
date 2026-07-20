// Debug overlay: draws every live render-to-texture target as a grid
// of quads on top of the final composite (Phase 4 bring-up aid).
//
// Enabled with RIDDICK_DBG_RTT=1. Driven by CRC_GLES3::PresentToWindow
// right after the screen-FBO composite, while the default framebuffer
// is still bound. Self-contained: own shader program and quad VBO,
// no engine state touched beyond what PresentToWindow already clobbers.

#pragma once

#ifdef PLATFORM_LINUX

#include <GLES3/gl3.h>
#include "GLES3_Shader.h"

class CGLES3RTTOverlay
{
public:
	struct SEntry
	{
		GLuint m_Tex;   // GL texture to sample (FBO color attachment)
		int    m_TexID; // engine texture ID (for the stderr mapping)
		int    m_W, m_H;
	};

	CGLES3RTTOverlay();

	// Reads RIDDICK_DBG_RTT once. Call before first use.
	void InitFromEnv();
	bool Enabled() const { return m_Enabled; }

	// Draw the grid into the currently-bound framebuffer (must be the
	// window default framebuffer). Viewport is left at the last cell;
	// the caller restores its own target/viewport afterwards.
	// At most MAX_ENTRIES are drawn.
	void Render(int _WinW, int _WinH, const SEntry* _pEntries, int _nEntries);

	void Destroy();

	static const int MAX_ENTRIES = 64;

private:
	void EnsureGL();

	CGLES3Shader m_Shader;
	int    m_TexLoc;
	GLuint m_VBO;
	GLuint m_VAO;
	bool   m_Enabled;
	bool   m_bGLInited;
	int    m_LastMappingHash; // spam guard for the stderr cell mapping

	CGLES3RTTOverlay(const CGLES3RTTOverlay&);
	CGLES3RTTOverlay& operator=(const CGLES3RTTOverlay&);
};

#endif // PLATFORM_LINUX
