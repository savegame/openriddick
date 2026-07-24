#include "PCH.h"

#ifdef PLATFORM_LINUX

#include "GLES3_Shader.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

CGLES3Shader::CGLES3Shader()
	: m_Program(0), m_nUCache(0)
{
	for (int i = 0; i < MAX_UNIFORM_CACHE; ++i)
	{
		m_UCache[i].pName = 0;
		m_UCache[i].Location = -1;
	}
}

CGLES3Shader::~CGLES3Shader()
{
	Destroy();
}

void CGLES3Shader::Destroy()
{
	if (m_Program)
	{
		glDeleteProgram(m_Program);
		m_Program = 0;
	}
	m_nUCache = 0;
}

GLuint CGLES3Shader::CompileStage(GLenum _Type, const char* _pSrc, const char* _pDebugName, const char* _pStageName)
{
	GLuint sh = glCreateShader(_Type);
	if (!sh)
		return 0;
	glShaderSource(sh, 1, &_pSrc, 0);
	glCompileShader(sh);

	GLint ok = GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLint LogLen = 0;
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &LogLen);
		char* pLog = (char*)malloc((LogLen > 1) ? LogLen : 1);
		if (pLog)
		{
			glGetShaderInfoLog(sh, LogLen, 0, pLog);
			fprintf(stderr, "[GLES3] %s: %s compile failed:\n%s\n",
				_pDebugName ? _pDebugName : "<unnamed>", _pStageName, pLog);
			free(pLog);
		}
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

bool CGLES3Shader::Build(const char* _pVertSrc, const char* _pFragSrc, const char* _pDebugName)
{
	Destroy();

	GLuint Vs = CompileStage(GL_VERTEX_SHADER,   _pVertSrc, _pDebugName, "VS");
	if (!Vs) return false;
	GLuint Fs = CompileStage(GL_FRAGMENT_SHADER, _pFragSrc, _pDebugName, "FS");
	if (!Fs) { glDeleteShader(Vs); return false; }

	m_Program = glCreateProgram();
	if (!m_Program) { glDeleteShader(Vs); glDeleteShader(Fs); return false; }

	glAttachShader(m_Program, Vs);
	glAttachShader(m_Program, Fs);
	glLinkProgram(m_Program);
	glDetachShader(m_Program, Vs);
	glDetachShader(m_Program, Fs);
	glDeleteShader(Vs);
	glDeleteShader(Fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(m_Program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		GLint LogLen = 0;
		glGetProgramiv(m_Program, GL_INFO_LOG_LENGTH, &LogLen);
		char* pLog = (char*)malloc((LogLen > 1) ? LogLen : 1);
		if (pLog)
		{
			glGetProgramInfoLog(m_Program, LogLen, 0, pLog);
			fprintf(stderr, "[GLES3] %s: link failed:\n%s\n",
				_pDebugName ? _pDebugName : "<unnamed>", pLog);
			free(pLog);
		}
		glDeleteProgram(m_Program);
		m_Program = 0;
		return false;
	}
	return true;
}

void CGLES3Shader::Use() const
{
	if (m_Program)
		glUseProgram(m_Program);
}

int CGLES3Shader::UniformLocation(const char* _pName)
{
	if (!m_Program || !_pName)
		return -1;
	for (int i = 0; i < m_nUCache; ++i)
	{
		if (m_UCache[i].pName == _pName || strcmp(m_UCache[i].pName, _pName) == 0)
			return m_UCache[i].Location;
	}
	int Loc = glGetUniformLocation(m_Program, _pName);
	if (m_nUCache < MAX_UNIFORM_CACHE)
	{
		m_UCache[m_nUCache].pName = _pName;
		m_UCache[m_nUCache].Location = Loc;
		++m_nUCache;
	}
	return Loc;
}

int CGLES3Shader::AttribLocation(const char* _pName) const
{
	if (!m_Program || !_pName)
		return -1;
	return glGetAttribLocation(m_Program, _pName);
}

void CGLES3Shader::SetInt   (int _Loc, int _V)                                const { if (_Loc >= 0) glUniform1i(_Loc, _V); }
void CGLES3Shader::SetFloat (int _Loc, float _V)                              const { if (_Loc >= 0) glUniform1f(_Loc, _V); }
void CGLES3Shader::SetVec4  (int _Loc, float _X, float _Y, float _Z, float _W)const { if (_Loc >= 0) glUniform4f(_Loc, _X, _Y, _Z, _W); }
void CGLES3Shader::SetMat4  (int _Loc, const float* _p16)                     const { if (_Loc >= 0) glUniformMatrix4fv(_Loc, 1, GL_FALSE, _p16); }

#endif // PLATFORM_LINUX
