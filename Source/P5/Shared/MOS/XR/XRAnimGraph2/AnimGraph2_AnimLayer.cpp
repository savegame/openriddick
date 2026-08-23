//--------------------------------------------------------------------------------

#include "PCH.h"

#include <stdio.h>	// [AG2LAYER] field dump
#include <stdlib.h>	// getenv

// Opacity is not stored in the PC v5/v6 record at all (see the layout note in
// Read()), so every layer keeps the default. Kept as counters only so the
// FULLANIMLAYERS census in AnimGraph2_IO.cpp still has something to print.
int g_AG2LayerOpacityRestored = 0;
int g_AG2LayerOpacityKept = 0;
#include "Mda.h"
#include "MFile.h"
#include "AnimGraph2_AnimLayer.h"

//--------------------------------------------------------------------------------
//- AnimLayer_Full ---------------------------------------------------------------
//--------------------------------------------------------------------------------

void CXRAG2_AnimLayer::Clear()
{
	m_iAnim = CXRAG2_AnimLayer::GetDefaultAnimIndex();
	m_AnimFlags = AG2_ANIMFLAGS_NULL;
	m_iBaseJoint = CXRAG2_AnimLayer::GetDefaultBaseJointIndex();
	m_iMergeOperator = CXRAG2_AnimLayer::GetDefaultMergeOperator();
	m_iTimeControlProperty = CXRAG2_AnimLayer::GetDefaultTimeControlProperty();
	m_TimeOffset = CXRAG2_AnimLayer::GetDefaultTimeOffset();
	m_TimeScale = CXRAG2_AnimLayer::GetDefaultTimeScale();
	m_Opacity = CXRAG2_AnimLayer::GetDefaultOpacity();
}

//--------------------------------------------------------------------------------

