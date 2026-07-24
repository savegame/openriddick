#include "PCH.h"

#include "MDisplayPresent.h"

SRiddickPresent g_RiddickPresent;

/*
	Coordinate derivation. The composite shader maps window UV
	(GL, bottom-left origin) to FBO UV as:
	  0:   (u, v)
	  90:  (v, 1-u)
	  180: (1-u, 1-v)
	  270: (1-v, u)
	The pixel-space formulas below are those mappings converted to
	top-left-origin pixel coordinates.
*/

void SRiddickPresent::WindowToFBO(int _wx, int _wy, int& _fx, int& _fy) const
{
	const float uw = (m_WinW > 0) ? (float)_wx / (float)m_WinW : 0.0f;
	const float vw = (m_WinH > 0) ? 1.0f - (float)_wy / (float)m_WinH : 0.0f;
	float uf, vf;
	switch (m_Rotate)
	{
	default:  uf = uw;        vf = vw;        break;
	case 90:  uf = vw;        vf = 1.0f - uw; break;
	case 180: uf = 1.0f - uw; vf = 1.0f - vw; break;
	case 270: uf = 1.0f - vw; vf = uw;        break;
	}
	_fx = (int)(uf * (float)m_FBOW);
	_fy = (int)((1.0f - vf) * (float)m_FBOH);
}

void SRiddickPresent::RotateDelta(int _dx, int _dy, int& _ox, int& _oy) const
{
	switch (m_Rotate)
	{
	default:  _ox =  _dx; _oy =  _dy; break;
	case 90:  _ox = -_dy; _oy =  _dx; break;
	case 180: _ox = -_dx; _oy = -_dy; break;
	case 270: _ox =  _dy; _oy = -_dx; break;
	}
}
