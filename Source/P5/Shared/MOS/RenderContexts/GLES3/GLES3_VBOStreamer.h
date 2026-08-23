// Transient VBO/EBO streamer for the GLES3 backend (Phase 4 milestone M0).
//
// Ring-buffered dynamic buffers for per-draw geometry uploads: the engine
// hands us a chunk of vertex bytes + a chunk of index bytes per draw, we
// return byte offsets suitable for glVertexAttribPointer / glDrawElements.
// Ring wraps by re-orphaning the whole buffer (glBufferData(NULL) then
// glBufferSubData); persistent-mapped rings are a M4+ optimisation.

#pragma once

#ifdef PLATFORM_LINUX

#include <GLES3/gl3.h>

class CGLES3VBOStreamer
{
public:
	CGLES3VBOStreamer();
	~CGLES3VBOStreamer();

	// _VBOBytes: capacity of the vertex ring. _IBOBytes: index ring.
	// Both are orphaned+refilled when a request would overflow.
	bool Create(int _VBOBytes = 1 * 1024 * 1024, int _IBOBytes = 256 * 1024);
	void Destroy();

	struct SPushResult
	{
		bool  Ok;
		GLuint Buffer;
		int    ByteOffset;
	};

	// Copy _pBytes[0.._Size) into the vertex ring. Binds GL_ARRAY_BUFFER
	// as a side effect. ByteOffset in SPushResult is the offset within
	// SPushResult.Buffer, suitable as the last arg of
	// glVertexAttribPointer when the buffer is bound.
	SPushResult PushVertices(const void* _pBytes, int _Size);

	// Same for indices; binds GL_ELEMENT_ARRAY_BUFFER.
	SPushResult PushIndices(const void* _pBytes, int _Size);

	GLuint GetVBO() const { return m_VBO; }
	GLuint GetIBO() const { return m_IBO; }

	// Bumped every time the vertex ring is orphaned (wraps). A cached
	// (Buffer, ByteOffset) from an earlier PushVertices stays valid only
	// while this value is unchanged.
	int GetVBGeneration() const { return m_VBGen; }

private:
	GLuint m_VBO, m_IBO;
	int    m_VBOSize, m_IBOSize;
	int    m_VBOHead, m_IBOHead;
	int    m_VBGen;

	CGLES3VBOStreamer(const CGLES3VBOStreamer&);
	CGLES3VBOStreamer& operator=(const CGLES3VBOStreamer&);
};

#endif // PLATFORM_LINUX
