//--------------------------------------------------------------------------------

#include "PCH.h"

#include <stdio.h>	// [AG2LAYER] field dump
#include <stdlib.h>	// getenv
#include "MDA.h"
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
				// v5/v6 (PC): Opacity is stored, AnimFlags is a single byte
				_pFile->ReadLE(m_TimeOffset);
				_pFile->ReadLE(m_TimeScale);
				_pFile->ReadLE(m_Opacity);

				_pFile->ReadLE(m_iAnim);
				uint8 AnimFlags;
				_pFile->ReadLE(AnimFlags);
				m_AnimFlags = AnimFlags;
				_pFile->ReadLE(m_iBaseJoint);
				_pFile->ReadLE(m_iMergeOperator);
				_pFile->ReadLE(m_iTimeControlProperty);

				// RIDDICK_DBG_AG2FMT=1: dump the decoded fields of the first
				// few layers.
				//
				// The record SIZE is already confirmed correct -- [AG2FMT]
				// shows FULLANIMLAYERS with stride == consumed == 18 and no
				// remainder. But a correct size does not prove a correct field
				// ORDER: 12 bytes of floats plus 2+1+1+1+1 can be permuted
				// several ways, and the PC tool may not use ours. That matters
				// because a wrong m_iBaseJoint makes every layer look like a
				// partial (non-full-body) layer, and CXR_Skeleton::EvalAnim
				// then finds no full layer, aborts, and poisons all 120 bones
				// with QNaN -- the camera-in-the-floor bug.
				//
				// Read it as bytes: after the three floats the file holds
				// iAnim (2 bytes LE) and then four single bytes in the order
				// printed here. Plausible data is base=0 for most layers,
				// opacity in [0,1] and timescale near 1.
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
							"iAnim=%d flags=0x%02x base=%u merge=%u timeCtrl=%u\n",
							m_TimeOffset, m_TimeScale, m_Opacity, (int)m_iAnim,
							(unsigned)AnimFlags, (unsigned)m_iBaseJoint,
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
