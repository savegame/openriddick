#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_VBOStreamer.h"

CGLES3VBOStreamer::CGLES3VBOStreamer()
	: m_VBO(0), m_IBO(0), m_VBOSize(0), m_IBOSize(0), m_VBOHead(0), m_IBOHead(0)
{
}

CGLES3VBOStreamer::~CGLES3VBOStreamer()
{
	Destroy();
}

void CGLES3VBOStreamer::Destroy()
{
	if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
	if (m_IBO) { glDeleteBuffers(1, &m_IBO); m_IBO = 0; }
	m_VBOSize = m_IBOSize = 0;
	m_VBOHead = m_IBOHead = 0;
}

bool CGLES3VBOStreamer::Create(int _VBOBytes, int _IBOBytes)
{
	Destroy();
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_IBO);
	if (!m_VBO || !m_IBO) { Destroy(); return false; }

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, _VBOBytes, 0, GL_STREAM_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, _IBOBytes, 0, GL_STREAM_DRAW);

	m_VBOSize = _VBOBytes;
	m_IBOSize = _IBOBytes;
	m_VBOHead = 0;
	m_IBOHead = 0;
	return true;
}

CGLES3VBOStreamer::SPushResult CGLES3VBOStreamer::PushVertices(const void* _pBytes, int _Size)
{
	SPushResult R = { false, 0, 0 };
	if (!m_VBO || _Size <= 0 || _Size > m_VBOSize) return R;

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	if (m_VBOHead + _Size > m_VBOSize)
	{
		// Orphan and rewind.
		glBufferData(GL_ARRAY_BUFFER, m_VBOSize, 0, GL_STREAM_DRAW);
		m_VBOHead = 0;
	}
	glBufferSubData(GL_ARRAY_BUFFER, m_VBOHead, _Size, _pBytes);
	R.Ok         = true;
	R.Buffer     = m_VBO;
	R.ByteOffset = m_VBOHead;
	m_VBOHead   += _Size;
	return R;
}

CGLES3VBOStreamer::SPushResult CGLES3VBOStreamer::PushIndices(const void* _pBytes, int _Size)
{
	SPushResult R = { false, 0, 0 };
	if (!m_IBO || _Size <= 0 || _Size > m_IBOSize) return R;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
	if (m_IBOHead + _Size > m_IBOSize)
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_IBOSize, 0, GL_STREAM_DRAW);
		m_IBOHead = 0;
	}
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, m_IBOHead, _Size, _pBytes);
	R.Ok         = true;
	R.Buffer     = m_IBO;
	R.ByteOffset = m_IBOHead;
	m_IBOHead   += _Size;
	return R;
}

#endif // PLATFORM_LINUX
