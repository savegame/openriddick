
/*
	SDL2 display context + GLES3 render context skeleton (phases 3-4).

	Current state: opens an SDL2 window with a GLES 3.0 context, clears
	and swaps on PageFlip. The CRC_GLES3 render methods are still stubs
	inherited from the NULL bring-up renderer; attrib translation, VBO
	streaming, textures and the shader generator land here next.
	If SDL/GL init fails (e.g. headless), it degrades to NULL behaviour.
*/

#include "PCH.h"

#include "../../MSystem/MSystem.h"
#include "../../MSystem/MSystem_Core.h"
#include "../../MSystem/Raster/MRCCore.h"
#include "../../MSystem/Raster/MDisplayPresent.h"
#include "../../MSystem/Raster/MTexture.h"
#include "../../MSystem/Raster/MTextureContainers.h"

#include "GLES3_Texture.h"
#include "GLES3_Shader.h"
#include "GLES3_VBOStreamer.h"
#include "GLES3_RTTOverlay.h"

#include <cstdlib>
#include <cstdint>
#include <cstring>

// One shared shader for M3 UI/frontend drawing. Attributes: aPos
// (vec3 world), aUV (vec2), aCol (vec4, unpacked from CPixel32 BGRA).
// Uniforms: uMVP (mat4), uUseTexture (bool), uTex (sampler2D).
static const char* kGLES3_UIVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=2) in vec4 aCol;\n"
	"layout(location=3) in vec2 aUV1;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat4 uTexMat;\n"
	"uniform mat4 uTexMat1;\n"
	"out vec2 vUV;\n"
	"out vec2 vUV1;\n"
	"out vec4 vCol;\n"
	"out float vDepth;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	"  vUV = (uTexMat * vec4(aUV, 0.0, 1.0)).xy;\n"
	"  vUV1 = (uTexMat1 * vec4(aUV1, 0.0, 1.0)).xy;\n"
	"  vDepth = gl_Position.w;\n"
	"  vCol = aCol;\n"
	"}\n";

static const char* kGLES3_UIFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec2 vUV1;\n"
	"in vec4 vCol;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uUseTexture;\n"
	// Secondary texture (channel 1: lightmaps etc.) -- modulates RGB.
	"uniform sampler2D uTex1;\n"
	"uniform int uUseTexture1;\n"
	// Debug modes for RIDDICK_DBG_SHADER: 0=normal, 1=UV as
	// RGB (see quad UVs), 2=solid red (see quad positions),
	// 3=vertex-color only (ignore texture).
	"uniform int uDbgMode;\n"
	// Alpha test: CRC_COMPARE_* code (1=never..8=always, 0=off) + ref
	"uniform int uAlphaFunc;\n"
	"uniform float uAlphaRef;\n"
	// Linear fog (CRC_FLAGS_FOG): mix to uFogColor by view depth
	"uniform int uFogEnable;\n"
	"uniform vec3 uFogColor;\n"
	"uniform float uFogStart;\n"
	"uniform float uFogEnd;\n"
	"in float vDepth;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  if (uDbgMode == 1) { oColor = vec4(vUV.x, vUV.y, 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 2) { oColor = vec4(1.0, 0.0, 0.0, 0.5); return; }\n"
	"  if (uDbgMode == 3) { oColor = vCol; return; }\n"
	"  vec4 c = vCol;\n"
	"  if (uUseTexture != 0) c *= texture(uTex, vUV);\n"
	"  if (uUseTexture1 != 0) c.rgb *= texture(uTex1, vUV1).rgb;\n"
	"  if (uAlphaFunc != 0 && uAlphaFunc != 8) {\n"
	"    bool pass = true;\n"
	"    if      (uAlphaFunc == 1) pass = false;\n"
	"    else if (uAlphaFunc == 2) pass = (c.a <  uAlphaRef);\n"
	"    else if (uAlphaFunc == 3) pass = (c.a == uAlphaRef);\n"
	"    else if (uAlphaFunc == 4) pass = (c.a <= uAlphaRef);\n"
	"    else if (uAlphaFunc == 5) pass = (c.a >  uAlphaRef);\n"
	"    else if (uAlphaFunc == 6) pass = (c.a != uAlphaRef);\n"
	"    else if (uAlphaFunc == 7) pass = (c.a >= uAlphaRef);\n"
	"    if (!pass) discard;\n"
	"  }\n"
	"  if (uFogEnable != 0) {\n"
	"    float f = clamp((uFogEnd - vDepth) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);\n"
	"    c.rgb = mix(uFogColor, c.rgb, f);\n"
	"  }\n"
	"  oColor = c;\n"
	"}\n";

#ifdef PLATFORM_LINUX

#include <SDL.h>
#include <GLES3/gl3.h>

// --- CRC_* -> GL enum tables (M1). -------------------------------------
static GLenum GLES3_MapBlend(int _CRCBlend)
{
	switch (_CRCBlend)
	{
	case CRC_BLEND_ZERO:         return GL_ZERO;
	case CRC_BLEND_ONE:          return GL_ONE;
	case CRC_BLEND_SRCCOLOR:     return GL_SRC_COLOR;
	case CRC_BLEND_INVSRCCOLOR:  return GL_ONE_MINUS_SRC_COLOR;
	case CRC_BLEND_SRCALPHA:     return GL_SRC_ALPHA;
	case CRC_BLEND_INVSRCALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
	case CRC_BLEND_DESTALPHA:    return GL_DST_ALPHA;
	case CRC_BLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
	case CRC_BLEND_DESTCOLOR:    return GL_DST_COLOR;
	case CRC_BLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
	case CRC_BLEND_SRCALPHASAT:  return GL_SRC_ALPHA_SATURATE;
	default:                     return GL_ONE;
	}
}

static GLenum GLES3_MapCompare(int _CRCCompare)
{
	switch (_CRCCompare)
	{
	case CRC_COMPARE_NEVER:        return GL_NEVER;
	case CRC_COMPARE_LESS:         return GL_LESS;
	case CRC_COMPARE_EQUAL:        return GL_EQUAL;
	case CRC_COMPARE_LESSEQUAL:    return GL_LEQUAL;
	case CRC_COMPARE_GREATER:      return GL_GREATER;
	case CRC_COMPARE_NOTEQUAL:     return GL_NOTEQUAL;
	case CRC_COMPARE_GREATEREQUAL: return GL_GEQUAL;
	case CRC_COMPARE_ALWAYS:       return GL_ALWAYS;
	default:                       return GL_LEQUAL;
	}
}

// Composite pass: fullscreen quad sampling the screen FBO with an
// optional 90-degree-step rotation (uRot = rotation/90).
static const char* kGLES3_CompVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec2 aPos;\n"
	"uniform int uRot;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  gl_Position = vec4(aPos, 0.0, 1.0);\n"
	"  vec2 uv = aPos * 0.5 + 0.5;\n"
	"  if      (uRot == 1) vUV = vec2(uv.y, 1.0 - uv.x);\n"
	"  else if (uRot == 2) vUV = vec2(1.0 - uv.x, 1.0 - uv.y);\n"
	"  else if (uRot == 3) vUV = vec2(1.0 - uv.y, uv.x);\n"
	"  else                vUV = uv;\n"
	"}\n";

static const char* kGLES3_CompFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"uniform sampler2D uTex;\n"
	"out vec4 oColor;\n"
	"void main(){ oColor = vec4(texture(uTex, vUV).rgb, 1.0); }\n";

// The active CRC_GLES3 instance (single renderer per process); lets
// CDisplayContextSDL2::PageFlip run the composite pass without a typed
// member (the class is nested below).
static void* g_pGLES3RCInst = 0;

class CDisplayContextSDL2 : public CDisplayContext
{
protected:
	MRTC_DECLARE;
public:

	CImage m_Image;
	SDL_Window* m_pWindow;
	SDL_GLContext m_GLContext;
	// m_Width/m_Height is the LOGICAL (engine-visible, FBO) resolution;
	// the physical window is m_WinWidth x m_WinHeight. They differ when
	// -rotate 90/270 (swapped) or -fbosize is given.
	int m_Width, m_Height;
	int m_WinWidth, m_WinHeight;
	int m_Rotate;

