// GLES3 shader-program wrapper (Phase 4 milestone M0).
//
// Small self-contained helper: compile a vertex+fragment source pair,
// link, cache the resulting GLuint, and expose uniform-location
// lookups. Nothing engine-specific here -- CRC_GLES3 owns instances
// and drives the semantics.

#pragma once

#ifdef PLATFORM_LINUX

#include <GLES3/gl3.h>

class CGLES3Shader
{
public:
	CGLES3Shader();
	~CGLES3Shader();

	// Compile vs+fs source, link. Returns true on success. On failure
	// writes the GL info log to stderr and leaves the program at 0.
	bool Build(const char* _pVertSrc, const char* _pFragSrc, const char* _pDebugName = 0);

	// Bind for subsequent draws. No-op if Build has never succeeded.
	void Use() const;

	// Cached uniform lookup (linear scan; UI is small).
	int  UniformLocation(const char* _pName);
	int  AttribLocation(const char* _pName) const;

	// Direct uniform setters (only what M1..M3 need for the UI pipeline).
	void SetInt   (int _Loc, int _V) const;
	void SetFloat (int _Loc, float _V) const;
	void SetVec4  (int _Loc, float _X, float _Y, float _Z, float _W) const;
	void SetMat4  (int _Loc, const float* _p16) const;

	GLuint GetProgram() const { return m_Program; }
	bool   IsValid()    const { return m_Program != 0; }

	void Destroy();

private:
	GLuint m_Program;

	static const int MAX_UNIFORM_CACHE = 16;
	struct UCache { const char* pName; int Location; };
	UCache m_UCache[MAX_UNIFORM_CACHE];
	int    m_nUCache;

	// Non-copyable.
	CGLES3Shader(const CGLES3Shader&);
	CGLES3Shader& operator=(const CGLES3Shader&);

	static GLuint CompileStage(GLenum _Type, const char* _pSrc, const char* _pDebugName, const char* _pStageName);
};

#endif // PLATFORM_LINUX
