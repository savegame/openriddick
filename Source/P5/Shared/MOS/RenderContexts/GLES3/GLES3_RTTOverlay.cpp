// Debug overlay: grid of live RTT targets. See header for details.

#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_RTTOverlay.h"

#include <stdio.h>
#include <stdlib.h>

// Same convention as the confirmed-correct screen composite: uv =
// pos*0.5+0.5, no flip (RTT FBO content shares the screen FBO's
// GL orientation).
static const char* kOverlayVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec2 aPos;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  gl_Position = vec4(aPos, 0.0, 1.0);\n"
	"  vUV = aPos * 0.5 + 0.5;\n"
	"}\n";

static const char* kOverlayFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"uniform sampler2D uTex;\n"
	"out vec4 oColor;\n"
	"void main(){ oColor = vec4(texture(uTex, vUV).rgb, 1.0); }\n";

CGLES3RTTOverlay::CGLES3RTTOverlay()
	: m_TexLoc(-1), m_VBO(0), m_VAO(0),
	  m_Enabled(false), m_bGLInited(false), m_LastMappingHash(0)
{
}

void CGLES3RTTOverlay::InitFromEnv()
{
	const char* e = getenv("RIDDICK_DBG_RTT");
	m_Enabled = (e && *e && *e != '0');
}

void CGLES3RTTOverlay::EnsureGL()
{
	if (m_bGLInited) return;
	m_bGLInited = true;
	if (m_Shader.Build(kOverlayVertSrc, kOverlayFragSrc, "RTTOverlay"))
		m_TexLoc = m_Shader.UniformLocation("uTex");
	static const float sQuad[8] = { -1,-1,  1,-1,  -1,1,  1,1 };
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);
	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(sQuad), sQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CGLES3RTTOverlay::Render(int _WinW, int _WinH, const SEntry* _pEntries, int _nEntries)
{
	if (!m_Enabled || _nEntries <= 0 || _WinW <= 0 || _WinH <= 0) return;
	EnsureGL();
	if (!m_Shader.IsValid()) return;
	if (_nEntries > MAX_ENTRIES) _nEntries = MAX_ENTRIES;

	// Grid layout: near-square, columns first. The whole grid lives in
	// the left quarter of the window so the underlying frame stays
	// visible next to it.
	int Cols = 1;
	while (Cols * Cols < _nEntries) ++Cols;
	const int Rows = (_nEntries + Cols - 1) / Cols;
	const int AreaW = _WinW / 4;
	const int CellW = AreaW / Cols;
	const int CellH = _WinH / Rows;
	if (CellW <= 0 || CellH <= 0) return;

	// Cell mapping to stderr when the set changes (texture IDs can
	// only be told apart by position otherwise).
	int Hash = _nEntries;
	for (int i = 0; i < _nEntries; ++i)
		Hash = Hash * 31 + _pEntries[i].m_TexID;
	if (Hash != m_LastMappingHash)
	{
		m_LastMappingHash = Hash;
		fprintf(stderr, "[GLES3-RTT-OVL] grid %dx%d:\n", Cols, Rows);
		for (int i = 0; i < _nEntries; ++i)
			fprintf(stderr, "  cell %d (row %d col %d): id=%d %dx%d\n",
				i, i / Cols, i % Cols,
				_pEntries[i].m_TexID, _pEntries[i].m_W, _pEntries[i].m_H);
		fflush(stderr);
	}

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	m_Shader.Use();
	glUniform1i(m_TexLoc, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(m_VAO);

	for (int i = 0; i < _nEntries; ++i)
	{
		const int Col = i % Cols;
		const int Row = i / Cols;
		// Row 0 at the top of the window (GL viewport Y is bottom-up).
		glViewport(Col * CellW, _WinH - (Row + 1) * CellH, CellW, CellH);
		glBindTexture(GL_TEXTURE_2D, _pEntries[i].m_Tex);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindVertexArray(0);
}

void CGLES3RTTOverlay::Destroy()
{
	m_Shader.Destroy();
	if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
	if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
	m_bGLInited = false;
}

#endif // PLATFORM_LINUX