	static bool ParseSizeArg(const char* _p, int& _W, int& _H)
	{
		if (!_p) return false;
		int w = 0, h = 0;
		if (sscanf(_p, "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
		_W = w; _H = h;
		return true;
	}

	CDisplayContextSDL2()
	{
		m_pWindow = NULL;
		m_GLContext = NULL;
		m_WinWidth = 1280;
		m_WinHeight = 720;
		ParseSizeArg(getenv("RIDDICK_WINSIZE"), m_WinWidth, m_WinHeight);
		m_Rotate = 0;
		if (const char* r = getenv("RIDDICK_ROTATE"))
		{
			const int v = atoi(r);
			if (v == 90 || v == 180 || v == 270) m_Rotate = v;
		}
		// Logical size defaults to the window size, swapped at 90/270.
		if (m_Rotate == 90 || m_Rotate == 270)
			{ m_Width = m_WinHeight; m_Height = m_WinWidth; }
		else
			{ m_Width = m_WinWidth; m_Height = m_WinHeight; }
		ParseSizeArg(getenv("RIDDICK_FBOSIZE"), m_Width, m_Height);

		g_RiddickPresent.m_Rotate = m_Rotate;
		g_RiddickPresent.m_WinW = m_WinWidth;  g_RiddickPresent.m_WinH = m_WinHeight;
		g_RiddickPresent.m_FBOW = m_Width;     g_RiddickPresent.m_FBOH = m_Height;

		m_Image.Create(m_Width, m_Height, IMAGE_FORMAT_BGRA8, IMAGE_MEM_IMAGE);
	}

	~CDisplayContextSDL2()
	{
		if (m_GLContext) SDL_GL_DeleteContext(m_GLContext);
		if (m_pWindow) SDL_DestroyWindow(m_pWindow);
	}

	virtual CPnt GetScreenSize(){return CPnt(m_Width, m_Height);};
	virtual CPnt GetMaxWindowSize(){return CPnt(m_Width, m_Height);};

	bool InitWindow()
	{
		if (m_pWindow)
			return true;
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_Init failed: %s - running headless", SDL_GetError()));
			return false;
		}
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		m_pWindow = SDL_CreateWindow("OpenRiddick",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			m_WinWidth, m_WinHeight, SDL_WINDOW_OPENGL);
		if (!m_pWindow)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_CreateWindow failed: %s - running headless", SDL_GetError()));
			return false;
		}
		m_GLContext = SDL_GL_CreateContext(m_pWindow);
		if (!m_GLContext)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_GL_CreateContext failed: %s - running headless", SDL_GetError()));
			SDL_DestroyWindow(m_pWindow);
			m_pWindow = NULL;
			return false;
		}
		SDL_GL_SetSwapInterval(1);
		ConOutL(CStrF("(CDisplayContextSDL2) GL_VERSION: %s", (const char*)glGetString(GL_VERSION)));
		ConOutL(CStrF("(CDisplayContextSDL2) window %dx%d, logical (FBO) %dx%d, rotate %d",
			m_WinWidth, m_WinHeight, m_Width, m_Height, m_Rotate));
		return true;
	}

	virtual void Create()
	{
		CDisplayContext::Create();
		InitWindow();
	}

	virtual int PageFlip()
	{
		if (m_pWindow)
		{
			// SDL event pump lives in CInputContext_SDL2::Update now --
			// draining SDL_PollEvent here too would split events
			// between the two consumers.
			// Composite the screen FBO into the window (with rotation)
			// before swapping; see CRC_GLES3::PresentToWindow.
			if (g_pGLES3RCInst)
				((CRC_GLES3*)g_pGLES3RCInst)->PresentToWindow();
			SDL_GL_SwapWindow(m_pWindow);
			// The engine now drives its own clears via
			// CRC_GLES3::RenderTarget_Clear; the bring-up glClear here
			// used to hide unrendered frames but would fight the engine
			// once real drawing lands.
			if (m_spRenderContext)
				m_spRenderContext->DbgFramePrint();
		}
		return CDisplayContext::PageFlip();
	}

	virtual void SetMode(int nr)
	{
	}

	virtual void ModeList_Init()
	{
	}

	virtual int SpawnWindow(int _Flags = 0)
	{
		return 0;
	}

	virtual void DeleteWindow(int _iWnd)
	{
	}

	virtual void SelectWindow(int _iWnd)
	{
	}

	virtual void SetWindowPosition(int _iWnd, CRct _Rct)
	{
	}

	virtual void SetPalette(spCImagePalette _spPal)
	{
	}

	virtual CImage* GetFrameBuffer()
	{
		return &m_Image;
	}

	virtual void ClearFrameBuffer(int _Buffers = (CDC_CLEAR_COLOR | CDC_CLEAR_ZBUFFER | CDC_CLEAR_STENCIL), int _Color = 0)
	{
	}

	int GetMode()
	{
		return 0;
	}


	class CRC_GLES3 : public CRC_Core
	{
	public:

		CDisplayContextSDL2 *m_pDisplayContext;
		int m_iTC = -1;
		int m_iVBCtxRC = -1;


		void Internal_RenderPolygon(int _nV, const CVec3Dfp32* _pV, const CVec3Dfp32* _pN, const CVec4Dfp32* _pCol = NULL, const CVec4Dfp32* _pSpec = NULL, /*const fp32* _pFog = NULL,*/
									const CVec4Dfp32* _pTV0 = NULL, const CVec4Dfp32* _pTV1 = NULL, const CVec4Dfp32* _pTV2 = NULL, const CVec4Dfp32* _pTV3 = NULL, int _Color = 0xffffffff)
		{
		}

		// M1 cached GL state / matrix capture. Uploaded to shader
		// programs by M3 draw calls.
		CMat4Dfp32 m_ProjMat;
		CMat4Dfp32 m_ModelMat;
		CMat4Dfp32 m_TexMat[4];

		// M2: engine TextureID -> GLuint. Sparse; 0 means "not
		// uploaded yet"; the vector grows on first touch.
		TArray<GLuint> m_lGLTex;

		// 1x1 magenta placeholder handed back for texture IDs the
		// texture context can't produce (typically CTextureContainer_
		// Screen / RTT slots -- we don't render-to-texture yet, so
		// the engine samples "nothing"; give it a loud debug colour
		// so missing-RTT surfaces are obvious rather than invisible.
		GLuint m_PlaceholderTex;

		GLuint GetPlaceholderTex()
		{
			if (m_PlaceholderTex) return m_PlaceholderTex;
			glGenTextures(1, &m_PlaceholderTex);
			glBindTexture(GL_TEXTURE_2D, m_PlaceholderTex);
			// RED, not magenta, so it visually distinguishes from
			// "no texture bound at all" (which shows vCol=white/black).
			const unsigned char Magenta[4] = { 255, 0, 0, 255 };
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, Magenta);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
			return m_PlaceholderTex;
		}

		// Render-to-texture support. The engine allocates a set of
		// texture IDs from CTextureContainer_Screen; those IDs have no
		// data on disk -- we're supposed to render into them, then the
		// engine samples them as normal textures for compositing.
		// One SFBOSlot per RTT texture ID: colorTex is what samplers
		// see, depthRbo is a bundled depth+stencil renderbuffer, fbo
		// binds them together.
		struct SFBOSlot
		{
			GLuint m_FBO;
			GLuint m_ColorTex;
			GLuint m_DepthRbo;
			int    m_Width, m_Height;
		};
		TArray<SFBOSlot> m_lFBO; // sparse, indexed by texture id

		SFBOSlot* GetFBOSlot(int _TextureID)
		{
			if (_TextureID <= 0) return 0;
			if (_TextureID >= m_lFBO.Len()) return 0;
			SFBOSlot* p = &m_lFBO[_TextureID];
			return p->m_FBO ? p : 0;
		}

		SFBOSlot* EnsureFBOFor(int _TextureID)
		{
			if (_TextureID <= 0 || !m_pTC) return 0;
			if (_TextureID >= m_lFBO.Len())
			{
				const int Old = m_lFBO.Len();
				m_lFBO.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lFBO.Len(); ++i)
				{
					m_lFBO[i].m_FBO = 0; m_lFBO[i].m_ColorTex = 0;
					m_lFBO[i].m_DepthRbo = 0;
					m_lFBO[i].m_Width = m_lFBO[i].m_Height = 0;
				}
			}
			SFBOSlot& S = m_lFBO[_TextureID];
			if (S.m_FBO) return &S;

			CImage Desc;
			int nMips = 0;
			m_pTC->GetTextureDesc(_TextureID, &Desc, nMips);
			int W = Desc.GetWidth();
			int H = Desc.GetHeight();
			// Screen container may report zero if the engine hasn't
			// called PrepareFrame yet; fall back to the window size --
			// it's what the frontend composition wants anyway.
			if ((W <= 0 || H <= 0) && m_pDisplayContext)
			{
				W = m_pDisplayContext->m_Width;
				H = m_pDisplayContext->m_Height;
			}
			if (W <= 0 || H <= 0) return 0;

			glGenTextures(1, &S.m_ColorTex);
			glBindTexture(GL_TEXTURE_2D, S.m_ColorTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

			glGenRenderbuffers(1, &S.m_DepthRbo);
			glBindRenderbuffer(GL_RENDERBUFFER, S.m_DepthRbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			glGenFramebuffers(1, &S.m_FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, S.m_FBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, S.m_ColorTex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, S.m_DepthRbo);
			GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (Status != GL_FRAMEBUFFER_COMPLETE)
			{
				fprintf(stderr, "[GLES3-RTT] id=%d FBO incomplete 0x%x (%dx%d)\n",
					_TextureID, (unsigned)Status, W, H);
				glDeleteFramebuffers(1, &S.m_FBO);   S.m_FBO = 0;
				glDeleteRenderbuffers(1, &S.m_DepthRbo); S.m_DepthRbo = 0;
				glDeleteTextures(1, &S.m_ColorTex);  S.m_ColorTex = 0;
				return 0;
			}
			S.m_Width  = W;
			S.m_Height = H;
			// Zero-clear the color: fresh RTT contents are undefined per
			// spec (Mesa observed to return WHITE on Intel iGPU), and the
			// engine samples these textures BEFORE writing them (for the
			// menu-cube envmap: first draw = full-screen quad textured by
			// the capture, then CopyToTexture updates the capture). An
			// uninitialised white capture floods the whole backbuffer with
			// white on the first frame -- root cause of the "white cube"
			// menu bug 2026-07-21.
			GLint PrevDraw = 0;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PrevDraw);
			const GLboolean bScissor = glIsEnabled(GL_SCISSOR_TEST);
			if (bScissor) glDisable(GL_SCISSOR_TEST);
			glBindFramebuffer(GL_FRAMEBUFFER, S.m_FBO);
			// Force clear: bright green so we notice if it's WHAT gets
			// sampled by subsequent draws (screen turns green instead
			// of white). If we see WHITE the FBO isn't actually the
			// texture the shader samples.
			glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			unsigned char Px[4] = { 0xaa, 0xaa, 0xaa, 0xaa };
			glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, Px);
			GLenum Err = glGetError();
			fprintf(stderr,
				"[GLES3-RTT-VERIFY] id=%d after-clear px=(%u,%u,%u,%u) glErr=0x%x\n",
				_TextureID, Px[0], Px[1], Px[2], Px[3], (unsigned)Err);
			glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)PrevDraw);
			if (bScissor) glEnable(GL_SCISSOR_TEST);
			fprintf(stderr, "[GLES3-RTT] id=%d FBO ok %dx%d  colorTex=%u fbo=%u  CLEARED-TO-BLACK\n",
				_TextureID, W, H, S.m_ColorTex, S.m_FBO);
			fflush(stderr);
			return &S;
		}

		void ReleaseAllFBOs()
		{
			for (int i = 0; i < m_lFBO.Len(); ++i)
			{
				if (m_lFBO[i].m_FBO)       glDeleteFramebuffers(1,  &m_lFBO[i].m_FBO);
				if (m_lFBO[i].m_DepthRbo)  glDeleteRenderbuffers(1, &m_lFBO[i].m_DepthRbo);
				if (m_lFBO[i].m_ColorTex)  glDeleteTextures(1,      &m_lFBO[i].m_ColorTex);
				m_lFBO[i].m_FBO = m_lFBO[i].m_DepthRbo = m_lFBO[i].m_ColorTex = 0;
			}
		}

		// --- Phase 5: screen FBO + rotated composite ----------------
		// The engine renders every "backbuffer" pass into this FBO at
		// the logical resolution (display m_Width x m_Height); PageFlip
		// composites it into the physical window with the configured
		// rotation (g_RiddickPresent).
		GLuint m_ScreenFBO, m_ScreenColorTex, m_ScreenDepthRbo;
		int    m_ScreenW, m_ScreenH;
		bool   m_bScreenFBOFailed;
		CGLES3Shader m_CompShader;
		int    m_CompRotLoc, m_CompTexLoc;
		GLuint m_CompVBO;
		CGLES3RTTOverlay m_RTTOverlay;

		// Logical target height for top-left -> bottom-left Y flips
		// (scissor, clear rects, viewports). This is the FBO height,
		// not the window height.
		int ScreenH() const
		{
			return m_pDisplayContext ? m_pDisplayContext->m_Height : 0;
		}

		bool EnsureScreenFBO()
		{
			if (m_ScreenFBO) return true;
			if (m_bScreenFBOFailed || !m_pDisplayContext) return false;
			const int W = m_pDisplayContext->m_Width;
			const int H = m_pDisplayContext->m_Height;
			if (W <= 0 || H <= 0) return false;

			glGenTextures(1, &m_ScreenColorTex);
			glBindTexture(GL_TEXTURE_2D, m_ScreenColorTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

			glGenRenderbuffers(1, &m_ScreenDepthRbo);
			glBindRenderbuffer(GL_RENDERBUFFER, m_ScreenDepthRbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			glGenFramebuffers(1, &m_ScreenFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_ScreenColorTex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, m_ScreenDepthRbo);
			const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (Status != GL_FRAMEBUFFER_COMPLETE)
			{
				fprintf(stderr, "[GLES3] screen FBO incomplete 0x%x (%dx%d) - rendering direct to window\n",
					(unsigned)Status, W, H);
				glDeleteFramebuffers(1, &m_ScreenFBO);      m_ScreenFBO = 0;
				glDeleteRenderbuffers(1, &m_ScreenDepthRbo); m_ScreenDepthRbo = 0;
				glDeleteTextures(1, &m_ScreenColorTex);     m_ScreenColorTex = 0;
				m_bScreenFBOFailed = true;
				return false;
			}
			m_ScreenW = W;
			m_ScreenH = H;
			fprintf(stderr, "[GLES3] screen FBO %dx%d ok (rotate %d)\n",
				W, H, g_RiddickPresent.m_Rotate);
			fflush(stderr);
			return true;
		}

		void ReleaseScreenFBO()
		{
			if (m_ScreenFBO)      { glDeleteFramebuffers(1,  &m_ScreenFBO);      m_ScreenFBO = 0; }
			if (m_ScreenDepthRbo) { glDeleteRenderbuffers(1, &m_ScreenDepthRbo); m_ScreenDepthRbo = 0; }
			if (m_ScreenColorTex) { glDeleteTextures(1,      &m_ScreenColorTex); m_ScreenColorTex = 0; }
			if (m_CompVBO)        { glDeleteBuffers(1,       &m_CompVBO);        m_CompVBO = 0; }
		}

		// Bind the engine's notion of "the backbuffer": the screen FBO
		// when available, else the real window backbuffer.
		bool m_bRTTActive;

		void BindScreenTarget()
		{
			m_bRTTActive = false;
			if (EnsureScreenFBO())
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
				glViewport(0, 0, m_ScreenW, m_ScreenH);
				return;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (m_pDisplayContext)
				glViewport(0, 0, m_pDisplayContext->m_WinWidth, m_pDisplayContext->m_WinHeight);
		}

		// Called from CDisplayContextSDL2::PageFlip right before
		// SDL_GL_SwapWindow: draw the screen FBO into the window with
		// the configured rotation. Leaves the screen FBO bound again so
		// any engine draws issued before the next SetRenderTarget still
		// land in the right place.
		void PresentToWindow()
		{
			if (!m_ScreenFBO || !m_pDisplayContext) return;
			InitGLResources();
			if (!m_CompShader.IsValid()) return;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_pDisplayContext->m_WinWidth, m_pDisplayContext->m_WinHeight);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

			m_CompShader.Use();
			glUniform1i(m_CompRotLoc, (g_RiddickPresent.m_Rotate / 90) & 3);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_ScreenColorTex);
			glUniform1i(m_CompTexLoc, 0);

			glBindVertexArray(m_VAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_CompVBO);
			glEnableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			// Debug: draw every live RTT target as a grid on top
			// (RIDDICK_DBG_RTT=1). Window framebuffer is still bound.
			if (m_RTTOverlay.Enabled())
			{
				CGLES3RTTOverlay::SEntry Entries[CGLES3RTTOverlay::MAX_ENTRIES];
				int nEntries = 0;
				for (int i = 0; i < m_lFBO.Len() && nEntries < CGLES3RTTOverlay::MAX_ENTRIES; ++i)
				{
					if (!m_lFBO[i].m_FBO) continue;
					Entries[nEntries].m_Tex   = m_lFBO[i].m_ColorTex;
					Entries[nEntries].m_TexID = i;
					Entries[nEntries].m_W     = m_lFBO[i].m_Width;
					Entries[nEntries].m_H     = m_lFBO[i].m_Height;
					++nEntries;
				}
				m_RTTOverlay.Render(m_pDisplayContext->m_WinWidth,
					m_pDisplayContext->m_WinHeight, Entries, nEntries);
			}

			// Restore the engine's render target for the next frame.
			glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
			glViewport(0, 0, m_ScreenW, m_ScreenH);
		}

		// M3: GL resources for the draw path. Initialised lazily on
		// first Render_* call (Create() may run before the SDL2 GL
		// context is current).
		CGLES3Shader      m_UIShader;
		int m_UTexMat1Loc = -1, m_UTex1Loc = -1, m_UUseTex1Loc = -1;
		int m_UTexMatLoc = -1, m_UAlphaFuncLoc = -1, m_UAlphaRefLoc = -1;
		int m_UFogEnableLoc = -1, m_UFogColorLoc = -1, m_UFogStartLoc = -1, m_UFogEndLoc = -1;
		int m_DbgVBIDSkipFmt = 0;
		int m_DbgVBIDLastSkip = -1;
		CGLES3VBOStreamer m_Streamer;
		GLuint            m_VAO;
		bool              m_bGLInited;
		int               m_UMVPLoc;
		int               m_UUseTexLoc;
		int               m_UTexLoc;
		int               m_UDbgModeLoc;
		int               m_DbgShaderMode; // 0=off, 1=uv, 2=pos, 3=no_tex
		CRC_Attributes*   m_pCurAttrib;

		// Debug overrides via env vars (see DbgInit / SetupDbgOverrides).
		int  m_DbgNoDepth;   // RIDDICK_NO_DEPTH=1
		int  m_DbgNoCull;    // RIDDICK_NO_CULL=1
		int  m_DbgNoBlend;   // RIDDICK_NO_BLEND=1
		int  m_DbgNoAlpha;   // RIDDICK_NO_ALPHA=1  (disable alpha test in shader)
		int  m_DbgForceWire; // RIDDICK_FORCE_WIRE=1 (swap prim to GL_LINE_STRIP)

		// Frame dumper: on frame == m_DbgDumpFrameTarget (env RIDDICK_DUMP_FRAME=N)
		// write per-drawcall info to /tmp/openriddick_frame.txt then never again.
		int  m_DbgDumpFrameTarget;
		int  m_DbgDumpActive;    // 1 during the target frame
		int  m_DbgDumpDrawIdx;
		FILE* m_DbgDumpFp;

		// Diagnostic per-frame counters. Enable with RIDDICK_DBG_GL=1;
		// prints one line every DBG_INTERVAL frames.
		enum { DBG_INTERVAL = 60 };
		int m_DbgEnabled;
		int m_DbgTexDumpsLeft;
		int m_DbgFrames;
		int m_DbgDrawTri, m_DbgDrawStrip, m_DbgDrawWire, m_DbgDrawPoly, m_DbgDrawPrim;
		int m_DbgDrawVBID, m_DbgTexBound, m_DbgTexMissing;
		int m_DbgTotalVerts, m_DbgTotalIdx;
		int m_DbgAttribSets, m_DbgMatrixSets, m_DbgBeginScenes;
		int m_DbgUploadRGBA, m_DbgUploadDXT1, m_DbgUploadDXT3, m_DbgUploadDXT5, m_DbgUploadFail;

		static int DbgEnvFlag(const char* _Name)
		{
			const char* e = getenv(_Name);
			return (e && *e && *e != '0') ? 1 : 0;
		}
		void DbgInit()
		{
			m_DbgNoDepth   = DbgEnvFlag("RIDDICK_NO_DEPTH");
			m_DbgNoCull    = DbgEnvFlag("RIDDICK_NO_CULL");
			m_DbgNoBlend   = DbgEnvFlag("RIDDICK_NO_BLEND");
			m_DbgNoAlpha   = DbgEnvFlag("RIDDICK_NO_ALPHA");
			m_DbgForceWire = DbgEnvFlag("RIDDICK_FORCE_WIRE");
			m_DbgDumpFrameTarget = 0;
			if (const char* d = getenv("RIDDICK_DUMP_FRAME"))
			{
				int n = atoi(d);
				if (n > 0) m_DbgDumpFrameTarget = n;
			}
			m_DbgDumpActive = 0;
			m_DbgDumpDrawIdx = 0;
			m_DbgDumpFp = 0;
			m_DbgTotalFrames = 0;
			if (const char* d = getenv("RIDDICK_DUMP_OBJ"))
			{
				if (*d)
				{
					char cmd[512];
					snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", d);
					if (system(cmd) == -1) { /* best-effort */ }
					m_DumpObjDir = d;
					m_DumpObjMax = 500;
					fprintf(stderr, "[GEOM-DUMP] enabled -> %s (cap %d files)\n",
						d, m_DumpObjMax);
				}
			}
			if (m_DbgNoDepth || m_DbgNoCull || m_DbgNoBlend || m_DbgNoAlpha ||
				m_DbgForceWire || m_DbgDumpFrameTarget > 0)
			{
				fprintf(stderr, "[GL-DBG] overrides: NoDepth=%d NoCull=%d NoBlend=%d "
					"NoAlpha=%d ForceWire=%d DumpFrame=%d\n",
					m_DbgNoDepth, m_DbgNoCull, m_DbgNoBlend, m_DbgNoAlpha,
					m_DbgForceWire, m_DbgDumpFrameTarget);
			}
			const char* e = getenv("RIDDICK_DBG_GL");
			m_DbgEnabled = (e && *e && *e != '0') ? 1 : 0;
			// One-shot: dump the first 30 draw-call attrib channel
			// arrays to see whether the engine ever uses texture
			// slots > 0 (multitexture).
			m_DbgTexDumpsLeft = m_DbgEnabled ? 30 : 0;
			m_DbgFrames = 0;
			DbgResetCounters();
		}
		void DbgResetCounters()
		{
			m_DbgDrawTri = m_DbgDrawStrip = m_DbgDrawWire = m_DbgDrawPoly = m_DbgDrawPrim = 0;
			m_DbgDrawVBID = m_DbgTexBound = m_DbgTexMissing = 0;
			m_DbgVBIDSkipFmt = 0;
			m_DbgTotalVerts = m_DbgTotalIdx = 0;
			m_DbgAttribSets = m_DbgMatrixSets = m_DbgBeginScenes = 0;
			m_DbgUploadRGBA = m_DbgUploadDXT1 = m_DbgUploadDXT3 = m_DbgUploadDXT5 = m_DbgUploadFail = 0;
		}
		int m_DbgTotalFrames;
		void DbgDumpTick()
		{
			++m_DbgTotalFrames;
			// F9 → arm dump for the NEXT frame (this frame's drawcalls
			// already happened). Edge-detect so a long press only fires
			// once. Keyboard state is populated by SDL_PumpEvents which
			// CInputContext_SDL2 does every frame.
			static int s_PrevF9 = 0;
			int NumKeys = 0;
			const Uint8* pState = SDL_GetKeyboardState(&NumKeys);
			int F9 = (pState && NumKeys > SDL_SCANCODE_F9) ? pState[SDL_SCANCODE_F9] : 0;
			if (F9 && !s_PrevF9 && !m_DbgDumpActive && m_DbgDumpFrameTarget == 0)
			{
				m_DbgDumpFrameTarget = m_DbgTotalFrames + 1;
				fprintf(stderr, "[GL-DBG] F9: dump armed for frame %d\n",
					m_DbgDumpFrameTarget);
			}
			s_PrevF9 = F9;
			if (m_DbgDumpActive)
			{
				// Finish: close and never open again.
				if (m_DbgDumpFp)
				{
					fprintf(m_DbgDumpFp, "-- end frame %d, %d drawcalls --\n",
						m_DbgTotalFrames - 1, m_DbgDumpDrawIdx);
					fclose(m_DbgDumpFp);
					m_DbgDumpFp = 0;
				}
				m_DbgDumpActive = 0;
				m_DbgDumpFrameTarget = 0;
				fprintf(stderr, "[GL-DBG] frame dump written to /tmp/openriddick_frame.txt\n");
			}
			else if (m_DbgDumpFrameTarget > 0 && m_DbgTotalFrames == m_DbgDumpFrameTarget)
			{
				m_DbgDumpFp = fopen("/tmp/openriddick_frame.txt", "w");
				if (m_DbgDumpFp)
				{
					fprintf(m_DbgDumpFp, "-- frame %d dump --\n", m_DbgTotalFrames);
					m_DbgDumpActive = 1;
					m_DbgDumpDrawIdx = 0;
				}
				else
				{
					fprintf(stderr, "[GL-DBG] failed to open /tmp/openriddick_frame.txt\n");
					m_DbgDumpFrameTarget = 0;
				}
			}
		}
		void DbgFramePrint()
		{
			DbgDumpTick();
			if (!m_DbgEnabled) return;
			++m_DbgFrames;
			if (m_DbgFrames < DBG_INTERVAL) return;
			// Snapshot + reset the global upload counters.
			m_DbgUploadRGBA = g_GLES3_UploadRGBA; g_GLES3_UploadRGBA = 0;
			m_DbgUploadDXT1 = g_GLES3_UploadDXT1; g_GLES3_UploadDXT1 = 0;
			m_DbgUploadDXT3 = g_GLES3_UploadDXT3; g_GLES3_UploadDXT3 = 0;
			m_DbgUploadDXT5 = g_GLES3_UploadDXT5; g_GLES3_UploadDXT5 = 0;
			m_DbgUploadFail = g_GLES3_UploadFail; g_GLES3_UploadFail = 0;
			fprintf(stderr,
				"[GL-DBG] %df: draw{tri=%d strip=%d wire=%d poly=%d prim=%d VBID=%d skip=%d lastFmt=%d} "
				"verts=%d idx=%d texB=%d texMiss=%d attr=%d mat=%d beg=%d "
				"upl{rgba=%d dxt1=%d dxt3=%d dxt5=%d fail=%d}\n",
				m_DbgFrames, m_DbgDrawTri, m_DbgDrawStrip, m_DbgDrawWire,
				m_DbgDrawPoly, m_DbgDrawPrim, m_DbgDrawVBID,
				m_DbgVBIDSkipFmt, m_DbgVBIDLastSkip,
				m_DbgTotalVerts, m_DbgTotalIdx, m_DbgTexBound, m_DbgTexMissing,
				m_DbgAttribSets, m_DbgMatrixSets, m_DbgBeginScenes,
				m_DbgUploadRGBA, m_DbgUploadDXT1, m_DbgUploadDXT3, m_DbgUploadDXT5, m_DbgUploadFail);
			fflush(stderr);
			m_DbgFrames = 0;
			DbgResetCounters();
		}

		CRC_GLES3()
			: m_VAO(0), m_bGLInited(false), m_UMVPLoc(-1),
			  m_UUseTexLoc(-1), m_UTexLoc(-1), m_UDbgModeLoc(-1),
			  m_DbgShaderMode(0), m_PlaceholderTex(0), m_pCurAttrib(0)
		{
			m_ScreenFBO = m_ScreenColorTex = m_ScreenDepthRbo = 0;
			m_ScreenW = m_ScreenH = 0;
			m_bScreenFBOFailed = false;
			m_CompRotLoc = m_CompTexLoc = -1;
			m_CompVBO = 0;
			m_bRTTActive = false;
			g_pGLES3RCInst = this;
			m_ProjMat.Unit();
			m_ModelMat.Unit();
			for (int i = 0; i < 4; ++i) m_TexMat[i].Unit();
			DbgInit();
			m_RTTOverlay.InitFromEnv();
			const char* e = getenv("RIDDICK_DBG_SHADER");
			if (e)
			{
				if      (strcmp(e, "uv")     == 0) m_DbgShaderMode = 1;
				else if (strcmp(e, "pos")    == 0) m_DbgShaderMode = 2;
				else if (strcmp(e, "no_tex") == 0) m_DbgShaderMode = 3;
			}
		}

		~CRC_GLES3()
		{
			if (g_pGLES3RCInst == this) g_pGLES3RCInst = 0;
			m_RTTOverlay.Destroy();
			Texture_ReleaseAll();
			ReleaseAllFBOs();
			ReleaseScreenFBO();
			if (m_PlaceholderTex) { glDeleteTextures(1, &m_PlaceholderTex); m_PlaceholderTex = 0; }
			if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
		}

		void InitGLResources()
		{
			if (m_bGLInited) return;
			m_bGLInited = true;
			m_Streamer.Create();
			if (m_UIShader.Build(kGLES3_UIVertSrc, kGLES3_UIFragSrc, "UI"))
			{
				m_UMVPLoc     = m_UIShader.UniformLocation("uMVP");
				m_UUseTexLoc  = m_UIShader.UniformLocation("uUseTexture");
				m_UTexLoc     = m_UIShader.UniformLocation("uTex");
				m_UDbgModeLoc   = m_UIShader.UniformLocation("uDbgMode");
				m_UTexMatLoc    = m_UIShader.UniformLocation("uTexMat");
				m_UTexMat1Loc   = m_UIShader.UniformLocation("uTexMat1");
				m_UTex1Loc      = m_UIShader.UniformLocation("uTex1");
				m_UUseTex1Loc   = m_UIShader.UniformLocation("uUseTexture1");
				m_UAlphaFuncLoc = m_UIShader.UniformLocation("uAlphaFunc");
				m_UAlphaRefLoc  = m_UIShader.UniformLocation("uAlphaRef");
				m_UFogEnableLoc = m_UIShader.UniformLocation("uFogEnable");
				m_UFogColorLoc  = m_UIShader.UniformLocation("uFogColor");
				m_UFogStartLoc  = m_UIShader.UniformLocation("uFogStart");
				m_UFogEndLoc    = m_UIShader.UniformLocation("uFogEnd");
			}
			glGenVertexArrays(1, &m_VAO);
			if (m_CompShader.Build(kGLES3_CompVertSrc, kGLES3_CompFragSrc, "Composite"))
			{
				m_CompRotLoc = m_CompShader.UniformLocation("uRot");
				m_CompTexLoc = m_CompShader.UniformLocation("uTex");
			}
			static const float sQuad[8] = { -1,-1,  1,-1,  -1,1,  1,1 };
			glGenBuffers(1, &m_CompVBO);
			glBindBuffer(GL_ARRAY_BUFFER, m_CompVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(sQuad), sQuad, GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		// --- M2 texture path ---------------------------------------
		void Texture_ReleaseAll()
		{
			for (int i = 0; i < m_lGLTex.Len(); ++i)
			{
				if (m_lGLTex[i])
					glDeleteTextures(1, &m_lGLTex[i]);
				m_lGLTex[i] = 0;
			}
		}

		// Ensures engine textureID has been uploaded to a GLuint.
		// Returns 0 on failure (unknown ID, empty container, format
		// not yet supported by GLES3_Texture MapFormat).
		// Sparse "we already logged this failure" flags parallel to
		// m_lGLTex. Byte per texture ID: 0 = not touched, 1 =
		// upload attempted (success or failure). Prevents stderr
		// flood when the engine keeps re-requesting an unsupported
		// texture every frame.
		TArray<uint8> m_lTexLogged;

		// RIDDICK_DBG_GL census: log every distinct texture ID the
		// engine ever puts in a draw attrib, once, with its name.
		// Answers "does the engine even ask for this texture?".
		TArray<uint8> m_lTexSeenDbg;

		void DbgNoteTexID(int _TextureID, int _Channel)
		{
			if (!m_DbgEnabled || _TextureID <= 0) return;
			if (_TextureID >= m_lTexSeenDbg.Len())
			{
				const int Old = m_lTexSeenDbg.Len();
				m_lTexSeenDbg.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lTexSeenDbg.Len(); ++i)
					m_lTexSeenDbg[i] = 0;
			}
			if (m_lTexSeenDbg[_TextureID]) return;
			m_lTexSeenDbg[_TextureID] = 1;
			fprintf(stderr, "[GL-TEXREQ] ch%d id=%d name='%s'\n", _Channel, _TextureID,
				m_pTC ? (const char*)m_pTC->GetName(_TextureID) : "?");
			fflush(stderr);
		}

		GLuint TextureID_EnsureUploaded(int _TextureID)
		{
			if (_TextureID < 0 || !m_pTC) return 0;
			if (_TextureID >= m_lGLTex.Len())
			{
				const int Old = m_lGLTex.Len();
				m_lGLTex.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lGLTex.Len(); ++i)
					m_lGLTex[i] = 0;
			}
			if (_TextureID >= m_lTexLogged.Len())
			{
				const int Old = m_lTexLogged.Len();
				m_lTexLogged.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lTexLogged.Len(); ++i)
					m_lTexLogged[i] = 0;
			}
			if (m_lGLTex[_TextureID])
				return m_lGLTex[_TextureID];
			// If this ID is bound to an FBO (RTT slot), hand back its
			// color texture -- that's the "content" the engine expects
			// to sample after rendering to it.
			if (SFBOSlot* pSlot = GetFBOSlot(_TextureID))
				return pSlot->m_ColorTex;

			// Uploads need the GL context, which is current only on the
			// render thread; world precache calls us from loader threads
			// (glGenTextures returns 0 with glGetError==0 there). Defer:
			// the draw path retries on the GL thread. Confirmed by the
			// 2026-07-20 run log (~500 textures failed exactly this way).
			if (!SDL_GL_GetCurrentContext())
				return 0;

			// Note: file-backed containers (VirtualXTC) page the texel
			// data in inside GetTexture itself (Load(iLocal, iMip)), so
			// no extra force-load call is needed here.
			CImage* pImg = m_pTC->GetTexture(_TextureID, 0, -1);
			if (!pImg)
			{
				if (!m_lTexLogged[_TextureID])
				{
					m_lTexLogged[_TextureID] = 1;
					fprintf(stderr, "[GLES3-TEX-FAIL] id=%d  name='%s'  GetTexture()==NULL -> placeholder\n",
						_TextureID, (const char*)m_pTC->GetName(_TextureID));
					fflush(stderr);
				}
				// Shared magenta placeholder; NOT stored into m_lGLTex
				// (release path would double-free the shared GLuint).
				// No fail-latching: data may arrive later (async
				// loaders), so retry on the next request.
				return GetPlaceholderTex();
			}
			GLuint T = CGLES3TextureUploader::Upload2D(pImg, true);
			m_lGLTex[_TextureID] = T;
			const bool bLogged = m_lTexLogged[_TextureID] != 0;
			m_lTexLogged[_TextureID] = 1;
			if (T)
			{
				fprintf(stderr, "[GLES3-TEX-OK] id=%d  name='%s'  %dx%d  fmt=0x%x mem=0x%x\n",
					_TextureID, (const char*)m_pTC->GetName(_TextureID),
					pImg->GetWidth(), pImg->GetHeight(),
					(unsigned)pImg->GetFormat(), (unsigned)pImg->GetMemModel());
				fflush(stderr);
			}
			if (!T && !bLogged)
			{
				const int Fmt = pImg->GetFormat();
				const int Mem = pImg->GetMemModel();
				int S3TCSub = -1;
				if (pImg->IsCompressed() && (Mem & IMAGE_MEM_COMPRESSTYPE_S3TC))
				{
					unsigned char* pRaw = (unsigned char*)pImg->LockCompressed();
					if (pRaw)
						S3TCSub = (int)((const CImage_CompressHeader_S3TC*)pRaw)->getCompressType();
				}
				fprintf(stderr,
					"[GLES3-TEX-FAIL] id=%d  %dx%d  format=0x%x  memmodel=0x%x  s3tcSub=%d\n",
					_TextureID, pImg->GetWidth(), pImg->GetHeight(),
					(unsigned)Fmt, (unsigned)Mem, S3TCSub);
				fflush(stderr);
			}
			return T;
		}

		virtual void Texture_Precache(int _TextureID)
		{
			TextureID_EnsureUploaded(_TextureID);
		}

		virtual void Texture_Flush(int _TextureID)
		{
			if (_TextureID >= 0 && _TextureID < m_lGLTex.Len() && m_lGLTex[_TextureID])
			{
				glDeleteTextures(1, &m_lGLTex[_TextureID]);
				m_lGLTex[_TextureID] = 0;
			}
		}

		virtual void Texture_MakeAllDirty(int _iPicMip = -1)
		{
			Texture_ReleaseAll();
		}

		virtual void Texture_PrecacheFlush() {}
		virtual void Texture_PrecacheBegin(int _Count) {}
		virtual void Texture_PrecacheEnd() {}

		void Create(CObj* _pContext, const char* _pParams)
		{
			CRC_Core::Create(_pContext, _pParams);

			MACRO_GetSystem;

			m_Caps_TextureFormats = -1;
			m_Caps_DisplayFormats = -1;
			m_Caps_ZFormats = -1;
			m_Caps_StencilDepth = 8;
			m_Caps_AlphaDepth = 8;
			// Honest caps: the engine picks its render path from these.
			// Claiming everything (-1) routed the world through paths our
			// backend cannot execute: OCCLUSIONQUERY culling (our query
			// stubs report "0 pixels visible" -> whole world culled) and
			// FRAGMENTPROGRAM20/30 shader layers (XRShader.cpp:1268).
			// With a minimal set the engine falls back to the fixed-
			// function/texenv multipass path, which maps onto our
			// attrib-based GLES3 drawing.
			m_Caps_Flags = CRC_CAPS_FLAGS_HWAPI
			             | CRC_CAPS_FLAGS_ARBITRARY_TEXTURE_SIZE
			             | CRC_CAPS_FLAGS_SEPARATESTENCIL;

			// 2 texture units = the engine's most compatible multipass
			// path (base + lightmap per pass) -- exactly what the shader
			// in this backend implements (uTex/uTex1).
			m_Caps_nMultiTexture = 2;
			m_Caps_nMultiTextureCoords = 2;
			m_Caps_nMultiTextureEnv = 2;

			// Register with the texture/VB contexts (the PS3 backend does
			// this in its Create; CRC_Core::Create does not) so they can
			// track per-RC state and dirty-notify us.
			m_iTC = m_pTC->AddRenderContext(this);
			m_iVBCtxRC = m_pVBCtx->AddRenderContext(this);

		}

		const char* GetRenderingStatus() { return ""; }
		virtual void Flip_SetInterval(int _nFrames){};

		// Render target: bind engine-requested FBO if any texture ID is
		// specified, otherwise fall back to the window backbuffer (fb0).
		void RenderTarget_SetRenderTarget(const CRC_RenderTargetDesc& _RenderTarget)
		{
			int TargetID = 0;
			for (int i = 0; i < 4 /*CRC_MAXMRT*/; ++i)
			{
				if (_RenderTarget.m_lColorTextureID[i])
				{
					TargetID = (int)_RenderTarget.m_lColorTextureID[i];
					break;
				}
			}
			if (m_DbgDumpActive && m_DbgDumpFp)
			{
				fprintf(m_DbgDumpFp,
					"---- RenderTarget_SetRenderTarget ids=[%u,%u,%u,%u] target=%d ----\n",
					(unsigned)_RenderTarget.m_lColorTextureID[0],
					(unsigned)_RenderTarget.m_lColorTextureID[1],
					(unsigned)_RenderTarget.m_lColorTextureID[2],
					(unsigned)_RenderTarget.m_lColorTextureID[3], TargetID);
			}
			if (m_DbgEnabled)
			{
				fprintf(stderr, "[GLES3-RT] SetRenderTarget ids=[%u,%u,%u,%u] -> %s\n",
					(unsigned)_RenderTarget.m_lColorTextureID[0],
					(unsigned)_RenderTarget.m_lColorTextureID[1],
					(unsigned)_RenderTarget.m_lColorTextureID[2],
					(unsigned)_RenderTarget.m_lColorTextureID[3],
					TargetID ? "FBO" : "backbuffer");
			}
			if (TargetID > 0)
			{
				SFBOSlot* pSlot = EnsureFBOFor(TargetID);
				if (pSlot)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, pSlot->m_FBO);
					glViewport(0, 0, pSlot->m_Width, pSlot->m_Height);
					m_bRTTActive = true;
					return;
				}
				// Failed to build FBO -- fall through to backbuffer so
				// something renders.
			}
			BindScreenTarget();
		}

		// Copy the current READ framebuffer's colour attachment into the
		// GL texture backing _TextureID. This is how the engine
		// "snapshots" a scene rendered into the backbuffer for later
		// sampling by another pass (PS3 backend uses gcmSetTransferImage
		// for the same purpose -- see MRenderPS3_Texture.cpp:2107).
		void RenderTarget_CopyToTexture(int _TextureID, CRct _SrcRect, CPnt _Dest, bint _bContinueTiling, uint16 _Slice, int _iMRT)
		{
			if (_TextureID <= 0) return;
			SFBOSlot* pSlot = EnsureFBOFor(_TextureID);
			if (!pSlot) return;

			// The engine's rect is top-left origin, GL is bottom-left.
			// Flip Y so the copy pulls the correct region from the
			// currently-bound framebuffer. Row ORDER inside the copied
			// band is ambiguous (depends on the GUI path the engine
			// picked, which shifted with the caps change) -- so it is
			// runtime-switchable: RIDDICK_COPYTEX_FLIP=1 flips rows via
			// a Y-inverted blit, default is plain copy. A/B test without
			// rebuilding.
			int SrcH = ScreenH();
			int W = _SrcRect.p1.x - _SrcRect.p0.x;
			int H = _SrcRect.p1.y - _SrcRect.p0.y;
			if (W <= 0 || H <= 0) return;

			// Default = flipped blit: confirmed correct on screen
			// 2026-07-20 (menu orientation AND row positions right).
			// RIDDICK_COPYTEX_FLIP=0 switches back to the plain copy.
			static int sFlip = -1;
			if (sFlip < 0)
			{
				const char* e = getenv("RIDDICK_COPYTEX_FLIP");
				sFlip = (e && *e == '0') ? 0 : 1;
			}

			if (sFlip)
			{
				GLint PrevDraw = 0, PrevRead = 0;
				glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PrevDraw);
				glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &PrevRead);
				const GLboolean bScissor = glIsEnabled(GL_SCISSOR_TEST);
				if (bScissor) glDisable(GL_SCISSOR_TEST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)PrevDraw);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pSlot->m_FBO);
				glBlitFramebuffer(
					_SrcRect.p0.x, SrcH - _SrcRect.p0.y,
					_SrcRect.p1.x, SrcH - _SrcRect.p1.y,
					_Dest.x, _Dest.y, _Dest.x + W, _Dest.y + H,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)PrevRead);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)PrevDraw);
				if (bScissor) glEnable(GL_SCISSOR_TEST);
			}
			else
			{
				int SrcY = SrcH - _SrcRect.p1.y;
				// Save + restore currently-bound texture so we don't
				// disturb the current drawcall's binding.
				GLint PrevTex = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &PrevTex);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pSlot->m_ColorTex);
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, _Dest.x, _Dest.y,
					_SrcRect.p0.x, SrcY, W, H);
				glBindTexture(GL_TEXTURE_2D, (GLuint)PrevTex);
			}

			if (m_DbgEnabled)
			{
				fprintf(stderr, "[GLES3-RT] CopyToTexture id=%d  src(%d,%d..%d,%d)->dst(%d,%d)  %dx%d\n",
					_TextureID, _SrcRect.p0.x, _SrcRect.p0.y,
					_SrcRect.p1.x, _SrcRect.p1.y, _Dest.x, _Dest.y, W, H);
			}
		}

		void RenderTarget_Clear(CRct _ClearRect, int _WhatToClear, CPixel32 _Color, fp32 _ZBufferValue, int _StecilValue)
		{
			static int sLogged = 0;
			if (sLogged < 20)
			{
				GLint CurFBO = 0;
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurFBO);
				fprintf(stderr, "[GLES3-CLEAR] FBO=%d what=0x%x rgba=(%u,%u,%u,%u) rect=(%d,%d..%d,%d)\n",
					(int)CurFBO, _WhatToClear,
					_Color.GetR(), _Color.GetG(), _Color.GetB(), _Color.GetA(),
					_ClearRect.p0.x, _ClearRect.p0.y, _ClearRect.p1.x, _ClearRect.p1.y);
				fflush(stderr);
				++sLogged;
			}
			GLbitfield Mask = 0;
			if (_WhatToClear & CDC_CLEAR_COLOR)
			{
				const fp32 s = 1.0f / 255.0f;
				glClearColor(_Color.GetR() * s, _Color.GetG() * s, _Color.GetB() * s, _Color.GetA() * s);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				Mask |= GL_COLOR_BUFFER_BIT;
			}
			if (_WhatToClear & CDC_CLEAR_ZBUFFER)
			{
				glClearDepthf(_ZBufferValue);
				glDepthMask(GL_TRUE);
				Mask |= GL_DEPTH_BUFFER_BIT;
			}
			if (_WhatToClear & CDC_CLEAR_STENCIL)
			{
				glClearStencil(_StecilValue);
				glStencilMask(0xff);
				Mask |= GL_STENCIL_BUFFER_BIT;
			}
			if (!Mask)
				return;

			// Empty rect => full target clear. Otherwise clip via scissor.
			const int w = _ClearRect.p1.x - _ClearRect.p0.x;
			const int h = _ClearRect.p1.y - _ClearRect.p0.y;
			if (w > 0 && h > 0 && m_pDisplayContext)
			{
				glEnable(GL_SCISSOR_TEST);
				// GLES scissor origin is bottom-left; engine rects are top-left.
				const int y = ScreenH() - _ClearRect.p1.y;
				glScissor(_ClearRect.p0.x, y, w, h);
				glClear(Mask);
				glDisable(GL_SCISSOR_TEST);
			}
			else
			{
				glClear(Mask);
			}
		}

		// Full-set translation of a CRC_Attributes bundle to GL state.
		// Called by both Attrib_Set (delta) and Attrib_SetAbsolute
		// (reset). We ignore the delta hint for M1 and re-apply
		// everything -- keep it simple; M4 can turn this into a diff.
		void ApplyAttribs(CRC_Attributes* _pAttrib)
		{
			if (!_pAttrib) return;
			m_pCurAttrib = _pAttrib;
			const uint32 F = _pAttrib->m_Flags;

			// Depth
			if (F & CRC_FLAGS_ZCOMPARE)
			{
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GLES3_MapCompare(_pAttrib->m_ZCompare));
			}
			else
			{
				glDisable(GL_DEPTH_TEST);
			}
			glDepthMask((F & CRC_FLAGS_ZWRITE) ? GL_TRUE : GL_FALSE);

			// Blend
			if (F & CRC_FLAGS_BLEND)
			{
				glEnable(GL_BLEND);
				const uint16 SD = _pAttrib->m_SourceDestBlend;
				const uint8  Src = (uint8)(SD & 0xff);
				const uint8  Dst = (uint8)((SD >> 8) & 0xff);
				glBlendFunc(GLES3_MapBlend(Src), GLES3_MapBlend(Dst));
			}
			else
			{
				glDisable(GL_BLEND);
			}

			// Colour + alpha write
			const GLboolean CW = (F & CRC_FLAGS_COLORWRITE) ? GL_TRUE : GL_FALSE;
			const GLboolean AW = (F & CRC_FLAGS_ALPHAWRITE) ? GL_TRUE : GL_FALSE;
			glColorMask(CW, CW, CW, AW);

			// Culling
			if ((F & CRC_FLAGS_CULL) && !m_DbgNoCull)
			{
				glEnable(GL_CULL_FACE);
				// Engine winding is CW when CULLCW; GLES default front = CCW.
				glFrontFace((F & CRC_FLAGS_CULLCW) ? GL_CW : GL_CCW);
				glCullFace(GL_BACK);
			}
			else
			{
				glDisable(GL_CULL_FACE);
			}

			// Debug overrides (final say).
			if (m_DbgNoDepth) { glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); }
			if (m_DbgNoBlend) { glDisable(GL_BLEND); }

			// Scissor
			if (F & CRC_FLAGS_SCISSOR)
			{
				uint32 MinX, MinY, MaxX, MaxY;
				_pAttrib->m_Scissor.GetRect(MinX, MinY, MaxX, MaxY);
				const int H = ScreenH();
				const int W = (int)(MaxX - MinX);
				const int Hgt = (int)(MaxY - MinY);
				if (W > 0 && Hgt > 0)
				{
					glEnable(GL_SCISSOR_TEST);
					glScissor((int)MinX, H - (int)MaxY, W, Hgt);
				}
				else
				{
					glDisable(GL_SCISSOR_TEST);
				}
			}
			else
			{
				glDisable(GL_SCISSOR_TEST);
			}

			// Polygon offset
			if (F & CRC_FLAGS_POLYGONOFFSET)
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(_pAttrib->m_PolygonOffsetScale, _pAttrib->m_PolygonOffsetUnits);
			}
			else
			{
				glDisable(GL_POLYGON_OFFSET_FILL);
			}

			// Stencil
			if (F & CRC_FLAGS_STENCIL)
			{
				glEnable(GL_STENCIL_TEST);
				glStencilMask(_pAttrib->m_StencilWriteMask);
				// Op mapping: 0..7 keep/zero/replace/incr/decr/invert/
				// incr_wrap/decr_wrap; funcs stored 1..8 in CRC_COMPARE_*.
				static const GLenum sOpTbl[] = {
					GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT,
					GL_INCR_WRAP, GL_DECR_WRAP,
				};
				const int F0 = _pAttrib->m_StencilFrontOpFail   & 7;
				const int F1 = _pAttrib->m_StencilFrontOpZFail  & 7;
				const int F2 = _pAttrib->m_StencilFrontOpZPass  & 7;
				if (F & CRC_FLAGS_SEPARATESTENCIL)
				{
					// Two-sided stencil (engine shadow volumes: incr on
					// front faces, decr on back faces in a single pass).
					const int B0 = _pAttrib->m_StencilBackOpFail   & 7;
					const int B1 = _pAttrib->m_StencilBackOpZFail  & 7;
					const int B2 = _pAttrib->m_StencilBackOpZPass  & 7;
					glStencilFuncSeparate(GL_FRONT, GLES3_MapCompare(_pAttrib->m_StencilFrontFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilFuncSeparate(GL_BACK, GLES3_MapCompare(_pAttrib->m_StencilBackFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilOpSeparate(GL_FRONT, sOpTbl[F0], sOpTbl[F1], sOpTbl[F2]);
					glStencilOpSeparate(GL_BACK,  sOpTbl[B0], sOpTbl[B1], sOpTbl[B2]);
				}
				else
				{
					glStencilFunc(GLES3_MapCompare(_pAttrib->m_StencilFrontFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilOp(sOpTbl[F0], sOpTbl[F1], sOpTbl[F2]);
				}
			}
			else
			{
				glDisable(GL_STENCIL_TEST);
			}
		}

		void Attrib_Set(CRC_Attributes* _pAttrib)         { ++m_DbgAttribSets; ApplyAttribs(_pAttrib); }
		void Attrib_SetAbsolute(CRC_Attributes* _pAttrib) { ++m_DbgAttribSets; ApplyAttribs(_pAttrib); }

		void Matrix_SetRender(int _iMode, const CMat4Dfp32* _pMatrix)
		{
			++m_DbgMatrixSets;
			// Matrix_Update passes NULL to mean "matrix is identity"
			// (see MRender.cpp:3410). Reset our cached matrix so a
			// stale value from a previous frame doesn't leak in.
			CMat4Dfp32 Unit; Unit.Unit();
			const CMat4Dfp32& M = _pMatrix ? *_pMatrix : Unit;
			switch (_iMode)
			{
			case CRC_MATRIX_MODEL:      m_ModelMat = M; break;
			case CRC_MATRIX_PROJECTION: m_ProjMat  = M; break;
			case CRC_MATRIX_TEXTURE0:
			case CRC_MATRIX_TEXTURE0 + 1:
			case CRC_MATRIX_TEXTURE0 + 2:
			case CRC_MATRIX_TEXTURE0 + 3:
				m_TexMat[_iMode - CRC_MATRIX_TEXTURE0] = M;
				break;
			default: break;
			}
		}

		// BeginScene: sync viewport with the active CRC_Viewport rect.
		// The engine calls Viewport_Set separately too, but Base_CRC's
		// stub does nothing; we just do it here so state is coherent
		// before draw calls start.
		void BeginScene(CRC_Viewport* _pVP)
		{
			++m_DbgBeginScenes;
			// Make sure "the backbuffer" means the screen FBO even if
			// the engine never called SetRenderTarget this frame (the
			// composite pass leaves fb0 bound only transiently).
			if (!m_bRTTActive)
				BindScreenTarget();
			CRC_Core::BeginScene(_pVP);
			// CRC_Core::BeginScene already calls Viewport_Set(_pVP)
			// which invokes our overridden Viewport_Update below --
			// projection + glViewport are set from there.
		}

		// Called by CRC_Core whenever the active viewport changes
		// (Viewport_Set from CRC_Core::BeginScene, from
		// CXR_VBManager::Internal_Render mid-scene, from
		// Viewport_Push/Pop). Mirrors CRCPS3GCM::Viewport_Update:
		//
		//   1) pull raw projection matrix from CRC_Viewport
		//   2) scale rows 0 (x) and 1 (y) by 2/width, 2/height --
		//      engine's projection projects into pixel-space
		//      [-w/2, +w/2] / [-h/2, +h/2], not NDC [-1, +1]; the
		//      backend has to bring it into clip space itself
		//   3) set the actual GL viewport (Y-flipped for GLES)
		void Viewport_Update()
		{
			CRC_Core::Viewport_Update();
			CRC_Viewport* pVP = Viewport_Get();
			if (!pVP) return;

			CRct R = pVP->GetViewArea();
			const int W = R.p1.x - R.p0.x;
			const int H = R.p1.y - R.p0.y;

			CMat4Dfp32 ProjMat = pVP->GetProjectionMatrix();
			if (W > 0 && H > 0)
			{
				const fp32 xs = 2.0f / (fp32)W;
				const fp32 ys = 2.0f / (fp32)H;
				ProjMat.k[0][0] *= xs; ProjMat.k[1][0] *= xs;
				ProjMat.k[2][0] *= xs; ProjMat.k[3][0] *= xs;
				ProjMat.k[0][1] *= ys; ProjMat.k[1][1] *= ys;
				ProjMat.k[2][1] *= ys; ProjMat.k[3][1] *= ys;
			}
			m_ProjMat = ProjMat;

			if (m_pDisplayContext && W > 0 && H > 0)
			{
				const int Y = ScreenH() - R.p1.y;
				glViewport(R.p0.x, Y, W, H);
			}
		}

		virtual int Texture_GetBackBufferTextureID() {return 0;}
		virtual int Texture_GetFrontBufferTextureID() {return 0;}
		virtual int Texture_GetZBufferTextureID() {return 0;}
		virtual int Geometry_GetVBSize(int _VBID) {return 0;}

		// --- M3 draw path ------------------------------------------
		// Interleaved vertex: pos.xyz (3f) + uv0.xy (2f) + uv1.xy (2f,
		// channel 1: lightmaps) + colour BGRA packed as uint32. 32 bytes.
		struct SUIVert { float x,y,z, u,v, u1,v1; uint32_t col; };

		// Pack CPixel32 (BGRA byte order per MImage.h) to RGBA-word for
		// the shader (glVertexAttribPointer normalized ubyte4 reads in
		// memory order, so we swap B/R to feed vCol.rgb correctly).
		static uint32_t PackColorBGRA_to_RGBA(uint32_t _bgra)
		{
			return ( _bgra & 0xff00ff00u)
				 | ((_bgra & 0x00ff0000u) >> 16)
				 | ((_bgra & 0x000000ffu) << 16);
		}

		// Build interleaved buffer from the CRC_Core-accumulated
		// m_Geom + m_GeomColor. tex-channel 0 is enough for UI; higher
		// channels arrive in M4 shader generator.
		bool BuildInterleavedVerts(SUIVert*& _pOut, int& _nOut, bool& _bMalloced)
		{
			_bMalloced = false;
			// BSP2 solid-world path: engine called Geometry_VertexBuffer(VBID)
			// (setter that just stores VBID in m_GeomVBID — see MRender.cpp:4127)
			// then Render_IndexedTriangles(piPrim, nPrim). m_Geom stays stale
			// from a previous UI draw; we must fetch the real vertices from
			// the VBID here. VBB uses packed formats (I16/NS/NU) that
			// BuildVertsFromVBB (which VRegFetch decodes) already handles.
			// Without this override, retail-Win32 GL delegated to CRC_Core
			// which had a full-VBB fetch built in; our old code was reading
			// stale m_Geom, hence "stretched polygons across the whole
			// screen" symptom in Pa1_TheDream.
			if (m_GeomVBID != 0 && m_pVBCtx)
			{
				CRC_BuildVertexBuffer VBB;
				VBB.Clear();
				m_pVBCtx->VB_Get((int)m_GeomVBID, VBB, VB_GETFLAGS_BUILD);
				int nV = 0;
				SUIVert* p = BuildVertsFromVBB(VBB, nV);
				if (!p) { _pOut = 0; _nOut = 0; return false; }
				_pOut = p;
				_nOut = nV;
				_bMalloced = true;
				return true;
			}

			const int nV = (int)m_Geom.m_nV;
			if (nV <= 0 || !m_Geom.m_pV) { _pOut = 0; _nOut = 0; return false; }
			static SUIVert sScratch[16384];
			SUIVert* p = (nV <= 16384) ? sScratch : (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!p) return false;
			_bMalloced = (nV > 16384);

			const CVec3Dfp32* pV   = m_Geom.m_pV;
			const fp32*       pTV0 = m_Geom.m_pTV[0];
			const int         nUV  = m_Geom.m_nTVComp[0]; // 0/2/3/4
			const fp32*       pTV1 = m_Geom.m_pTV[1];
			const int         nUV1 = m_Geom.m_nTVComp[1];
			const CPixel32*   pCol = m_Geom.m_pCol;
			const uint32_t    ConstCol = PackColorBGRA_to_RGBA(*(const uint32_t*)&m_GeomColor);

			for (int i = 0; i < nV; ++i)
			{
				p[i].x = pV[i].k[0];
				p[i].y = pV[i].k[1];
				p[i].z = pV[i].k[2];
				if (pTV0 && nUV >= 2)
				{
					p[i].u = pTV0[i * nUV + 0];
					p[i].v = pTV0[i * nUV + 1];
				}
				else
				{
					p[i].u = 0.0f; p[i].v = 0.0f;
				}
				if (pTV1 && nUV1 >= 2)
				{
					p[i].u1 = pTV1[i * nUV1 + 0];
					p[i].v1 = pTV1[i * nUV1 + 1];
				}
				else
				{
					p[i].u1 = 0.0f; p[i].v1 = 0.0f;
				}
				p[i].col = pCol ? PackColorBGRA_to_RGBA(*(uint32_t*)&pCol[i]) : ConstCol;
			}
			_pOut = p;
			_nOut = nV;
			return true;
		}

		void FreeScratch(SUIVert* _p, int _nV, bool _bMalloced)
		{
			// _bMalloced set by BuildInterleavedVerts when the buffer came
			// from malloc (VBID path always, or fallback path when nV>16384).
			if (_bMalloced) free(_p);
			(void)_nV;
		}

		// Vertex layout for SUIVert at byte offset _Base inside the
		// currently-bound VBO.
		void SetVertexAttribPointers(intptr_t _Base)
		{
			const GLsizei S = (GLsizei)sizeof(SUIVert);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 0));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 12));
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 20));
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, S, (const void*)(_Base + 28));
		}

		void DisableVertexAttribPointers()
		{
			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
		}

		// Shared uniform + texture setup for the UI shader: MVP,
		// texture matrices, alpha test, fog (optional), textures from
		// the current attrib (channel 0 = base, channel 1 = secondary
		// modulate -- lightmaps).
		void SetupCommonUniforms(bool _bAllowFog)
		{
			m_UIShader.Use();
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			m_UIShader.SetMat4(m_UMVPLoc, (const float*)&MVP);
			m_UIShader.SetMat4(m_UTexMatLoc,  (const float*)&m_TexMat[0]);
			m_UIShader.SetMat4(m_UTexMat1Loc, (const float*)&m_TexMat[1]);

			// Alpha test (no fixed-function path in GLES3; done in shader)
			if (!m_DbgNoAlpha && m_pCurAttrib && m_pCurAttrib->m_AlphaCompare != CRC_COMPARE_ALWAYS)
			{
				m_UIShader.SetInt(m_UAlphaFuncLoc, m_pCurAttrib->m_AlphaCompare);
				m_UIShader.SetFloat(m_UAlphaRefLoc, (float)m_pCurAttrib->m_AlphaRef * (1.0f / 255.0f));
			}
			else
				m_UIShader.SetInt(m_UAlphaFuncLoc, 0);

			if (_bAllowFog && m_pCurAttrib && (m_pCurAttrib->m_Flags & CRC_FLAGS_FOG))
			{
				const CPixel32 FC = m_pCurAttrib->m_FogColor;
				const float Fog[3] = { FC.GetR() * (1.0f/255.0f), FC.GetG() * (1.0f/255.0f), FC.GetB() * (1.0f/255.0f) };
				m_UIShader.SetInt(m_UFogEnableLoc, 1);
				glUniform3fv(m_UFogColorLoc, 1, Fog);
				m_UIShader.SetFloat(m_UFogStartLoc, m_pCurAttrib->m_FogStart);
				m_UIShader.SetFloat(m_UFogEndLoc, m_pCurAttrib->m_FogEnd);
			}
			else
				m_UIShader.SetInt(m_UFogEnableLoc, 0);

			// Textures. Base = channel 0; if channel 0 is empty scan
			// 1..N for the first non-zero (shader-driven UI surfaces
			// sometimes park the main texture in a higher slot) -- in
			// that case there is no secondary. Channel 1 on top of a
			// channel-0 base = multitexture (lightmap modulate).
			int UseTex = 0, UseTex1 = 0;
			if (m_pCurAttrib)
			{
				int Tex0 = (int)m_pCurAttrib->m_TextureID[0];
				int Tex1 = (int)m_pCurAttrib->m_TextureID[1];
				if (!Tex0)
				{
					Tex1 = 0;
					for (int c = 1; c < CRC_MAXTEXTURES; ++c)
						if (m_pCurAttrib->m_TextureID[c]) { Tex0 = (int)m_pCurAttrib->m_TextureID[c]; break; }
				}
				DbgNoteTexID(Tex0, 0);
				DbgNoteTexID(Tex1, 1);
				if (Tex0 > 0)
				{
					GLuint T = TextureID_EnsureUploaded(Tex0);
					m_DbgLastBind0 = T;
					if (T)
					{
						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, T);
						m_UIShader.SetInt(m_UTexLoc, 0);
						UseTex = 1;
						++m_DbgTexBound;
					}
					else
						++m_DbgTexMissing;
				}
				else
					m_DbgLastBind0 = 0;
				if (UseTex && Tex1 > 0)
				{
					GLuint T1 = TextureID_EnsureUploaded(Tex1);
					if (T1)
					{
						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, T1);
						m_UIShader.SetInt(m_UTex1Loc, 1);
						UseTex1 = 1;
						++m_DbgTexBound;
					}
					else
						++m_DbgTexMissing;
					glActiveTexture(GL_TEXTURE0);
				}
			}
			m_UIShader.SetInt(m_UUseTexLoc, UseTex);
			m_UIShader.SetInt(m_UUseTex1Loc, UseTex1);
			m_UIShader.SetInt(m_UDbgModeLoc, m_DbgShaderMode);
			m_DbgLastUseTex = UseTex;
			m_DbgLastUseTex1 = UseTex1;
		}
		int    m_DbgLastUseTex   = 0;
		int    m_DbgLastUseTex1  = 0;
		GLuint m_DbgLastBind0    = 0;

		// Geometry dumper: on RIDDICK_DUMP_OBJ=<dir>, writes each unique
		// drawn mesh to <dir>/geom_XXXX.obj. Keyed by nV + first-vertex
		// hash so the same VB doesn't spam files every frame. Cap 500.
		int         m_DumpObjMax     = 0;
		int         m_DumpObjCount   = 0;
		const char* m_DumpObjDir     = 0;
		TArray<uint32> m_DumpObjSeen;
		static uint32 HashVerts(const SUIVert* _p, int _n, int _mvpTag)
		{
			uint32 h = 2166136261u ^ (uint32)_n ^ (uint32)_mvpTag;
			int nSample = _n < 8 ? _n : 8;
			for (int i = 0; i < nSample; ++i)
			{
				uint32 x = *(const uint32*)&_p[i].x;
				uint32 y = *(const uint32*)&_p[i].y;
				uint32 z = *(const uint32*)&_p[i].z;
				h = (h ^ x) * 16777619u;
				h = (h ^ y) * 16777619u;
				h = (h ^ z) * 16777619u;
			}
			return h;
		}
		void DumpGeomOBJ(const char* _Tag, const SUIVert* _pV, int _nV,
		                 const uint16* _pI, int _nI, GLenum _Prim)
		{
			if (m_DumpObjMax <= 0 || m_DumpObjCount >= m_DumpObjMax) return;
			if (!_pV || _nV <= 0 || !_pI || _nI <= 0) return;
			if (_Prim != GL_TRIANGLES && _Prim != GL_TRIANGLE_STRIP && _Prim != GL_TRIANGLE_FAN)
				return;
			CMat4Dfp32 MVP; m_ModelMat.Multiply(m_ProjMat, MVP);
			int mvpTag = (int)(((const uint32*)&MVP)[0] ^ ((const uint32*)&MVP)[5]);
			uint32 h = HashVerts(_pV, _nV, mvpTag);
			for (int i = 0; i < m_DumpObjSeen.Len(); ++i)
				if (m_DumpObjSeen[i] == h) return;
			m_DumpObjSeen.Add(h);
			static int sApplyMVP = -1;
			if (sApplyMVP < 0)
			{
				const char* e = getenv("RIDDICK_OBJ_APPLY_MVP");
				sApplyMVP = (e && *e && *e != '0') ? 1 : 0;
			}
			char path[512];
			snprintf(path, sizeof(path), "%s/geom_%04d_%s_v%d_i%d%s.obj",
				m_DumpObjDir, m_DumpObjCount, _Tag, _nV, _nI,
				sApplyMVP ? "_mvp" : "");
			FILE* f = fopen(path, "w");
			if (!f) return;
			int Tex0 = m_pCurAttrib ? (int)m_pCurAttrib->m_TextureID[0] : 0;
			fprintf(f, "# openriddick GLES3 geom dump #%d  tag=%s  Tex0=%d  applyMVP=%d\n",
				m_DumpObjCount, _Tag, Tex0, sApplyMVP);
			fprintf(f, "# nV=%d nI=%d prim=0x%04x\n", _nV, _nI, (unsigned)_Prim);
			const float* m = (const float*)&MVP;
			fprintf(f, "# MVP: %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g\n",
				m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7],
				m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]);
			for (int i = 0; i < _nV; ++i)
			{
				float x = _pV[i].x, y = _pV[i].y, z = _pV[i].z;
				if (sApplyMVP)
				{
					// Engine is row-vector: clip = v_row * MVP (same numeric
					// layout as GL's column-major M * v_col).
					const float w1 = 1.0f;
					const float cx = x*m[0] + y*m[4] + z*m[8]  + w1*m[12];
					const float cy = x*m[1] + y*m[5] + z*m[9]  + w1*m[13];
					const float cz = x*m[2] + y*m[6] + z*m[10] + w1*m[14];
					const float cw = x*m[3] + y*m[7] + z*m[11] + w1*m[15];
					const float iw = (cw != 0.0f) ? (1.0f / cw) : 1.0f;
					x = cx * iw; y = cy * iw; z = cz * iw;
				}
				fprintf(f, "v %.6f %.6f %.6f\n", x, y, z);
			}
			for (int i = 0; i < _nV; ++i)
				fprintf(f, "vt %.6f %.6f\n", _pV[i].u, _pV[i].v);
			if (_Prim == GL_TRIANGLES)
			{
				for (int i = 0; i + 2 < _nI; i += 3)
				{
					int a = _pI[i] + 1, b = _pI[i+1] + 1, c = _pI[i+2] + 1;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a,a, b,b, c,c);
				}
			}
			else if (_Prim == GL_TRIANGLE_STRIP)
			{
				for (int i = 0; i + 2 < _nI; ++i)
				{
					int a = _pI[i] + 1, b = _pI[i+1] + 1, c = _pI[i+2] + 1;
					if (i & 1) { int t = b; b = c; c = t; }
					if (a==b || b==c || a==c) continue;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a,a, b,b, c,c);
				}
			}
			else
			{
				int a0 = _pI[0] + 1;
				for (int i = 1; i + 1 < _nI; ++i)
				{
					int b = _pI[i] + 1, c = _pI[i+1] + 1;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a0,a0, b,b, c,c);
				}
			}
			fclose(f);
			++m_DumpObjCount;
			if (m_DumpObjCount == m_DumpObjMax)
				fprintf(stderr, "[GEOM-DUMP] hit cap %d files in %s\n",
					m_DumpObjMax, m_DumpObjDir);
		}

		// Common draw: submits _nInd 16-bit indices with GL primitive
		// _GLPrim, using the currently-set geometry (m_Geom) + attrib
		// (m_pCurAttrib) + captured matrices.
		void DrawIndexed(GLenum _GLPrim, uint16* _pInd, int _nInd)
		{
			if (!_pInd || _nInd <= 0) return;
			if (!m_bGLInited) InitGLResources();
			if (!m_UIShader.IsValid()) return;

			// Flush deferred attrib/matrix state (mirrors the PS3
			// backend: engine mutates its own stack, then expects
			// the backend to reify GL state at draw time). Without
			// this our virtual Attrib_Set/Matrix_SetRender never
			// fire and m_pCurAttrib stays null -> no texture ever
			// binds, m_TextureID lookups all return 0.
			if (m_AttribChanged) Attrib_Update();
			if (m_MatrixChanged) Matrix_Update();

			SUIVert* pVerts = 0; int nVerts = 0; bool bMalloced = false;
			if (!BuildInterleavedVerts(pVerts, nVerts, bMalloced)) return;

			// One-shot log: Model, Proj, MVP and vertex[0] → NDC for the
			// first 5 drawcalls of the first frame after startmap load.
			// Diagnoses which matrix mangles vertices.
			static int sMtxLog = 0;
			// Only fire for draws with non-identity Model (i.e. real
			// world geometry, not UI). UI keeps Model=Unit.
			const float* mmChk = (const float*)&m_ModelMat;
			const bool bModelId = (mmChk[0]==1 && mmChk[5]==1 && mmChk[10]==1 && mmChk[15]==1
				&& mmChk[1]==0 && mmChk[2]==0 && mmChk[3]==0
				&& mmChk[4]==0 && mmChk[6]==0 && mmChk[7]==0
				&& mmChk[8]==0 && mmChk[9]==0 && mmChk[11]==0
				&& mmChk[12]==0 && mmChk[13]==0 && mmChk[14]==0);
			if (sMtxLog < 5 && nVerts > 0 && !bModelId && getenv("RIDDICK_DBG_MTX"))
			{
				const float* mm = (const float*)&m_ModelMat;
				const float* mp = (const float*)&m_ProjMat;
				CMat4Dfp32 MVP; m_ModelMat.Multiply(m_ProjMat, MVP);
				const float* mv = (const float*)&MVP;
				const float x = pVerts[0].x, y = pVerts[0].y, z = pVerts[0].z;
				const float cx = x*mv[0] + y*mv[4] + z*mv[8]  + mv[12];
				const float cy = x*mv[1] + y*mv[5] + z*mv[9]  + mv[13];
				const float cz = x*mv[2] + y*mv[6] + z*mv[10] + mv[14];
				const float cw = x*mv[3] + y*mv[7] + z*mv[11] + mv[15];
				fprintf(stderr, "[MTX #%d] nV=%d v0=(%.2f,%.2f,%.2f)\n"
					"  Model rows: [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  Proj  rows: [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  clip=(%g,%g,%g,%g) NDC=(%g,%g,%g)\n",
					sMtxLog, nVerts, x, y, z,
					mm[0],mm[1],mm[2],mm[3], mm[4],mm[5],mm[6],mm[7],
					mm[8],mm[9],mm[10],mm[11], mm[12],mm[13],mm[14],mm[15],
					mp[0],mp[1],mp[2],mp[3], mp[4],mp[5],mp[6],mp[7],
					mp[8],mp[9],mp[10],mp[11], mp[12],mp[13],mp[14],mp[15],
					cx, cy, cz, cw,
					cw != 0 ? cx/cw : 0, cw != 0 ? cy/cw : 0, cw != 0 ? cz/cw : 0);
				fflush(stderr);
				++sMtxLog;
			}

			// Bisect diagnostic: RIDDICK_TEST_TRI=1 → replace vertex data
			// and MVP with a known-good triangle at identity MVP. If a
			// centered white triangle appears where the game normally
			// draws, our pipeline (shader/attribs/streamer/blend) is fine,
			// so the bug must be in engine → SUIVert conversion or MVP.
			// If nothing (or garbage) shows — pipeline itself is broken.
			// TEST_TRI=1: identity MVP + RGB triangle (proves pipeline).
			// TEST_TRI=2: keep engine MVP + RGB triangle (proves whether
			// engine MVP places geometry in a sane on-screen location).
			static int sTestTri = -1;
			if (sTestTri < 0)
			{
				const char* e = getenv("RIDDICK_TEST_TRI");
				sTestTri = (e && *e) ? atoi(e) : 0;
			}
			// A world-scale triangle (units ≈ 1 meter engine scale) so
			// that when engine's world-view MVP is applied the tri lands
			// somewhere sensible in a real map.
			static SUIVert sTri[3] = {
				{ -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xff0000ffu },
				{  1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xff00ff00u },
				{  0.0f,  1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0xffff0000u },
			};
			static uint16 sTriIdx[3] = { 0, 1, 2 };
			if (sTestTri)
			{
				FreeScratch(pVerts, nVerts, bMalloced);
				pVerts = sTri; nVerts = 3; bMalloced = false;
				_pInd = sTriIdx; _nInd = 3;
				if (sTestTri == 1)
				{
					m_ModelMat.Unit();
					m_ProjMat.Unit();
				}
				// mode 2: leave m_ModelMat / m_ProjMat untouched → uses
				// engine's per-drawable MVP. Every drawcall becomes a
				// tiny 2m triangle at that drawable's world position.
			}

			glBindVertexArray(m_VAO);
			CGLES3VBOStreamer::SPushResult vRes = m_Streamer.PushVertices(pVerts, nVerts * (int)sizeof(SUIVert));
			CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices (_pInd,  _nInd  * (int)sizeof(uint16));
			if (!vRes.Ok || !iRes.Ok) { FreeScratch(pVerts, nVerts, bMalloced); return; }

			// Attribs (VBO already bound by PushVertices).
			glBindBuffer(GL_ARRAY_BUFFER, vRes.Buffer);
			SetVertexAttribPointers((intptr_t)vRes.ByteOffset);

			SetupCommonUniforms(true);
			m_DbgTotalVerts += nVerts;
			m_DbgTotalIdx   += _nInd;

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
			GLenum DrawPrim = m_DbgForceWire ? GL_LINE_STRIP : _GLPrim;
			glDrawElements(DrawPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);

			DbgDumpDraw("DrawIndexed", DrawPrim, pVerts, nVerts, _pInd, _nInd);
			DumpGeomOBJ("DrawIndexed", pVerts, nVerts, _pInd, _nInd, _GLPrim);
			FreeScratch(pVerts, nVerts, bMalloced);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
		}

		// Frame-dumper: writes one line + first-vertex sample per drawcall
		// during the target frame, to /tmp/openriddick_frame.txt. Called
		// from DrawIndexed and DrawUserVerts. pVerts may be NULL if the
		// caller freed it already; then only counts + MVP are dumped.
		void DbgDumpDraw(const char* _Tag, GLenum _GLPrim,
		                 const SUIVert* _pVerts, int _nVerts,
		                 const uint16* _pInd, int _nInd)
		{
			if (!m_DbgDumpActive || !m_DbgDumpFp) return;
			if (m_DbgDumpDrawIdx == 0)
			{
				fprintf(m_DbgDumpFp,
					"UNIFORM LOCATIONS: uMVP=%d uUseTex=%d uUseTex1=%d "
					"uTex=%d uTex1=%d uDbgMode=%d uAlphaFunc=%d\n",
					m_UMVPLoc, m_UUseTexLoc, m_UUseTex1Loc,
					m_UTexLoc, m_UTex1Loc, m_UDbgModeLoc, m_UAlphaFuncLoc);
			}
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			uint32 F = m_pCurAttrib ? m_pCurAttrib->m_Flags : 0;
			GLint CurFBO = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurFBO);
			int BlendSrc = -1, BlendDst = -1;
			if (m_pCurAttrib && (F & CRC_FLAGS_BLEND))
			{
				const uint16 SD = m_pCurAttrib->m_SourceDestBlend;
				BlendSrc = SD & 0xff; BlendDst = (SD >> 8) & 0xff;
			}
			fprintf(m_DbgDumpFp,
				"[#%d] %s prim=0x%04x nV=%d nI=%d "
				"flags=0x%08x ZTest=%d ZWrite=%d Cull=%d Blend=%d(%d/%d) "
				"Stencil=%d AlphaCmp=%d FBO=%d RTT=%d\n",
				m_DbgDumpDrawIdx, _Tag, (unsigned)_GLPrim, _nVerts, _nInd,
				F,
				(F & CRC_FLAGS_ZCOMPARE) ? 1 : 0,
				(F & CRC_FLAGS_ZWRITE)   ? 1 : 0,
				(F & CRC_FLAGS_CULL)     ? 1 : 0,
				(F & CRC_FLAGS_BLEND)    ? 1 : 0, BlendSrc, BlendDst,
				(F & CRC_FLAGS_STENCIL)  ? 1 : 0,
				m_pCurAttrib ? (int)m_pCurAttrib->m_AlphaCompare : -1,
				(int)CurFBO, m_bRTTActive ? 1 : 0);
			// All texture slots + whether the engine's TextureID has an
			// uploaded GL name in our cache.
			fprintf(m_DbgDumpFp, "  Tex:");
			for (int s = 0; s < 4 && m_pCurAttrib; ++s)
			{
				int Tid = (int)m_pCurAttrib->m_TextureID[s];
				GLuint GLTex = 0;
				if (Tid > 0 && Tid < (int)m_lGLTex.Len()) GLTex = m_lGLTex[Tid];
				// Also call the exact same path the draw uses right now,
				// so we see what the shader actually got (placeholder etc.).
				GLuint LiveTex = (Tid > 0) ? TextureID_EnsureUploaded(Tid) : 0;
				fprintf(m_DbgDumpFp, " [%d]=%d(cache=%u live=%u)", s, Tid,
					(unsigned)GLTex, (unsigned)LiveTex);
			}
			fprintf(m_DbgDumpFp, " placeholder=%u\n", (unsigned)m_PlaceholderTex);
			fprintf(m_DbgDumpFp, "  SHADER: UseTex=%d UseTex1=%d BoundGL0=%u\n",
				m_DbgLastUseTex, m_DbgLastUseTex1, (unsigned)m_DbgLastBind0);
			const float* m = (const float*)&MVP;
			fprintf(m_DbgDumpFp,
				"  MVP: [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n",
				m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7],
				m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]);
			if (_pVerts && _nVerts > 0)
			{
				float mnx=_pVerts[0].x,mny=_pVerts[0].y,mnz=_pVerts[0].z;
				float mxx=mnx,mxy=mny,mxz=mnz;
				for (int i=1;i<_nVerts;++i)
				{
					const SUIVert& v = _pVerts[i];
					if (v.x<mnx)mnx=v.x; if (v.x>mxx)mxx=v.x;
					if (v.y<mny)mny=v.y; if (v.y>mxy)mxy=v.y;
					if (v.z<mnz)mnz=v.z; if (v.z>mxz)mxz=v.z;
				}
				fprintf(m_DbgDumpFp,
					"  Vert bbox: [%.3f..%.3f, %.3f..%.3f, %.3f..%.3f]\n",
					mnx,mxx,mny,mxy,mnz,mxz);
				int nShow = _nVerts < 3 ? _nVerts : 3;
				for (int i = 0; i < nShow; ++i)
				{
					const SUIVert& v = _pVerts[i];
					fprintf(m_DbgDumpFp,
						"    V[%d] pos=(%.3f,%.3f,%.3f) uv=(%.3f,%.3f) col=0x%08x\n",
						i, v.x, v.y, v.z, v.u, v.v, v.col);
				}
			}
			if (_pInd && _nInd > 0)
			{
				int nShow = _nInd < 9 ? _nInd : 9;
				fprintf(m_DbgDumpFp, "  Idx[0..%d]:", nShow-1);
				for (int i = 0; i < nShow; ++i) fprintf(m_DbgDumpFp, " %u", (unsigned)_pInd[i]);
				fprintf(m_DbgDumpFp, "\n");
			}
			++m_DbgDumpDrawIdx;
		}

		void Render_IndexedTriangles(uint16* _pTriVertIndices, int _nTriangles)
		{
			// nTriangles == 0xffff is the "list of lists" recursion
			// convention (see PS3 backend). Handle it too so BSP4 can
			// batch. For M3 (UI) it will effectively never trigger.
			if (_nTriangles == 0xffff)
			{
				mint* pList = (mint*)_pTriVertIndices;
				int nLists = (int)*pList++;
				for (int i = 0; i < nLists; ++i)
				{
					int nTri = (int)*pList++;
					Render_IndexedTriangles(*((uint16**)pList), nTri);
					++pList;
				}
				return;
			}
			++m_DbgDrawTri;
			DrawIndexed(GL_TRIANGLES, _pTriVertIndices, _nTriangles * 3);
		}

		void Render_IndexedTriangleStrip(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawStrip;
			DrawIndexed(GL_TRIANGLE_STRIP, _pIndices, _Len);
		}

		void Render_IndexedWires(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawWire;
			DrawIndexed(GL_LINES, _pIndices, _Len);
		}

		void Render_IndexedPolygon(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawPoly;
			// GLES has no GL_POLYGON. Treat as triangle fan; caller
			// generally sends convex fan-friendly ordering.
			DrawIndexed(GL_TRIANGLE_FAN, _pIndices, _Len);
		}

		void Render_IndexedPrimitives(uint16* _pPrimStream, int _StreamLen)
		{
			++m_DbgDrawPrim;
			if (!_pPrimStream || _StreamLen <= 0) return;
			CRCPrimStreamIterator It(_pPrimStream, _StreamLen);
			if (!It.IsValid()) return;
			do
			{
				const uint16* pPrim = It.GetCurrentPointer();
				int nInd = *pPrim;
				GLenum Prim = 0;
				switch (It.GetCurrentType())
				{
				case CRC_RIP_TRIANGLES: Prim = GL_TRIANGLES;      nInd *= 3; break;
				case CRC_RIP_TRISTRIP:  Prim = GL_TRIANGLE_STRIP;            break;
				case CRC_RIP_TRIFAN:    Prim = GL_TRIANGLE_FAN;              break;
				default: break;
				}
				if (Prim && nInd > 0)
					DrawIndexed(Prim, (uint16*)(pPrim + 1), nInd);
			}
			while (It.Next());
		}

		// Draw an already-built interleaved vertex array with explicit
		// indices (used by the VBID path; DrawIndexed keeps its own
		// m_Geom-based flow).
		void DrawUserVerts(GLenum _GLPrim, const SUIVert* _pVerts, int _nVerts, const uint16* _pInd, int _nInd)
		{
			if (!_pVerts || _nVerts <= 0 || !_pInd || _nInd <= 0) return;
			if (!m_bGLInited) InitGLResources();
			if (!m_UIShader.IsValid()) return;
			if (m_AttribChanged) Attrib_Update();
			if (m_MatrixChanged) Matrix_Update();

			glBindVertexArray(m_VAO);
			CGLES3VBOStreamer::SPushResult vRes = m_Streamer.PushVertices(_pVerts, _nVerts * (int)sizeof(SUIVert));
			CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices (_pInd,  _nInd  * (int)sizeof(uint16));
			if (!vRes.Ok || !iRes.Ok) return;

			glBindBuffer(GL_ARRAY_BUFFER, vRes.Buffer);
			SetVertexAttribPointers((intptr_t)vRes.ByteOffset);

			SetupCommonUniforms(false);
			m_DbgTotalVerts += _nVerts;
			m_DbgTotalIdx   += _nInd;

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
			GLenum DrawPrim = m_DbgForceWire ? GL_LINE_STRIP : _GLPrim;
			glDrawElements(DrawPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);
			DbgDumpDraw("DrawUserVerts", DrawPrim, _pVerts, _nVerts, _pInd, _nInd);
			DumpGeomOBJ("DrawUserVerts", _pVerts, _nVerts, _pInd, _nInd, _GLPrim);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
		}

		// Fetch component _c (0..3) of vertex _i from a vertex register
		// of format _Fmt, following the range semantics documented at
		// the CRC_VREGFMT_* enum (MRender_Classes.h): F32/I16/U16 raw,
		// NSx normalized signed -1..1, NUx normalized unsigned 0..1.
		// Returns false for unsupported formats.
		static bool VRegFetch(const void* _p, int _Fmt, int _i, int _c, float& _Out)
		{
			switch (_Fmt)
			{
			case CRC_VREGFMT_V1_F32: case CRC_VREGFMT_V2_F32:
			case CRC_VREGFMT_V3_F32: case CRC_VREGFMT_V4_F32:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_F32 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = ((const fp32*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_V1_I16: case CRC_VREGFMT_V2_I16:
			case CRC_VREGFMT_V3_I16: case CRC_VREGFMT_V4_I16:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_I16 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const int16*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_V1_U16: case CRC_VREGFMT_V2_U16:
			case CRC_VREGFMT_V3_U16: case CRC_VREGFMT_V4_U16:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_U16 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const uint16*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_NS1_I16: case CRC_VREGFMT_NS2_I16:
			case CRC_VREGFMT_NS3_I16: case CRC_VREGFMT_NS4_I16:
			{
				int n;
				if      (_Fmt == CRC_VREGFMT_NS1_I16) n = 1;
				else if (_Fmt == CRC_VREGFMT_NS2_I16) n = 2;
				else if (_Fmt == CRC_VREGFMT_NS3_I16) n = 3;
				else                                  n = 4;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const int16*)_p)[_i * n + _c] * (1.0f / 32767.0f);
				return true;
			}
			case CRC_VREGFMT_NU1_I16: case CRC_VREGFMT_NU2_I16:
			{
				const int n = (_Fmt == CRC_VREGFMT_NU1_I16) ? 1 : 2;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const uint16*)_p)[_i * n + _c] * (1.0f / 65535.0f);
				return true;
			}
			default:
				return false;
			}
		}

		// VBID path (precached geometry: frontend cube, models, BSP).
		// No GPU-side cache yet: fetch the CPU data from the engine's
		// VB context each draw, convert to SUIVert and stream it.
		// Positions/UVs accept the F32 and packed I16/U16/NSx/NUx
		// formats; anything else bumps m_DbgVBIDSkipFmt.
		// Convert VBID vertex-register data into our interleaved SUIVert
		// buffer. Returns malloc'd buffer (caller frees) or NULL on failure.
		// Extracted so both Render_VertexBuffer and
		// Render_VertexBuffer_IndexBufferTriangles share it.
		SUIVert* BuildVertsFromVBB(CRC_BuildVertexBuffer& VBB, int& _nV_out)
		{
			_nV_out = 0;
			const int nV = VBB.m_nV;
			if (nV <= 0) return NULL;

			const void* pPos = VBB.m_lpVReg[CRC_VREG_POS];
			const int PosFmt = VBB.m_Format.GetFormat(CRC_VREG_POS);
			{
				float Dummy;
				if (!pPos || !VRegFetch(pPos, PosFmt, 0, 0, Dummy))
				{
					++m_DbgVBIDSkipFmt;
					m_DbgVBIDLastSkip = PosFmt;
					return NULL;
				}
			}

			const void* pUV  = VBB.m_lpVReg[CRC_VREG_TEXCOORD0];
			const int   UVFmt = VBB.m_Format.GetFormat(CRC_VREG_TEXCOORD0);
			const void* pUV1 = VBB.m_lpVReg[CRC_VREG_TEXCOORD1];
			const int   UV1Fmt = VBB.m_Format.GetFormat(CRC_VREG_TEXCOORD1);
			const uint32_t* pCol = 0;
			if (VBB.m_Format.GetFormat(CRC_VREG_COLOR) == CRC_VREGFMT_N4_COL)
				pCol = (const uint32_t*)VBB.m_lpVReg[CRC_VREG_COLOR];

			// Per-register scale+offset (packed formats hold values as
			// raw*Scale+Offset; without applying we get "spikes" as the
			// user observed with the OBJ dumper).
			const bool bPosTx = (VBB.m_TransformEnable & (1u << CRC_VREG_POS)) != 0;
			const CRC_VRegTransform& PosTx = VBB.m_lTransform[CRC_VREG_POS];
			const bool bUVTx  = pUV && (VBB.m_TransformEnable & (1u << CRC_VREG_TEXCOORD0)) != 0;
			const CRC_VRegTransform& UVTx  = VBB.m_lTransform[CRC_VREG_TEXCOORD0];
			const bool bUV1Tx = pUV1 && (VBB.m_TransformEnable & (1u << CRC_VREG_TEXCOORD1)) != 0;
			const CRC_VRegTransform& UV1Tx = VBB.m_lTransform[CRC_VREG_TEXCOORD1];

			SUIVert* pVerts = (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!pVerts) return NULL;
			for (int i = 0; i < nV; ++i)
			{
				VRegFetch(pPos, PosFmt, i, 0, pVerts[i].x);
				VRegFetch(pPos, PosFmt, i, 1, pVerts[i].y);
				VRegFetch(pPos, PosFmt, i, 2, pVerts[i].z);
				if (bPosTx)
				{
					pVerts[i].x = pVerts[i].x * PosTx.m_Scale.k[0] + PosTx.m_Offset.k[0];
					pVerts[i].y = pVerts[i].y * PosTx.m_Scale.k[1] + PosTx.m_Offset.k[1];
					pVerts[i].z = pVerts[i].z * PosTx.m_Scale.k[2] + PosTx.m_Offset.k[2];
				}
				pVerts[i].u = pVerts[i].v = 0.0f;
				if (pUV)
				{
					if (!VRegFetch(pUV, UVFmt, i, 0, pVerts[i].u)) pVerts[i].u = 0.0f;
					if (!VRegFetch(pUV, UVFmt, i, 1, pVerts[i].v)) pVerts[i].v = 0.0f;
					if (bUVTx)
					{
						pVerts[i].u = pVerts[i].u * UVTx.m_Scale.k[0] + UVTx.m_Offset.k[0];
						pVerts[i].v = pVerts[i].v * UVTx.m_Scale.k[1] + UVTx.m_Offset.k[1];
					}
				}
				pVerts[i].u1 = pVerts[i].v1 = 0.0f;
				if (pUV1)
				{
					if (!VRegFetch(pUV1, UV1Fmt, i, 0, pVerts[i].u1)) pVerts[i].u1 = 0.0f;
					if (!VRegFetch(pUV1, UV1Fmt, i, 1, pVerts[i].v1)) pVerts[i].v1 = 0.0f;
					if (bUV1Tx)
					{
						pVerts[i].u1 = pVerts[i].u1 * UV1Tx.m_Scale.k[0] + UV1Tx.m_Offset.k[0];
						pVerts[i].v1 = pVerts[i].v1 * UV1Tx.m_Scale.k[1] + UV1Tx.m_Offset.k[1];
					}
				}
				pVerts[i].col = pCol ? PackColorBGRA_to_RGBA(pCol[i]) : 0xffffffffu;
			}
			_nV_out = nV;
			return pVerts;
		}

		void Render_VertexBuffer(int _VBID)
		{
			++m_DbgDrawVBID;
			if (!m_pVBCtx) return;

			CRC_BuildVertexBuffer VBB;
			VBB.Clear();
			m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);
			if (VBB.m_nV <= 0 || !VBB.m_piPrim || !VBB.m_nPrim) return;

			int nV = 0;
			SUIVert* pVerts = BuildVertsFromVBB(VBB, nV);
			if (!pVerts) return;

			// Walk the primitive stream: header word = index count,
			// then indices; type from the stream iterator.
			CRCPrimStreamIterator It(VBB.m_piPrim, VBB.m_nPrim);
			if (It.IsValid())
			{
				do
				{
					const uint16* pPrim = It.GetCurrentPointer();
					int nInd = *pPrim;
					GLenum Prim;
					switch (It.GetCurrentType())
					{
					case CRC_RIP_TRIANGLES: Prim = GL_TRIANGLES;      nInd *= 3; break;
					case CRC_RIP_TRISTRIP:  Prim = GL_TRIANGLE_STRIP;            break;
					case CRC_RIP_TRIFAN:    Prim = GL_TRIANGLE_FAN;              break;
					default:                Prim = 0;                            break;
					}
					if (Prim && nInd > 0)
						DrawUserVerts(Prim, pVerts, nV, pPrim + 1, nInd);
				}
				while (It.Next());
			}
			free(pVerts);
		}

		// BSP2 solid-world path. VBID delivers positions/UV/UV1/color;
		// IBID delivers the shared index pool (its m_piPrim). _PrimOffset
		// is a 16-bit index count into that pool, _nTriangles*3 indices
		// starting there form the triangle list. Mirrors PS3
		// CRCPS3GCM::Render_VertexBuffer_IndexBufferTriangles
		// (MRenderPS3_Render.cpp:245-267). Without this override, the
		// base CRC_Core stub (MRender.cpp:6031) silently drops every
		// SLC-cluster mesh submitted from CXR_Model_BSP2, so the entire
		// world stays invisible while shadow-volumes (which use the
		// separate Render_VertexBuffer path with almost-black colour)
		// come through faintly — matched Pa1_TheDream symptom exactly.
		void Render_VertexBuffer_IndexBufferTriangles(uint _VBID, uint _IBID,
		                                              uint _nTriangles, uint _PrimOffset)
		{
			++m_DbgDrawVBID;
			if (!m_pVBCtx || _nTriangles == 0) return;

			CRC_BuildVertexBuffer VBB;
			VBB.Clear();
			m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);
			int nV = 0;
			SUIVert* pVerts = BuildVertsFromVBB(VBB, nV);
			if (!pVerts) return;

			// Fetch the IB (usually a separate VB whose m_piPrim is the
			// shared index pool). May be the same as _VBID.
			const uint16* pIdx = VBB.m_piPrim;
			int nPrimIB = VBB.m_nPrim;
			CRC_BuildVertexBuffer IBB;
			if (_IBID != _VBID)
			{
				IBB.Clear();
				m_pVBCtx->VB_Get(_IBID, IBB, VB_GETFLAGS_BUILD);
				pIdx = IBB.m_piPrim;
				nPrimIB = IBB.m_nPrim;
			}

			// Diagnostic (RIDDICK_DBG_GL=1, first ~20 calls): print resolved
			// VB/IB details and a sample of indices so we can see whether the
			// shared index pool addresses vertices beyond nV (would cause
			// stretched garbage triangles).
			static int s_nDbg = 0;
			if (m_DbgEnabled && s_nDbg < 20 && pIdx)
			{
				const uint16* pFirst = pIdx + _PrimOffset;
				const int nIdxDraw = (int)(_nTriangles * 3);
				uint16 imin = 0xffff, imax = 0;
				const int nSample = Min(nIdxDraw, 12);
				for (int k = 0; k < nSample; ++k)
				{
					uint16 v = pFirst[k];
					if (v < imin) imin = v;
					if (v > imax) imax = v;
				}
				fprintf(stderr, "[GLES3-VBIT] VBID=%u IBID=%u nV=%d nPrimIB=%d off=%u nTri=%u  first idx=[%u,%u,%u,%u,%u,%u]  range=%u..%u\n",
					_VBID, _IBID, nV, nPrimIB, _PrimOffset, _nTriangles,
					(unsigned)pFirst[0], (unsigned)pFirst[1], (unsigned)pFirst[2],
					(unsigned)pFirst[3], (unsigned)pFirst[4], (unsigned)pFirst[5],
					(unsigned)imin, (unsigned)imax);
				++s_nDbg;
			}

			if (pIdx)
				DrawUserVerts(GL_TRIANGLES, pVerts, nV, pIdx + _PrimOffset, _nTriangles * 3);
			free(pVerts);
		}
		void Render_Wire(const CVec3Dfp32& _v0, const CVec3Dfp32& _v1, CPixel32 _Color){}
		void Render_WireStrip(const CVec3Dfp32* _pV, const uint16* _piV, int _nVertices, CPixel32 _Color){}
		void Render_WireLoop(const CVec3Dfp32* _pV, const uint16* _piV, int _nVertices, CPixel32 _Color){}

		virtual void Geometry_PrecacheFlush(){}
		virtual void Geometry_PrecacheBegin( int _Count ){}
		virtual void Geometry_PrecacheEnd(){}
		virtual void Geometry_Precache(int _VBID){}
		virtual CDisplayContext* GetDC(){ return m_pDisplayContext;}

		void Register(CScriptRegisterContext & _RegContext){}

	};

	TPtr<CRC_GLES3> m_spRenderContext;


	virtual CRenderContext* GetRenderContext(class CRCLock* _pLock)
	{
		if (!m_spRenderContext)
		{
			m_spRenderContext = MNew(CRC_GLES3);
			m_spRenderContext->m_pDisplayContext = this;
			m_spRenderContext->Create(this, "");

		}

		return m_spRenderContext;
	}

	virtual int Win32_CreateFromWindow(void* _hWnd, int _Flags = 0)
	{
		return 0;
	}

	virtual int Win32_CreateWindow(int _WS, void* _pWndParent, int _Flags = 0)
	{
		return 0;
	}

	virtual void Win32_ProcessMessages()
	{
	}

	virtual void* Win32_GethWnd(int _iWnd = 0)
	{
		return 0;
	}

	virtual void Parser_Modes()
	{
	}

	virtual void Register(CScriptRegisterContext & _RegContext)
	{
		CDisplayContext::Register(_RegContext);
	}

};

MRTC_IMPLEMENT_DYNAMIC_NO_IGNORE(CDisplayContextSDL2, CDisplayContext);

#endif // PLATFORM_LINUX
