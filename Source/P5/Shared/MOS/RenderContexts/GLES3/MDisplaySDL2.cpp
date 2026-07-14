
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

#ifdef PLATFORM_LINUX

#include <SDL.h>
#include <GLES3/gl3.h>

class CDisplayContextSDL2 : public CDisplayContext
{
protected:
	MRTC_DECLARE;
public:

	CImage m_Image;
	SDL_Window* m_pWindow;
	SDL_GLContext m_GLContext;
	int m_Width, m_Height;

	CDisplayContextSDL2()
	{
		m_pWindow = NULL;
		m_GLContext = NULL;
		m_Width = 1280;
		m_Height = 720;
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
			m_Width, m_Height, SDL_WINDOW_OPENGL);
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
			// Pump the SDL event queue (input translation arrives with
			// CInputContext_SDL2); keep the window responsive.
			SDL_Event Event;
			while (SDL_PollEvent(&Event))
			{
				if (Event.type == SDL_QUIT)
					exit(0);
			}
			SDL_GL_SwapWindow(m_pWindow);
			glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
		

		void Internal_RenderPolygon(int _nV, const CVec3Dfp32* _pV, const CVec3Dfp32* _pN, const CVec4Dfp32* _pCol = NULL, const CVec4Dfp32* _pSpec = NULL, /*const fp32* _pFog = NULL,*/
									const CVec4Dfp32* _pTV0 = NULL, const CVec4Dfp32* _pTV1 = NULL, const CVec4Dfp32* _pTV2 = NULL, const CVec4Dfp32* _pTV3 = NULL, int _Color = 0xffffffff)
		{
		}

		CRC_GLES3()
		{
		}

		~CRC_GLES3()
		{
		}

		void Create(CObj* _pContext, const char* _pParams)
		{
			CRC_Core::Create(_pContext, _pParams);

			MACRO_GetSystem;

			m_Caps_TextureFormats = -1;
			m_Caps_DisplayFormats = -1;
			m_Caps_ZFormats = -1;
			m_Caps_StencilDepth = 8;
			m_Caps_AlphaDepth = 8;
			m_Caps_Flags = -1;

			m_Caps_nMultiTexture = 16;
			m_Caps_nMultiTextureCoords = 8;
			m_Caps_nMultiTextureEnv = 8;

		}

		const char* GetRenderingStatus() { return ""; }
		virtual void Flip_SetInterval(int _nFrames){};

		void Attrib_Set(CRC_Attributes* _pAttrib){}
		void Attrib_SetAbsolute(CRC_Attributes* _pAttrib){}
		void Matrix_SetRender(int _iMode, const CMat4Dfp32* _pMatrix){}

		virtual void Texture_PrecacheFlush(){}
		virtual void Texture_PrecacheBegin( int _Count ){}
		virtual void Texture_PrecacheEnd(){}
		virtual void Texture_Precache(int _TextureID){}

		virtual int Texture_GetBackBufferTextureID() {return 0;}
		virtual int Texture_GetFrontBufferTextureID() {return 0;}
		virtual int Texture_GetZBufferTextureID() {return 0;}
		virtual int Geometry_GetVBSize(int _VBID) {return 0;}

		void Render_IndexedTriangles(uint16* _pTriVertIndices, int _nTriangles){}
		void Render_IndexedTriangleStrip(uint16* _pIndices, int _Len){}
		void Render_IndexedWires(uint16* _pIndices, int _Len){}
		void Render_IndexedPolygon(uint16* _pIndices, int _Len){}
		void Render_IndexedPrimitives(uint16* _pPrimStream, int _StreamLen){}
		void Render_VertexBuffer(int _VBID){}
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