void CXRAG2_AnimLayer::Read(CCFile* _pFile, int _Ver)
{
	switch(_Ver)
	{
		case XR_ANIMGRAPH2_VERSION3:
		{
			_pFile->ReadLE(m_TimeOffset);
			_pFile->ReadLE(m_TimeScale);
			_pFile->ReadLE(m_Opacity);

			_pFile->ReadLE(m_iAnim);
			int16 Flags;
			_pFile->ReadLE(Flags);
			m_AnimFlags = Flags;
			_pFile->ReadLE(m_iBaseJoint);
			_pFile->ReadLE(m_iMergeOperator);
			_pFile->ReadLE(m_iTimeControlProperty);
		}
		case XR_ANIMGRAPH2_VERSION:
			{
				_pFile->ReadLE(m_TimeOffset);
				_pFile->ReadLE(m_TimeScale);
				_pFile->ReadLE(m_Opacity);

				_pFile->ReadLE(m_iAnim);
				_pFile->ReadLE(m_AnimFlags);
				_pFile->ReadLE(m_iBaseJoint);
				_pFile->ReadLE(m_iMergeOperator);
				_pFile->ReadLE(m_iTimeControlProperty);
			}
		break;

		case XR_ANIMGRAPH2_VERSION5:
		case XR_ANIMGRAPH2_VERSION6:
			{
				// PC v5/v6 layout, taken from the retail binary rather than
				// guessed (GameWorld_Win32_x86_dll_decomp.c, the function that
				// ends in Error_static("CXRAG2_StateAnim::Read",
				// "Unsupported version %.4x")). Its case 5/6 reads, in order:
				//
				//   +0x00  4  TimeOffset            (fp32)
				//   +0x04  4  TimeScale             (fp32)
				//   +0x08  4  AnimFlags             (uint32)   <-- 32-bit
				//   +0x0c  2  iAnim                 (int16)
				//   +0x0e  1  <field the PC keeps at +0x0e; no member here>
				//   +0x0f  1  iBaseJoint            (uint8)
				//   +0x10  1  iMergeOperator        (uint8)
				//   +0x11  1  iTimeControlProperty  (uint8)
				//                                   = 18 bytes
				//
				// 18 is exactly the stride measured from the file
				// ([AG2FMT] FULLANIMLAYERS stride == consumed == 18, remainder
				// 0 on every graph), which confirms the reading.
				//
				// Two things were wrong before, and they explain the whole
				// NaN-skeleton bug:
				//  * bytes 8..11 were read as a float into m_Opacity. They are
				//    really AnimFlags, which is 0 for most layers -- so every
				//    layer got opacity 0.0. GetAnimLayers computes
				//    LayerBlend = Blend * GetOpacity() and drops the layer when
				//    that is zero, so no layer survived, CXR_Skeleton::EvalAnim
				//    found no full-body layer, aborted, and poisoned all 120
				//    bones with QNaN -- hence the camera dropping to the feet.
				//  * AnimFlags was taken from the single byte at +0x0e, so the
				//    real flags (VALUECOMPARE and friends) were lost entirely.
				// Opacity is simply not stored in v5/v6 -- the PC binary reads
				// no third float here at all, and in its v4 case it reads that
				// float straight into a discarded stack local. So it keeps the
				// default from Clear().
				_pFile->ReadLE(m_TimeOffset);
				_pFile->ReadLE(m_TimeScale);
				_pFile->ReadLE(m_AnimFlags);
				_pFile->ReadLE(m_iAnim);
				uint8 Unknown0E;
				_pFile->ReadLE(Unknown0E);
				_pFile->ReadLE(m_iBaseJoint);
				_pFile->ReadLE(m_iMergeOperator);
				_pFile->ReadLE(m_iTimeControlProperty);

				m_Opacity = CXRAG2_AnimLayer::GetDefaultOpacity();

				// RIDDICK_DBG_AG2FMT=1: decoded fields of the first layers.
				// Plausible data is base=0 on most layers, scale near 1 and
				// opacity 1.0 (defaulted).
				{
					static int s_On = -1;
					if (s_On < 0)
					{
						const char* e = getenv("RIDDICK_DBG_AG2FMT");
						s_On = (e && *e && *e != '0') ? 1 : 0;
					}
					static int s_nLogged = 0;
					if (s_On && s_nLogged < 24)
					{
						++s_nLogged;
						fprintf(stderr, "[AG2LAYER] ofs=%.3f scale=%.3f opacity=%.3f "
							"iAnim=%d flags=0x%08x u0e=%u base=%u merge=%u timeCtrl=%u\n",
							m_TimeOffset, m_TimeScale, m_Opacity, (int)m_iAnim,
							(unsigned)m_AnimFlags, (unsigned)Unknown0E,
							(unsigned)m_iBaseJoint,
							(unsigned)m_iMergeOperator, (unsigned)m_iTimeControlProperty);
						fflush(stderr);
					}
				}
			}
		break;

		default:
			Error_static("CXRAG2_StateAnim::Read", CStrF("Unsupported version %.4x", _Ver));
	}
}

//--------------------------------------------------------------------------------

void CXRAG2_AnimLayer::Write(CCFile* _pFile)
{
#ifndef	PLATFORM_CONSOLE
	_pFile->WriteLE(m_TimeOffset);
	_pFile->WriteLE(m_TimeScale);
	_pFile->WriteLE(m_Opacity);

	_pFile->WriteLE(m_iAnim);
	_pFile->WriteLE(m_AnimFlags);
	_pFile->WriteLE(m_iBaseJoint);
	_pFile->WriteLE(m_iMergeOperator);
	_pFile->WriteLE(m_iTimeControlProperty);
#endif	// PLATFORM_CONSOLE
}

//--------------------------------------------------------------------------------
#ifndef CPU_LITTLEENDIAN
void CXRAG2_AnimLayer::SwapLE()
{
	::SwapLE(m_iAnim);
	::SwapLE(m_AnimFlags);
	::SwapLE(m_iBaseJoint);
	::SwapLE(m_iMergeOperator);
	::SwapLE(m_iTimeControlProperty);
	::SwapLE(m_TimeOffset);
	::SwapLE(m_TimeScale);
	::SwapLE(m_Opacity);
}
#endif
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
