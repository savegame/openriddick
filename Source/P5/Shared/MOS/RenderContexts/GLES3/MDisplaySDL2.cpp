
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
			const unsigned char Magenta[4] = { 255, 0, 255, 255 };
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
			fprintf(stderr, "[GLES3-RTT] id=%d FBO ok %dx%d  colorTex=%u fbo=%u\n",
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

		void DbgInit()
		{
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
		void DbgFramePrint()
		{
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
			if (F & CRC_FLAGS_CULL)
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
		bool BuildInterleavedVerts(SUIVert*& _pOut, int& _nOut)
		{
			const int nV = (int)m_Geom.m_nV;
			if (nV <= 0 || !m_Geom.m_pV) { _pOut = 0; _nOut = 0; return false; }
			static SUIVert sScratch[16384];
			SUIVert* p = (nV <= 16384) ? sScratch : (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!p) return false;

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

		void FreeScratch(SUIVert* _p, int _nV)
		{
			static SUIVert sMarker;  (void)sMarker;
			if (_nV > 16384) free(_p);
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
			if (m_pCurAttrib && m_pCurAttrib->m_AlphaCompare != CRC_COMPARE_ALWAYS)
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

			SUIVert* pVerts = 0; int nVerts = 0;
			if (!BuildInterleavedVerts(pVerts, nVerts)) return;

			glBindVertexArray(m_VAO);
			CGLES3VBOStreamer::SPushResult vRes = m_Streamer.PushVertices(pVerts, nVerts * (int)sizeof(SUIVert));
			CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices (_pInd,  _nInd  * (int)sizeof(uint16));
			FreeScratch(pVerts, nVerts);
			if (!vRes.Ok || !iRes.Ok) return;

			// Attribs (VBO already bound by PushVertices).
			glBindBuffer(GL_ARRAY_BUFFER, vRes.Buffer);
			SetVertexAttribPointers((intptr_t)vRes.ByteOffset);

			SetupCommonUniforms(true);
			m_DbgTotalVerts += nVerts;
			m_DbgTotalIdx   += _nInd;

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
			glDrawElements(_GLPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
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
			// CRC_RIP_STREAM: sub-list-encoded stream. Feed through
			// CRC_Core's helper that turns it into a plain triangle
			// list, then draw that. Same trick PS3 uses (see
			// Geometry_BuildTriangleListFromPrimitives).
			// For M3 simplicity assume caller sends triangles only.
			DrawIndexed(GL_TRIANGLES, _pPrimStream, _StreamLen);
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
			glDrawElements(_GLPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);

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
		void Render_VertexBuffer(int _VBID)
		{
			++m_DbgDrawVBID;
			if (!m_pVBCtx) return;

			CRC_BuildVertexBuffer VBB;
			VBB.Clear();
			m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);
			const int nV = VBB.m_nV;
			if (nV <= 0 || !VBB.m_piPrim || !VBB.m_nPrim) return;

			const void* pPos = VBB.m_lpVReg[CRC_VREG_POS];
			const int PosFmt = VBB.m_Format.GetFormat(CRC_VREG_POS);
			{
				float Dummy;
				if (!pPos || !VRegFetch(pPos, PosFmt, 0, 0, Dummy))
				{
					++m_DbgVBIDSkipFmt;
					m_DbgVBIDLastSkip = PosFmt;
					return;
				}
			}

			const void* pUV  = VBB.m_lpVReg[CRC_VREG_TEXCOORD0];
			const int   UVFmt = VBB.m_Format.GetFormat(CRC_VREG_TEXCOORD0);
			const void* pUV1 = VBB.m_lpVReg[CRC_VREG_TEXCOORD1];
			const int   UV1Fmt = VBB.m_Format.GetFormat(CRC_VREG_TEXCOORD1);
			const uint32_t* pCol = 0;
			if (VBB.m_Format.GetFormat(CRC_VREG_COLOR) == CRC_VREGFMT_N4_COL)
				pCol = (const uint32_t*)VBB.m_lpVReg[CRC_VREG_COLOR];

			SUIVert* pVerts = (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!pVerts) return;
			for (int i = 0; i < nV; ++i)
			{
				VRegFetch(pPos, PosFmt, i, 0, pVerts[i].x);
				VRegFetch(pPos, PosFmt, i, 1, pVerts[i].y);
				VRegFetch(pPos, PosFmt, i, 2, pVerts[i].z);
				pVerts[i].u = pVerts[i].v = 0.0f;
				if (pUV)
				{
					if (!VRegFetch(pUV, UVFmt, i, 0, pVerts[i].u)) pVerts[i].u = 0.0f;
					if (!VRegFetch(pUV, UVFmt, i, 1, pVerts[i].v)) pVerts[i].v = 0.0f;
				}
				pVerts[i].u1 = pVerts[i].v1 = 0.0f;
				if (pUV1)
				{
					if (!VRegFetch(pUV1, UV1Fmt, i, 0, pVerts[i].u1)) pVerts[i].u1 = 0.0f;
					if (!VRegFetch(pUV1, UV1Fmt, i, 1, pVerts[i].v1)) pVerts[i].v1 = 0.0f;
				}
				pVerts[i].col = pCol ? PackColorBGRA_to_RGBA(pCol[i]) : 0xffffffffu;
			}

			// Walk the primitive stream: header word = index count,
			// then indices; type from the stream iterator.
			CRCPrimStreamIterator It(VBB.m_piPrim, VBB.m_nPrim);
			if (It.IsValid())
			{
				do
				{
					const uint16* pPrim = It.GetCurrentPointer();
					const int nInd = *pPrim;
					GLenum Prim;
					switch (It.GetCurrentType())
					{
					case CRC_RIP_TRIANGLES: Prim = GL_TRIANGLES;      break;
					case CRC_RIP_TRISTRIP:  Prim = GL_TRIANGLE_STRIP; break;
					case CRC_RIP_TRIFAN:    Prim = GL_TRIANGLE_FAN;   break;
					default:                Prim = 0;                 break;
					}
					if (Prim && nInd > 0)
						DrawUserVerts(Prim, pVerts, nV, pPrim + 1, nInd);
				}
				while (It.Next());
			}
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
