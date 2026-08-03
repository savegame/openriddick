#include "PCH.h"

#include <stdio.h>	// AG2_ReportBadState() logs to stderr
#include <stdlib.h>	// getenv() for RIDDICK_DBG_ANIMTIME

//--------------------------------------------------------------------------------

#ifdef	AG2_DEBUG
static bool bDebug = false;
#endif
//--------------------------------------------------------------------------------

#define DEFAULT_INVBLENDINDURATION	(0.0f)
#define DEFAULT_INVBLENDOUTDURATION	(0.0f)

#define PROPERTY_BOOL_GUNPLAYDISABLED 19
#define CHAR_STATEFLAG_LAYERADJUSTFOROFFSET	0x01000000
#define CHAR_STATEFLAG_TAGUSEANIMCAMERA 0x04000000
#define CHAR_STATEFLAG_NOBLENDOUT		0x00004000
#define CHAR_STATEFLAGHI_SKIPFORCEKEEP 0x00004000
#define CHAR_STATEFLAGHI_SYNCANIM	0x00000004
#define CHAR_STATEFLAGHI_ADJUSTSTATETIMESCALE	0x00000020
#define CHAR_STATEFLAGHI_PERFECTMOVEMENT	(0x00000040|0x00000080)
#define CHAR_STATEFLAGHI_IKSYSTEM_SKIPANIMATION		0x00080000
#define CHAR_STATEFLAGHI_DISABLEREFRESH		0x00200000
#define CHAR_STATEFLAGHI_ADAPTIVETIMESCALE	0x10000000
#define CHAR_STATEFLAGHI_RANDOMIZE_ENTER	0x80000000
//--------------------------------------------------------------------------------

#include "WAG2I_StateInst.h"
#include "WAG2I.h"
#include "WAG2I_Context.h"
#include "WAG2I_StateInstPacked.h"
#include "WAG2_ClientData.h"

//--------------------------------------------------------------------------------
// Linux-port hardening: invalid (animgraph, state) pairs must not be fatal.
//
// A dozen places below fetch the state description with nothing but an
// M_ASSERT and then dereference the result unconditionally.  M_ASSERT only
// prints and continues, and CXRAG2::GetState() legitimately returns NULL for
// an out-of-range index, so such a pair is a plain SIGSEGV -- which is what
// happens in Pa1_Pit, on a render worker thread inside
// CWObject_Character::OnClientRender -> ... -> GetAnimLayers().
//
// Two ways the pair goes bad:
//  * the animgraph resource failed to load, so it has zero states while the
//    state index stayed at whatever the previous animgraph handed out;
//  * UnpackSIP()/OnClientUpdate() overwrite m_iState from the move token but
//    leave m_bHasAnimation set from the previous state when the new state
//    does not resolve -- so the render path believes there is animation to
//    fetch and walks straight into the NULL.
// The second one is fixed at the source (both sites now clear the flag), the
// first can only be diagnosed from a log: AG2_ReportBadState() prints the
// animgraph name and its state count, so an empty resource is recognisable.
// NOTE: must sit AFTER the WAG2I includes above -- CXRAG2 is only declared
// there; before them the name is unknown and GCC degrades it to int.
static void AG2_ReportBadState(const char* _pWhere, const CXRAG2* _pAnimGraph, int _iAnimGraph, int _iState)
{
	static int s_nReported = 0;
	if (s_nReported >= 32)
		return;
	++s_nReported;

	CXRAG2* pAG = (CXRAG2*)_pAnimGraph;	// GetName()/GetNumStates() are non-const
	fprintf(stderr, "[AG2] %s: state not found -- iAnimGraph=%d iState=%d nStates=%d ag='%s'\n",
		_pWhere, _iAnimGraph, _iState,
		pAG ? (int)pAG->GetNumStates() : -1,
		pAG ? pAG->GetName().Str() : "<null>");
	fflush(stderr);
}

static fp32 Sinc(fp32 _x)
{
	MAUTOSTRIP(Sinc, 0.0f);
	//	return _x;

	return M_Sin((_x - 0.5f)*_PI)*0.5f + 0.5f;
}

//--------------------------------------------------------------------------------

CWAG2I_StateInstance::CWAG2I_StateInstance(const CWAG2I_SIID* _pSIID, CWAG2I* _pAG2I, CWAG2I_Token* _pToken)
{
	// Misc
	m_DirtyFlag = 0;
	m_pAG2I = _pAG2I;
	//m_pToken = _pToken;
	m_iAnimGraph = _pSIID->GetAnimGraphIndex();
	m_Flags = 0;
	m_EnqueueTime = _pSIID->GetEnqueueTime();
	m_iState = AG2_STATEINDEX_NULL;
	m_Priority = 0;

	// Enter
	//m_iEnterAction = _pSIID->GetEnterActionIndex();
	m_iEnterMoveToken = _pSIID->GetEnterMoveTokenIndex();
	m_EnterTime = AG2I_UNDEFINEDTIME;

	m_Enter_AnimBlendDuration = 0;
	m_Enter_AnimBlendDelay = 0;
	m_Enter_AnimTimeOffset = 0;

	m_iLoopControlAnimLayer = 0;
	m_AnimLoopDuration = 0;
	m_TimeScale = 1.0f;
	m_bHasAnimation = false;
	m_bSkipForceKeep = 0;
	m_bGotLeaveState = false;
	m_bGotLeaveStateInit = false;
	m_bGotEnterStateInit = false;
	m_bGotTerminate = false;
	m_bHasSyncAnim = false;
	m_bHasAdaptiveTimeScale = false;
	m_bHasPerfectMovement = false;
	m_bNoPrevExactMove = false;
	m_bShouldDisableRefresh = false;
	m_LeaveStateFrom = 0;
	m_bDontBlendOut = false;

	m_SyncAnimScale = -1.0f;
	m_AnchorTime = 0.0f;
	m_Duration1 = 0.0f;
	m_Duration2 = 0.0f;

	m_BlendInStartTime = AG2I_UNDEFINEDTIME;
	m_BlendInEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;

	// Leave
	//m_iLeaveAction = AG2_ACTIONINDEX_NULL;
	m_iLeaveMoveToken = AG2_MOVETOKENINDEX_NULL;
	m_LeaveTime = AG2I_UNDEFINEDTIME;

	m_Leave_AnimBlendDuration = 0;
	m_Leave_AnimBlendDelay = 0;

	m_BlendOutStartTime = AG2I_UNDEFINEDTIME;
	m_BlendOutEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;

	// Proceed Condition
	m_ProceedCondition_bDefault = true;
	m_ProceedCondition_iProperty = 0;
	m_ProceedCondition_PropertyType = 0;
	m_ProceedCondition_iOperator = 0;
	m_ProceedCondition_Constant = 0;
}

void CWAG2I_StateInstance::Clear()
{
	// Misc
	m_DirtyFlag = 0;
	m_pAG2I = NULL;
//	m_pToken = NULL;
	m_iAnimGraph = -1;
	m_Flags = 0;
	m_EnqueueTime.MakeInvalid();
	m_iState = AG2_STATEINDEX_NULL;
	m_Priority = 0;

	// Enter
	m_iEnterMoveToken = -1;
	m_EnterTime = AG2I_UNDEFINEDTIME;

	m_Enter_AnimBlendDuration = 0;
	m_Enter_AnimBlendDelay = 0;
	m_Enter_AnimTimeOffset = 0;

	m_iLoopControlAnimLayer = 0;
	m_AnimLoopDuration = 0;
	m_TimeScale = 1.0f;
	m_bHasAnimation = false;
	m_bSkipForceKeep = 0;
	m_bGotLeaveState = false;
	m_bGotLeaveStateInit = false;
	m_bGotEnterStateInit = false;
	m_bGotTerminate = false;
	m_bHasSyncAnim = false;
	m_bHasAdaptiveTimeScale = false;
	m_bDontBlendOut = false;
	m_bHasPerfectMovement = false;
	m_bNoPrevExactMove = false;
	m_bShouldDisableRefresh = false;
	m_LeaveStateFrom = 0;

	m_SyncAnimScale = -1.0f;
	m_AnchorTime = 0.0f;
	m_Duration1 = 0.0f;
	m_Duration2 = 0.0f;

	m_BlendInStartTime = AG2I_UNDEFINEDTIME;
	m_BlendInEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;

	// Leave
	//m_iLeaveAction = AG2_ACTIONINDEX_NULL;
	m_iLeaveMoveToken = AG2_MOVETOKENINDEX_NULL;
	m_LeaveTime = AG2I_UNDEFINEDTIME;

	m_Leave_AnimBlendDuration = 0;
	m_Leave_AnimBlendDelay = 0;

	m_BlendOutStartTime = AG2I_UNDEFINEDTIME;
	m_BlendOutEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;

	// Proceed Condition
	m_ProceedCondition_bDefault = true;
	m_ProceedCondition_iProperty = 0;
	m_ProceedCondition_PropertyType = 0;
	m_ProceedCondition_iOperator = 0;
	m_ProceedCondition_Constant = 0;
}

void CWAG2I_StateInstance::operator =(const CWAG2I_StateInstance& _StateInst)
{
	m_pAG2I = _StateInst.m_pAG2I;
	m_EnqueueTime = _StateInst.m_EnqueueTime;
	m_EnterTime = _StateInst.m_EnterTime;
	m_LeaveTime = _StateInst.m_LeaveTime;
	m_BlendInStartTime = _StateInst.m_BlendInStartTime;
	m_BlendInEndTime = _StateInst.m_BlendInEndTime;
	m_BlendOutStartTime = _StateInst.m_BlendOutStartTime;
	m_BlendOutEndTime = _StateInst.m_BlendOutEndTime;

	m_AnimTime = _StateInst.m_AnimTime;

	m_AnimLoopDuration = _StateInst.m_AnimLoopDuration;
	m_TimeScale = _StateInst.m_TimeScale;

	m_Enter_AnimBlendDuration = _StateInst.m_Enter_AnimBlendDuration;
	m_Enter_AnimBlendDelay = _StateInst.m_Enter_AnimBlendDelay;
	m_Enter_AnimTimeOffset = _StateInst.m_Enter_AnimTimeOffset;

	m_InvBlendInDuration = _StateInst.m_InvBlendInDuration;

	m_Leave_AnimBlendDuration = _StateInst.m_Leave_AnimBlendDuration;
	m_Leave_AnimBlendDelay = _StateInst.m_Leave_AnimBlendDelay;

	m_InvBlendOutDuration = _StateInst.m_InvBlendOutDuration;

	m_ProceedCondition_Constant = _StateInst.m_ProceedCondition_Constant;

	m_SyncPoints1 = _StateInst.m_SyncPoints1;
	m_SyncPoints2 = _StateInst.m_SyncPoints2;

	m_lTimeScales1 = _StateInst.m_lTimeScales1;
	m_lTimeScales2 = _StateInst.m_lTimeScales2;
	m_lTimes = _StateInst.m_lTimes;
	m_liLastEventKey = _StateInst.m_liLastEventKey;
	m_lNumLoops = _StateInst.m_lNumLoops;
	
	m_SyncAnimScale = _StateInst.m_SyncAnimScale;
	m_AnchorTime = _StateInst.m_AnchorTime;
	m_Duration1 = _StateInst.m_Duration1;
	m_Duration2 = _StateInst.m_Duration2;

	m_iState = _StateInst.m_iState;
	m_iEnterMoveToken = _StateInst.m_iEnterMoveToken;
	m_iLeaveMoveToken = _StateInst.m_iLeaveMoveToken;

	m_Flags = _StateInst.m_Flags;
	m_Priority = _StateInst.m_Priority;
	m_iLoopControlAnimLayer = _StateInst.m_iLoopControlAnimLayer;
	m_ProceedCondition_iProperty = _StateInst.m_ProceedCondition_iProperty;
	m_ProceedCondition_PropertyType = _StateInst.m_ProceedCondition_PropertyType;
	m_ProceedCondition_iOperator = _StateInst.m_ProceedCondition_iOperator;
	m_iAnimGraph = _StateInst.m_iAnimGraph;
	m_DirtyFlag = _StateInst.m_DirtyFlag;

	m_LeaveStateFrom = _StateInst.m_LeaveStateFrom;

	m_bHasAnimation = _StateInst.m_bHasAnimation;
	m_ProceedCondition_bDefault = _StateInst.m_ProceedCondition_bDefault;
	m_bSkipForceKeep = _StateInst.m_bSkipForceKeep;
	m_bGotLeaveState = _StateInst.m_bGotLeaveState;
	m_bGotLeaveStateInit = _StateInst.m_bGotLeaveStateInit;
	m_bGotEnterStateInit = _StateInst.m_bGotEnterStateInit;
	m_bGotTerminate = _StateInst.m_bGotTerminate;
	m_bHasSyncAnim = _StateInst.m_bHasSyncAnim;
	m_bHasAdaptiveTimeScale = _StateInst.m_bHasAdaptiveTimeScale;
	m_bDontBlendOut = _StateInst.m_bDontBlendOut;
	m_bHasPerfectMovement = _StateInst.m_bHasPerfectMovement;
	m_bNoPrevExactMove = _StateInst.m_bNoPrevExactMove;
	m_bShouldDisableRefresh = _StateInst.m_bShouldDisableRefresh;
}

//--------------------------------------------------------------------------------

fp32 CWAG2I_StateInstance::GetAnimLoopDuration(const CWAG2I_Context* _pContext)
{
	MSCOPESHORT(CWAG2I_StateInstance::GetAnimLoopDuration);

	CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	if (!pAnimGraph || !_pContext->m_pWorldData)
		return 0.0f;

	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (pState == NULL || !pState->GetNumAnimLayers())
		return 0.0f;

	const CXRAG2_AnimLayer* pAnimLayer = pState->GetNumAnimLayers() > 0 ? pAnimGraph->GetAnimLayer(pState->GetBaseAnimLayerIndex() + m_iLoopControlAnimLayer) : NULL;
	if (pAnimLayer == NULL)
		return 0.0f;

	const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, pAnimLayer->GetAnimIndex());
	if (pAnimLayerSeq == NULL)
		return 0;

	fp32 TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
	fp32 Duration = pAnimLayerSeq->GetDuration();
	return TimeScale > 0.0f ? Duration / TimeScale : Duration;
}

fp32 CWAG2I_StateInstance::GetLoopTimeScale(const CWAG2I_Context* _pContext) const
{
	MSCOPESHORT(CWAG2I_StateInstance::GetTimeScale);

	const CXRAG2_State* pState = m_pAG2I->GetState(m_iState, m_iAnimGraph);
	if (pState == NULL || !pState->GetNumAnimLayers())
		return 1.0f;

	const CXRAG2_AnimLayer* pAnimLayer = m_pAG2I->GetAnimLayer(pState->GetBaseAnimLayerIndex() + m_iLoopControlAnimLayer,m_iAnimGraph);
	M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::GetLoopTimeScale: INVALID ANIMLAYER");
	if (!pAnimLayer)
		return 1.0f;

	return pAnimLayer->GetTimeScale();
}

//--------------------------------------------------------------------------------
/*
fp32 CWAG2I_StateInstance::GetAnimTime(CWAG2I_Context* _pContext, int8 _iAnimLayer) const
{
	const CXRAG2_AnimLayer* pAnimLayer = m_pAG2I->GetAnimLayer(_iAnimLayer);
	if (pAnimLayer == NULL)
		return 0;

	spCXR_Anim_SequenceData pAnimLayerSeq = pAnimList->GetAnimSequenceData(pAnimLayer->GetAnimIndex());
	if (pAnimLayerSeq == NULL)
		return 0;

	m_pAG2I->RefreshStateInstanceProperties(_pContext, this);

	fp32 AnimTime;
	AnimTime = m_pAG2I->EvaluateProperty(_pContext, pAnimLayer->GetTimeControlProperty());
	AnimTime = (AnimTime + pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) * pAnimLayer->GetTimeScale();
	AnimTime = pAnimLayerSeq->GetLoopedTime(AnimTime);

	return AnimTime;
}
*/
//--------------------------------------------------------------------------------
/*
fp32 CWAG2I_StateInstance::GetAnimTimeOffset(CWAG2I_Context* _pContext, int8 _iAnimTimeOffsetType)
{
	if (_iAnimTimeOffsetType == AG2_ANIMLAYER_TIMEOFFSETTYPE_INDEPENDENT)
		return 0;

	fp32 AnimTime = GetAnimTime(_pContext);

	if (_iAnimTimeOffsetType == AG2_ANIMLAYER_TIMEOFFSETTYPE_INHERIT)
		return AnimTime;

	if (_iAnimTimeOffsetType == AG2_ANIMLAYER_TIMEOFFSETTYPE_INHERITEQ)
		return (AnimTime / m_AnimLoopDuration);

	return 0;
}
*/
//--------------------------------------------------------------------------------
/*
void CWAG2I_StateInstance::AdjustAnimTimeOffset(CWAG2I_Context* _pContext)
{
	if ((m_EnterTime == AG2I_UNDEFINEDTIME) || (m_Enter_iAnimTimeOffsetType == AG2_ANIMLAYER_TIMEOFFSETTYPE_INDEPENDENT))
		return;

	CWAG2I_SIID SIID(GetEnqueueTime(), GetEnterActionIndex());
	fp32 InheritedAnimTimeOffset = m_pSIQ->GetPrevStateInstanceAnimTimeOffset(_pContext, &SIID, m_Enter_iAnimTimeOffsetType);

	switch (m_Enter_iAnimTimeOffsetType)
	{
		case AG2_ANIMLAYER_TIMEOFFSETTYPE_INDEPENDENT: InheritedAnimTimeOffset = 0; break;
		case AG2_ANIMLAYER_TIMEOFFSETTYPE_INHERIT: break;
		case AG2_ANIMLAYER_TIMEOFFSETTYPE_INHERITEQ: InheritedAnimTimeOffset *= m_AnimLoopDuration; break;
	}

	m_Enter_AnimTimeOffset += InheritedAnimTimeOffset;
}
*/
//--------------------------------------------------------------------------------

bool CWAG2I_StateInstance::ProceedQuery(const CWAG2I_Context* _pContext, fp32& _TimeFraction) const
{
	MSCOPESHORT(CWAG2I_StateInstance::ProceedQuery);
	
	// If no custom condition is specified, use default condition (i.e. have token left this state already?).
	if (m_ProceedCondition_bDefault)
	{
		_TimeFraction = 0;
		return (m_iLeaveMoveToken != AG2_MOVETOKENINDEX_NULL);
	}

	M_ASSERT(m_pAG2I->m_pEvaluator, "!");
	return m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(_pContext, 
													m_ProceedCondition_iProperty,
													m_ProceedCondition_PropertyType,
													m_ProceedCondition_iOperator,
													CAG2Val::From(m_ProceedCondition_Constant),
													_TimeFraction);
}


void CWAG2I_StateInstance::FindBreakoutSequence(const CWAG2I_Context* _pContext, int16& _iSequence, CMTime& _AnimTime) const
{
	const CXRAG2_State* pState = m_pAG2I->GetState(m_iState, m_iAnimGraph);
	// 0x00100000 == CHAR_STATEFLAG2_DISABLEBREAKOUT
	if (!pState || (pState->GetFlags(0) & 0x00100000) || 
		(_pContext->m_GameTime - m_EnterTime).Compare(CMTime::CreateFromSeconds(0.001f)) < 0)
		return;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	if (!pAnimGraph)
		return;
	const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(pState->GetBaseAnimLayerIndex());
	if (pAnimLayer && (pState->GetNumAnimLayers() > 0))
	{
		int16 iAnim = pAnimLayer->GetAnimIndex();

		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (pAnimLayerSeq && (pAnimLayerSeq->m_Flags & ANIM_SEQFLAGS_HASBREAKOUTPOINTS))
		{				
			//			int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
			CMTime ContinousTime;
			if (m_bHasSyncAnim)
			{
				fp32 Time;
				fp32 TimeScale;
				GetSyncAnimTime(_pContext,0,Time,TimeScale);
				ContinousTime = CMTime::CreateFromSeconds(Time);
			}
			else if (m_bHasAdaptiveTimeScale)
			{
				fp32 Time;
				fp32 TimeScale;
				GetAdaptiveTime(_pContext,0,Time,TimeScale);
				ContinousTime = CMTime::CreateFromSeconds(Time);
			}
			else
			{
				ContinousTime = _pContext->m_GameTime - m_EnterTime;// m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();
				ContinousTime = (ContinousTime + CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset)).Scale(pAnimLayer->GetTimeScale());
			}
			
			// Set previous properties
			_iSequence = iAnim;
			_AnimTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);
		}
		else
		{
			_iSequence = -1;
			_AnimTime.Reset();
		}
	}
}

// Only end for now
void CWAG2I_StateInstance::EnterState_InitSyncVelocity(const CWAG2I_Context* _pContext, bool _bStart)
{
	// Check what velocity we want at the end (in units per second)
	fp32 DestSpeed = m_pAG2I->m_pEvaluator->GetDestinationSpeed();
	if (DestSpeed <= 0.0f)
		return;

	// For primary animation check what speed it has in the end and match state timescale to it
	const CXRAG2* pAG = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAG,"CWAG2I_StateInstance::EnterState_InitSyncVelocity: INVALID ANIMGRAPH");
	if (!pAG)
		return;
	const CXRAG2_State* pState = pAG->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState("EnterState_InitSyncVelocity", pAG, m_iAnimGraph, m_iState);
		return;
	}
	if (!pState->GetNumAnimLayers())
		return;

	const CXRAG2_AnimLayer* pAnimLayer = m_pAG2I->GetAnimLayer(pState->GetBaseAnimLayerIndex() + m_iLoopControlAnimLayer,m_iAnimGraph);
	M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::EnterState_InitSyncVelocity: INVALID ANIMLAYER");
	if (!pAnimLayer)
		return;
	const CXR_Anim_SequenceData* pAnimLayerSeq = pAG->GetAnimSequenceData(_pContext->m_pWorldData, pAnimLayer->GetAnimIndex());
	if (!pAnimLayerSeq)
		return;

	fp32 Duration = pAnimLayerSeq->GetDuration();

	vec128 MoveA, MoveB;
	CQuatfp32 RotA,RotB;
	CMTime TimeA,TimeB;
	// Sample over 2 ticks for now
	fp32 SampleTime = _pContext->m_pWPhysState->GetGameTickTime() * 2.0f;
	if (_bStart)
	{
		TimeA.Reset();
		TimeB = CMTime::CreateFromSeconds(SampleTime);
	}
	else
	{
		TimeA = CMTime::CreateFromSeconds(Duration - SampleTime);
		TimeB = CMTime::CreateFromSeconds(Duration);
	}
	pAnimLayerSeq->EvalTrack0(TimeA, MoveA, RotA);
	pAnimLayerSeq->EvalTrack0(TimeB, MoveB, RotB);
	fp32 AnimSpeed = (CVec4Dfp32(MoveB) - CVec4Dfp32(MoveA)).Length() / SampleTime;

	//ConOut(CStrF("AnimSpeed: %f WantedSpeed: %f Scale: %f",AnimSpeed,DestSpeed,DestSpeed/AnimSpeed));

	// Второе незащищённое деление того же рода: у клипа без корневого
	// движения AnimSpeed == 0, и тарировать по нему нечего. DestSpeed уже
	// проверен на > 0 выше, так что без этой проверки получался inf.
	if (AnimSpeed <= 1e-6f)
		return;

	m_TimeScale = DestSpeed / AnimSpeed;
}

bool CWAG2I_StateInstance::EnterState_InitSyncAnims(const CWAG2I_Context* _pContext)
{
	// Assumes state init correctly
	m_bHasSyncAnim = true;
	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	if (!pAnimGraph)
		return false;
	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState("EnterState_InitSyncAnims", pAnimGraph, m_iAnimGraph, m_iState);
		return false;
	}
	// Make sure we have 2 animations in this state
	if (pState->m_nAnimLayers < 2)
		return false;

	// Get animations
	const CXRAG2_AnimLayer* pLayer1 = pAnimGraph->GetAnimLayer(pState->m_iBaseAnimLayer);
	const CXRAG2_AnimLayer* pLayer2 = pAnimGraph->GetAnimLayer(pState->m_iBaseAnimLayer+1);
	M_ASSERT(pLayer1 && pLayer2,"CWAG2I_StateInstance::EnterState_InitSyncAnims Animations invalid");
	if (!pLayer1 || !pLayer2)
		return false;
	// Anim might not be cached
	const CXR_Anim_SequenceData* pAnimLayerSeq1 = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, pLayer1->m_iAnim);
	const CXR_Anim_SequenceData* pAnimLayerSeq2 = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, pLayer2->m_iAnim);
	if (!pAnimLayerSeq1 || !pAnimLayerSeq2)
		return false;

	// Set "global" timescale for both layers (get from first layer)
	m_TimeScale = pLayer1->GetTimeScale();

	// Hmm, keep this?..
	m_Duration1 = pAnimLayerSeq1->GetDuration();
	m_Duration2 = pAnimLayerSeq2->GetDuration();

	pAnimLayerSeq1->FindSyncPoints(m_SyncPoints1);
	pAnimLayerSeq2->FindSyncPoints(m_SyncPoints2);
	// Make sure sync points match up
	int32 Len1 = m_SyncPoints1.m_lPoints.Len();
	if (Len1 != m_SyncPoints2.m_lPoints.Len())
		return false;
	for (int32 i = 0; i < Len1; i++)
	{
		if (m_SyncPoints1.m_lPoints[i].m_Type != m_SyncPoints2.m_lPoints[i].m_Type)
		{
			m_bHasSyncAnim = false;
			ConOutL(CStrF("WARNING: Syncpoints mismatch - iState: %d", m_iState));
			return false;
		}
	}

	// Alrighty then, got the syncpoints, 
	// Scale of animation (0 = anim1, 1 = anim2)
	// When animscale changes, these might be needed
	m_AnchorTime = 0.0f;
	const int32 TimeScaleLen = m_SyncPoints1.m_lPoints.Len() + 1;
	m_lTimeScales1.SetLen(TimeScaleLen);
	m_lTimeScales2.SetLen(TimeScaleLen);
	m_lTimes.SetLen(TimeScaleLen+1);
	m_SyncAnimScale = -1.0f;//m_pAG2I->GetEvaluator()->GetSyncAnimTimeScale();

	return true;
}

// НАЙДЕННЫЙ ДЕФЕКТ (2026-08-03). Здесь рождался NaN, из-за которого
// персонажи «скользят в позе первого кадра» и «телепортируются».
//
// Синхронизация двух анимаций (ходьба/бег — состояния с двумя слоями)
// режет клип по syncpoint'ам и на каждый отрезок считает свой масштаб
// времени: TimeDiv = 1 / (Time - PrevTime). Деление НЕ защищено. Если два
// соседних syncpoint'а совпали по времени, знаменатель ноль:
//   * числитель тоже ноль -> 0 * inf = NaN;
//   * числитель не ноль   -> inf.
// Дальше NaN расходится по обеим веткам сразу:
//   1) GetSyncAnimTime возвращает NaN-время -> LoopedTime NaN -> клип
//      считается в NaN и отдаёт первый кадр => ПОЗА ЗАМИРАЕТ;
//   2) GetAnimVelocity строит TimeB = TimeA + TimeSpan*NaN = NaN, сравнение
//      TimeB < TimeA даёт «зациклились», включается компенсация шва петли
//      и к смещению ЗА ОДИН ТИК прибавляется путь за ВЕСЬ клип
//      (GetTotalTrack0) => СКОРОСТЬ В ДЕСЯТКИ РАЗ ВЫШЕ.
// Замер, на котором это поймано (RIDDICK_DBG_ANIMV, pa2_diner):
//   scale=-nan dMove=116.965 got=3509.0   против здорового got=125.0.
//
// Возвращаем false на вырожденном отрезке — вызывающий ставит масштаб 1.0,
// то есть отрезок играет в натуральном темпе вместо NaN. Заодно один раз
// печатаем сами syncpoint'ы: если их времена нулевые, вырожденность идёт
// от чтения .XSA (ключи ANIM_EVENT_TYPE_SYNC, XRAnim.cpp:1587), и чинить
// надо загрузчик, а не только это место.
static bool Riddick_SyncSliceOK(fp32 _Time, fp32 _PrevTime, int32 _i, int32 _Len,
	int16 _iState, const CXR_Anim_SyncPoints& _P1, const CXR_Anim_SyncPoints& _P2,
	fp32 _Dur1, fp32 _Dur2)
{
	if (_Time - _PrevTime > 1e-6f)
		return true;

	static int s_n = 0;
	if (s_n < 8)
	{
		++s_n;
		fprintf(stderr, "[SYNC] degenerate slice i=%d/%d iState=%d Time=%.5f PrevTime=%.5f dur1=%.3f dur2=%.3f\n",
			(int)_i, (int)_Len, (int)_iState, _Time, _PrevTime, _Dur1, _Dur2);
		fprintf(stderr, "[SYNC]   points1(%d):", (int)_P1.m_lPoints.Len());
		for (int32 k = 0; k < _P1.m_lPoints.Len() && k < 12; k++)
			fprintf(stderr, " %.4f/t%d", _P1.m_lPoints[k].m_Time, (int)_P1.m_lPoints[k].m_Type);
		fprintf(stderr, "\n[SYNC]   points2(%d):", (int)_P2.m_lPoints.Len());
		for (int32 k = 0; k < _P2.m_lPoints.Len() && k < 12; k++)
			fprintf(stderr, " %.4f/t%d", _P2.m_lPoints[k].m_Time, (int)_P2.m_lPoints[k].m_Type);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	return false;
}

void CWAG2I_StateInstance::UpdateSyncAnimScale(const CWAG2I_Context* _pContext, fp32 _SyncAnimScale)
{
	// Get syncanimscale from evaluator
	if (_SyncAnimScale == m_SyncAnimScale)
		return;

	fp32 PrevSyncScale = m_SyncAnimScale;
	m_SyncAnimScale = _SyncAnimScale;

	// Find relative time at current moment, find relative time on the new time and set the anchor 
	// time to that difference
	//CMTime Time = _pContext->m_GameTime - m_EnterTime - CMTime::CreateFromSeconds(m_AnchorTime);
	fp32 PrevDuration = m_lTimes[m_SyncPoints1.m_lPoints.Len()+1];

	// Update anchor time, rescale current animation time to the new target...
	m_AnimLoopDuration = LERP(m_Duration1,m_Duration2,m_SyncAnimScale);
	m_lTimes[0] = 0.0f;

	int32 Len = m_SyncPoints1.m_lPoints.Len();
	for (int32 i = 0; i <= Len; i++)
	{
		fp32 PrevTime = (i > 0 ? LERP(m_SyncPoints1.m_lPoints[i-1].m_Time,m_SyncPoints2.m_lPoints[i-1].m_Time,m_SyncAnimScale) : 0.0f);
		fp32 PrevTime1,PrevTime2;
		if (i > 0)
		{
			PrevTime1 = m_SyncPoints1.m_lPoints[i-1].m_Time;
			PrevTime2 = m_SyncPoints2.m_lPoints[i-1].m_Time;
		}
		else
		{
			PrevTime1 = 0.0f;
			PrevTime2 = 0.0f;
		}
		// Times at this syncpoint should be the one calculated above, find a timescale that will 
		// take us there from prev syncpoint... (or anchorpoint)
		if (i < Len)
		{
			fp32 Time = LERP(m_SyncPoints1.m_lPoints[i].m_Time,m_SyncPoints2.m_lPoints[i].m_Time,m_SyncAnimScale);
			m_lTimes[i+1] = Time;
			if (Riddick_SyncSliceOK(Time, PrevTime, i, Len, m_iState,
					m_SyncPoints1, m_SyncPoints2, m_Duration1, m_Duration2))
			{
				fp32 TimeDiv = 1.0f / (Time - PrevTime);
				m_lTimeScales1[i] = (m_SyncPoints1.m_lPoints[i].m_Time - PrevTime1) * TimeDiv;
				m_lTimeScales2[i] = (m_SyncPoints2.m_lPoints[i].m_Time - PrevTime2) * TimeDiv;
			}
			else
			{
				m_lTimeScales1[i] = 1.0f;
				m_lTimeScales2[i] = 1.0f;
			}
		}
		else
		{
			fp32 Time = LERP(m_Duration1,m_Duration2,m_SyncAnimScale);
			m_lTimes[i+1] = Time;
			if (Riddick_SyncSliceOK(Time, PrevTime, i, Len, m_iState,
					m_SyncPoints1, m_SyncPoints2, m_Duration1, m_Duration2))
			{
				fp32 TimeDiv = 1.0f / (Time - PrevTime);
				m_lTimeScales1[i] = (m_Duration1 - PrevTime1) * TimeDiv;
				m_lTimeScales2[i] = (m_Duration2 - PrevTime2) * TimeDiv;
			}
			else
			{
				m_lTimeScales1[i] = 1.0f;
				m_lTimeScales2[i] = 1.0f;
			}
		}
	}

	//CMTime Time = _pContext->m_GameTime - m_EnterTime - CMTime::CreateFromSeconds(m_AnchorTime);
	//fp32 CurrentRelTime = Time.GetTime() / m_lTimes[m_SyncPoints1.m_lPoints.Len()+1];
	if (PrevSyncScale != -1.0f)
	{
		//CMTime Time = _pContext->m_GameTime - m_EnterTime - CMTime::CreateFromSeconds(m_AnchorTime);
		CMTime StateTime = (_pContext->m_GameTime - m_EnterTime).Scale(m_TimeScale);
		//CMTime StateTime = CMTime::CreateFromSeconds(0.23f);
		CMTime Time = StateTime + CMTime::CreateFromSeconds(m_AnchorTime);
		fp32 CurrentAnimTime = Time.Modulus(PrevDuration).GetTime();
		fp32 CurrentRelTime =  CurrentAnimTime / PrevDuration;
//		fp32 NewAnimTime = Time.Modulus(m_lTimes[m_SyncPoints1.m_lPoints.Len()+1]).GetTime();
		//m_AnchorTime -= CurrentRelTime * m_lTimes[m_SyncPoints1.m_lPoints.Len()+1] - CurrentRelTime * PrevDuration;
		m_AnchorTime = (CMTime::CreateFromSeconds(CurrentRelTime * m_lTimes[m_SyncPoints1.m_lPoints.Len()+1]) - StateTime).Modulus(m_lTimes[m_SyncPoints1.m_lPoints.Len()+1]).GetTime();
	}
}

void CWAG2I_StateInstance::GetSyncAnimTime(const CWAG2I_Context* _pContext, int32 _iSlot, fp32& _Time, fp32& _TimeScale) const
{
	// Assumes we actually have a synced state and sequences are continuous (for now..)
	CMTime Time = (_pContext->m_GameTime - m_EnterTime).Scale(m_TimeScale) + CMTime::CreateFromSeconds(m_AnchorTime + m_Enter_AnimTimeOffset);
	//CMTime Time = CMTime::CreateFromSeconds(0.23f) + CMTime::CreateFromSeconds(m_AnchorTime);
	fp32 AnimTime = Time.Modulus(Max(m_lTimes[m_SyncPoints1.m_lPoints.Len()+1],0.01f)).GetTime();

	/*if (_pContext->m_pWPhysState->IsServer() && _iSlot == 0)
		ConOutL(CStrF("Scale: %f RelTime: %f AnchorT: %f AnimT: %f", m_SyncAnimScale,AnimTime /m_lTimes[m_SyncPoints1.m_lPoints.Len()+1],m_AnchorTime,AnimTime));*/
	
	// Find which timeslice to use
	int32 iSlice = -1;
	int32 Len = m_lTimes.Len() - 1;
	for (int32 i = 0; i < Len; i++)
	{
		if (AnimTime <= m_lTimes[i+1])
		{
			iSlice = i;
			break;
		}
	}

	if (iSlice < 0)
		iSlice = m_lTimes.Len() - 2;
	
	fp32 SliceTime;
	if (_iSlot == 0)
	{
		SliceTime = (iSlice == 0 ? 0.0f : iSlice > m_SyncPoints1.m_lPoints.Len() ? m_Duration1 : m_SyncPoints1.m_lPoints[iSlice-1].m_Time);
		_TimeScale = m_lTimeScales1[iSlice];
	}
	else
	{
		SliceTime = (iSlice == 0 ? 0.0f : iSlice > m_SyncPoints2.m_lPoints.Len() ? m_Duration2 : m_SyncPoints2.m_lPoints[iSlice-1].m_Time);
		_TimeScale = m_lTimeScales2[iSlice];
	}
	 // Get the final times
	_Time = (AnimTime - m_lTimes[iSlice]) * _TimeScale + SliceTime;
}

//--------------------------------------------------------------------------------

// Have adaptive timescales
bool CWAG2I_StateInstance::EnterState_AdaptiveTimeScale(const CWAG2I_Context* _pContext)
{
	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	if (!pAnimGraph)
		return false;
	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState("EnterState_AdaptiveTimeScale", pAnimGraph, m_iAnimGraph, m_iState);
		return false;
	}

	// Get animation
	const CXRAG2_AnimLayer* pLayer = pAnimGraph->GetAnimLayer(pState->m_iBaseAnimLayer);
	M_ASSERT(pLayer,"CWAG2I_StateInstance::EnterState_AdaptiveTimeScale Animation invalid");
	if (!pLayer)
		return false;
	// Anim might not be cached
	const CXR_Anim_SequenceData* pAnimLayerSeq1 = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, pLayer->m_iAnim);
	M_ASSERT(pAnimLayerSeq1,"CWAG2I_StateInstance::EnterState_AdaptiveTimeScale Animation invalid");
	if (!pAnimLayerSeq1)
		return false;

	// Set "global" timescale for both layers (get from first layer)
	m_bHasAdaptiveTimeScale = true;
	m_TimeScale = pLayer->GetTimeScale();

	// Hmm, keep this?..
	m_Duration1 = pAnimLayerSeq1->GetDuration();
	m_AnimLoopDuration = m_Duration1;// * m_TimeScale;

	vec128 Move;
	CQuatfp32 Rot;
	pAnimLayerSeq1->EvalTrack0(CMTime::CreateFromSeconds(m_Duration1),Move,Rot);
	Move = M_VSetW0(Move);
	const fp32 AnimMoveLen = CVec4Dfp32(Move).Length();
	const int PropIdx = (int)pLayer->GetMergeOperator();
	m_pAG2I->GetEvaluator()->SetPropertyFloat(pLayer->GetMergeOperator(),AnimMoveLen);

	// Куда именно записалась длина корневого движения. Индекс свойства --
	// авторское поле m_iMergeOperator из AG2, а формат v6 мы читаем
	// реверсом. Игровой код делит на СВОЙСТВО 8
	// (PROPERTY_FLOAT_ANIMMOVELENGTH, WObj_CharClientData.cpp:1237), так
	// что prop != 8 означает, что знаменатель остаётся нулевым навсегда.
	{
		static int s_n = 0;
		if (s_n < 12)
		{
			++s_n;
			fprintf(stderr, "[ADAPT] enter iState=%d iAnim=%d prop=%d animMoveLen=%.3f dur=%.3f\n",
				(int)m_iState, (int)pLayer->m_iAnim, PropIdx, AnimMoveLen, m_Duration1);
			fflush(stderr);
		}
	}


	// Scale of animation (0 = anim1)
	// When animscale changes, these might be needed
	m_AnchorTime = 0.0f;

	m_SyncAnimScale = -1.0f;
	UpdateAdaptiveTimeScale(_pContext,m_pAG2I->m_pEvaluator->GetAdaptiveTimeScale());

	return true;
}

void CWAG2I_StateInstance::UpdateAdaptiveTimeScale(const CWAG2I_Context* _pContext, fp32 _SyncAnimScale)
{
	// Get syncanimscale from evaluator
	// Max/Min не отсеивают NaN (любое сравнение с NaN ложно), а дальше он
	// уходит прямо в масштаб времени слоя -- ловим явно.
	if (!(_SyncAnimScale > -1.0e6f && _SyncAnimScale < 1.0e6f))
		_SyncAnimScale = 1.0f;
	_SyncAnimScale = Max(0.0f,Min(4.0f,_SyncAnimScale));
	if (_SyncAnimScale == m_SyncAnimScale)
		return;

	fp32 PrevSyncScale = m_SyncAnimScale;
	m_SyncAnimScale = _SyncAnimScale;

	// Find relative time at current moment, find relative time on the new time and set the anchor 
	// time to that difference
	//CMTime Time = _pContext->m_GameTime - m_EnterTime - CMTime::CreateFromSeconds(m_AnchorTime);
	fp32 PrevDuration = m_AnimLoopDuration;

	// Update anchor time, rescale current animation time to the new target...
	m_AnimLoopDuration = m_Duration1;// / m_SyncAnimScale;
	
	if (PrevSyncScale != -1.0f)
	{
		//CMTime Time = _pContext->m_GameTime - m_EnterTime - CMTime::CreateFromSeconds(m_AnchorTime);
		CMTime StateTime = (_pContext->m_GameTime - m_EnterTime).Scale(PrevSyncScale);
		CMTime NewStateTime = (_pContext->m_GameTime - m_EnterTime).Scale(m_SyncAnimScale);
		//CMTime StateTime = CMTime::CreateFromSeconds(0.23f);
		CMTime Time = StateTime + CMTime::CreateFromSeconds(m_AnchorTime);
		fp32 CurrentAnimTime = Time.Modulus(PrevDuration).GetTime();
		fp32 CurrentRelTime =  CurrentAnimTime / PrevDuration;
		//		fp32 NewAnimTime = Time.Modulus(m_lTimes[m_SyncPoints1.m_lPoints.Len()+1]).GetTime();
		//m_AnchorTime -= CurrentRelTime * m_lTimes[m_SyncPoints1.m_lPoints.Len()+1] - CurrentRelTime * PrevDuration;
		m_AnchorTime = (CMTime::CreateFromSeconds(CurrentRelTime * m_AnimLoopDuration) - NewStateTime).Modulus(m_AnimLoopDuration).GetTime();

		/*fp32 TestAnimTime = ((_pContext->m_GameTime - m_EnterTime).Scale(m_SyncAnimScale) + CMTime::CreateFromSeconds(m_AnchorTime)).Modulus(m_AnimLoopDuration).GetTime();
		fp32 TestRelTime =  TestAnimTime / m_AnimLoopDuration;
		int i = 0;*/
	}
}

void CWAG2I_StateInstance::GetAdaptiveTime(const CWAG2I_Context* _pContext, int32 _iSlot, fp32& _Time, fp32& _TimeScale) const
{
	// Assumes we actually have a synced state and sequences are continuous (for now..)
	_TimeScale = m_SyncAnimScale;
	CMTime Time = (_pContext->m_GameTime - m_EnterTime).Scale(_TimeScale) + CMTime::CreateFromSeconds(m_AnchorTime + m_Enter_AnimTimeOffset);
	//CMTime Time = CMTime::CreateFromSeconds(0.23f) + CMTime::CreateFromSeconds(m_AnchorTime);
	_Time = Time.Modulus(m_AnimLoopDuration).GetTime();
	
	/*if (_pContext->m_pWPhysState->IsClient())
		ConOutL(CStrF("Scale: %f Time: %f Anchor: %f Offset: %f",_TimeScale, _Time, m_AnchorTime, m_Enter_AnimTimeOffset));*/
}

//--------------------------------------------------------------------------------

bool CWAG2I_StateInstance::EnterState_Setup(const CWAG2I_Context* _pContext, int16 _iEnterMoveToken)
{
	MSCOPESHORT(CWAG2I_StateInstance::EnterState_Setup);

	m_EnqueueTime = _pContext->m_GameTime;	

	//m_iEnterAction = _iEnterAction;
	m_iEnterMoveToken = _iEnterMoveToken;
	//const CXRAG2_Action* pAction = m_pAG2I->GetAction(m_iEnterAction);
	
	/*if (!m_pAG2I->m_lspAnimGraph2.ValidPos(m_iAnimGraph)) // BUGFIX-CODE - TODO: fix properly
		return false;									  // it crashes on consite when shooting bad guy #2*/

	const CXRAG2_MoveToken* pMoveToken = m_pAG2I->GetMoveToken(m_iEnterMoveToken,m_iAnimGraph);
	if (pMoveToken == NULL)
		return false;

	m_Enter_AnimBlendDuration = pMoveToken->GetAnimBlendDuration();
	m_Enter_AnimBlendDelay = pMoveToken->GetAnimBlendDelay();
	m_Enter_AnimTimeOffset = pMoveToken->GetAnimTimeOffset();
	m_iState = pMoveToken->GetTargetStateIndex() & AG2_TARGETSTATE_INDEXMASK;
	
	const CXRAG2_State* pState = m_pAG2I->GetState(m_iState, m_iAnimGraph);
	if (pState == NULL)
		return false;

	int32 FlagsLo = pState->GetFlags(0);
	int32 FlagsHi = pState->GetFlags(1);
	if (FlagsHi & CHAR_STATEFLAGHI_DISABLEREFRESH)
		m_bShouldDisableRefresh = true;

	if (FlagsHi & CHAR_STATEFLAGHI_ADJUSTSTATETIMESCALE)
		EnterState_InitSyncVelocity(_pContext);
	else if (FlagsHi & CHAR_STATEFLAGHI_ADAPTIVETIMESCALE)
		EnterState_AdaptiveTimeScale(_pContext);
	else
		m_TimeScale = 1.0f;
	if (FlagsLo & CHAR_STATEFLAG_NOBLENDOUT)
		m_bDontBlendOut = true;

	// Randomize entry
	if (FlagsHi & CHAR_STATEFLAGHI_RANDOMIZE_ENTER)
		m_Enter_AnimTimeOffset = Random * 2.0f;	

	m_iLoopControlAnimLayer = pState->GetLoopControlAnimLayerIndex();
	m_AnimLoopDuration = GetAnimLoopDuration(_pContext);

	m_bHasPerfectMovement = FlagsHi & CHAR_STATEFLAGHI_PERFECTMOVEMENT ? 1 : 0;
	m_bNoPrevExactMove = FlagsHi & 0x00000200 ? 1 : 0;
	m_bHasAnimation = false;
	m_bSkipForceKeep = FlagsHi & CHAR_STATEFLAGHI_SKIPFORCEKEEP ? 1 : 0;
	/*if (m_iState == 1726)
		ConOut(CStrF("ForceKeep: %d",m_bSkipForceKeep));*/
	m_liLastEventKey.SetLen(pState->GetNumAnimLayers());
	m_lNumLoops.SetLen(pState->GetNumAnimLayers());
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		m_liLastEventKey[jAnimLayer] = 0;
		m_lNumLoops[jAnimLayer] = 0;
		int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = m_pAG2I->GetAnimLayer(iAnimLayer, m_iAnimGraph);
		if (pAnimLayer == NULL)
			continue;

		int16 iAnim = pAnimLayer->GetAnimIndex();
		if (iAnim != AG2_ANIMINDEX_NULL)
		{
			m_bHasAnimation = true;
			break;
		}
	}

	m_Priority = pState->GetPriority();

	m_EnterTime = AG2I_UNDEFINEDTIME;
	m_BlendInStartTime = AG2I_UNDEFINEDTIME;
	m_BlendInEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;
	
	if (FlagsHi & CHAR_STATEFLAGHI_SYNCANIM)
		EnterState_InitSyncAnims(_pContext);

	return true;
}


//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::EnterState_Initiate(const CWAG2I_Context* _pContext)
{
	MSCOPESHORT(CWAG2I_StateInstance::EnterState_Initiate);
	if (m_bGotEnterStateInit)
		return;
	m_bGotEnterStateInit = true;

	m_EnterTime = _pContext->m_GameTime;
	m_BlendInStartTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay);

	if (m_Enter_AnimBlendDuration < 0.0f)
	{
		m_BlendInEndTime = AG2I_LINKEDTIME;
		m_InvBlendInDuration = m_Enter_AnimBlendDuration;
	}
	else
	{
		m_BlendInEndTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay +  m_Enter_AnimBlendDuration);
		m_InvBlendInDuration = (m_Enter_AnimBlendDuration > 0) ? (1.0f / m_Enter_AnimBlendDuration) : DEFAULT_INVBLENDINDURATION;
	}

	// Init sync anims (should perhaps have a flag for it so we don't do it too much....)
	if (m_bHasSyncAnim)
		UpdateSyncAnimScale(_pContext,m_pAG2I->GetEvaluator()->GetSyncAnimTimeScale());
	//AdjustAnimTimeOffset(_pContext);
}

//--------------------------------------------------------------------------------

bool CWAG2I_StateInstance::LeaveState_Setup(const CWAG2I_Context* _pContext, int16 _iLeaveMoveToken)
{
	MSCOPESHORT(CWAG2I_StateInstance::LeaveState_Setup);

	m_bGotLeaveState = true;
	//m_iLeaveAction = _iLeaveAction;
	m_iLeaveMoveToken = _iLeaveMoveToken;
	//const CXRAG2_Action* pAction = m_pAG2I->GetAction(m_iLeaveAction);
	const CXRAG2_MoveToken* pMoveToken = m_pAG2I->GetMoveToken(m_iLeaveMoveToken, m_iAnimGraph);
	if (!pMoveToken)
		return false;

	m_Leave_AnimBlendDuration = pMoveToken->GetAnimBlendDuration();
	m_Leave_AnimBlendDelay = pMoveToken->GetAnimBlendDelay();
	m_bGotTerminate = pMoveToken->GetTargetStateIndex() == AG2_STATEINDEX_TERMINATE;
	if (m_bGotTerminate)
		m_bDontBlendOut = false;

	m_LeaveTime = AG2I_UNDEFINEDTIME;
	m_BlendOutStartTime = AG2I_UNDEFINEDTIME;
	m_BlendOutEndTime = AG2I_UNDEFINEDTIME;
	m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;

	// FIXME: Add support for only blending this stateinstance out if no succeeding stateinstance exists.
	// This is only the case when the token is terminated and no succeeding state will be reached.
	// I think setting m_InvBlendOutDuration = 0.0f will do...
	// Mayby it can be signaled by an AnimBlendDuration = -1.0f?

	return true;
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::LeaveState_Initiate(const CWAG2I_Context* _pContext)
{
	MSCOPESHORT(CWAG2I_StateInstance::LeaveState_Initiate);
	m_bGotLeaveStateInit = true;

	m_LeaveTime = _pContext->m_GameTime;
	m_BlendOutStartTime = m_LeaveTime + CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay);

	if (m_Leave_AnimBlendDuration < 0.0f)
	{
		m_BlendOutEndTime = AG2I_LINKEDTIME;
		m_InvBlendOutDuration = m_Leave_AnimBlendDuration;
	}
	else
	{
		m_BlendOutEndTime = m_LeaveTime +  CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay +  m_Leave_AnimBlendDuration);
		m_InvBlendOutDuration = (m_Leave_AnimBlendDuration > 0) ? (1.0f / m_Leave_AnimBlendDuration) : DEFAULT_INVBLENDOUTDURATION;
	}
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::PredictionMiss_Add(const CWAG2I_Context* _pContext, const CWAG2I_SIP* _pSIP)
{
	MSCOPESHORT(CWAG2I_StateInstance::PredictionMiss_Add);

	m_Flags |= STATEINSTANCEFLAGS_PREDMISS_ADD;

	// FIXME: Implement smooth prediction miss fading...
	UnpackSIP(_pContext, _pSIP);
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::PredictionMiss_Remove(const CWAG2I_Context* _pContext)
{
	MSCOPESHORT(CWAG2I_StateInstance::PredictionMiss_Remove);

	m_Flags |= STATEINSTANCEFLAGS_PREDMISS_REMOVE;

	fp32 FadePredictionMiss_AnimBlendDuration = 0.0f;

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0) // See if stateinstance has been reached at all.
	{ // StateInstance not yet reached. Set BlendOutEndTime to current gametime (so it will be removed).
		m_EnterTime = _pContext->m_GameTime;
		m_BlendInStartTime = m_EnterTime;
		m_BlendInEndTime = m_EnterTime;
		m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;
		m_LeaveTime = _pContext->m_GameTime;
		m_BlendOutStartTime = m_LeaveTime;
		m_BlendOutEndTime = m_LeaveTime;
		m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;
		return;
	}
	else if (m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) == 0) // See if there already is a current blendout initiated.
	{ // No blendout initiated.
		m_bGotLeaveStateInit = true;
		m_LeaveTime = _pContext->m_GameTime;
		m_BlendOutStartTime = m_LeaveTime;
		m_BlendOutEndTime = m_LeaveTime + CMTime::CreateFromSeconds(FadePredictionMiss_AnimBlendDuration);
		m_InvBlendOutDuration = (FadePredictionMiss_AnimBlendDuration > 0) ? (1.0f / FadePredictionMiss_AnimBlendDuration) : DEFAULT_INVBLENDOUTDURATION;
		return;
	}
	else
	{ // Blendout already initiated.
		// See if current blendout is slower than a faked one would be.
		if ((_pContext->m_GameTime + CMTime::CreateFromSeconds(FadePredictionMiss_AnimBlendDuration)).Compare(m_BlendOutEndTime) < 0)
		{
			// Find how much headstart current blendout has (i.e. how far current blendout has gone).
			fp32 CurrentBlend = Clamp01((_pContext->m_GameTime - m_BlendOutStartTime).GetTime() * m_InvBlendOutDuration);

			// Find when new blendout duration would end, given the current headstart.
			m_LeaveTime = _pContext->m_GameTime - CMTime::CreateFromSeconds(FadePredictionMiss_AnimBlendDuration * (1.0f - CurrentBlend));

			m_BlendOutStartTime = m_LeaveTime;

			m_BlendOutEndTime = m_LeaveTime + CMTime::CreateFromSeconds(FadePredictionMiss_AnimBlendDuration);
			m_InvBlendOutDuration = (FadePredictionMiss_AnimBlendDuration > 0) ? (1.0f / FadePredictionMiss_AnimBlendDuration) : DEFAULT_INVBLENDOUTDURATION;
		}
	}
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::SyncWithSIP(const CWAG2I_Context* _pContext, const CWAG2I_SIP* _pSIP)
{
	MSCOPESHORT(CWAG2I_StateInstance::SyncWithSIP);

	m_Flags |= STATEINSTANCEFLAGS_PREDMISS_UPDATE;
/*
	if (m_EnqueueTime != _pSIP->GetEnqueueTime())
		ConOutL(CStrF("CWAG2I_StateInstance::SyncWithSIP() - EnqueueTime %3.3f differs from SIP %3.3f", m_EnqueueTime, _pSIP->GetEnqueueTime()));

//	m_iEnterAction = _pSIP->GetEnterActionIndex();

	if (m_EnterTime != _pSIP->GetEnterTime())
		ConOutL(CStrF("CWAG2I_StateInstance::SyncWithSIP() - EnterTime %3.3f differs from SIP %3.3f", m_EnterTime, _pSIP->GetEnterTime()));

//	m_iLeaveAction = _pSIP->GetLeaveActionIndex();

	if (m_LeaveTime != _pSIP->GetLeaveTime())
		ConOutL(CStrF("CWAG2I_StateInstance::SyncWithSIP() - LeaveTime %3.3f differs from SIP %3.3f", m_LeaveTime, _pSIP->GetLeaveTime()));

//	m_BlendOutEndTime = _pSIP->GetBlendOutEndTime();
*/
	// FIXME: Implement smooth prediction miss fading...
	UnpackSIP(_pContext, _pSIP);
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::PackSIP(CWAG2I_SIP* _pSIP) const
{
	MSCOPESHORT(CWAG2I_StateInstance::PackSIP);

	_pSIP->SetEnqueueTime(m_EnqueueTime);
	_pSIP->SetEnterTime(m_EnterTime);
	_pSIP->SetLeaveTime(m_LeaveTime);
	_pSIP->SetBlendOutEndTime(m_BlendOutEndTime);
	_pSIP->SetAnimTimeOffset(m_Enter_AnimTimeOffset);
}

//--------------------------------------------------------------------------------

// Brute Force overwrite/replace everything...
void CWAG2I_StateInstance::UnpackSIP(const CWAG2I_Context* _pContext, const CWAG2I_SIP* _pSIP)
{
	MSCOPESHORT(CWAG2I_StateInstance::UnpackSIP);

	m_EnqueueTime = _pSIP->GetEnqueueTime();
	//m_iEnterAction = _pSIP->GetEnterActionIndex();
	m_iEnterMoveToken = _pSIP->GetEnterMoveTokenIndex();
	m_EnterTime = _pSIP->GetEnterTime();
	//m_iLeaveAction = _pSIP->GetLeaveActionIndex();
	m_iLeaveMoveToken = _pSIP->GetLeaveMoveTokenIndex();
	m_LeaveTime = _pSIP->GetLeaveTime();
	m_BlendOutEndTime = _pSIP->GetBlendOutEndTime();
	m_Enter_AnimTimeOffset = _pSIP->GetAnimTimeOffset();
	m_iAnimGraph = _pSIP->GetAnimGraphIndex();
	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph,"Invalid animgraph");
	if (!pAnimGraph)
	{
		// Nothing below can run without the graph, and m_bHasAnimation must
		// not stay set from the previous state -- the render path would then
		// walk into GetAnimLayers() with an unresolvable state.
		m_bHasAnimation = false;
		return;
	}

	//const CXRAG2_Action* pEnterAction = m_pAG2I->GetAction(m_iEnterAction);
	const CXRAG2_MoveToken* pMoveToken = m_pAG2I->GetMoveToken(m_iEnterMoveToken, m_iAnimGraph);
	if (pMoveToken)
	{
		m_Enter_AnimBlendDuration = pMoveToken->GetAnimBlendDuration();
		m_Enter_AnimBlendDelay = pMoveToken->GetAnimBlendDelay();
		m_iState = pMoveToken->GetTargetStateIndex() & AG2_TARGETSTATE_INDEXMASK;
			
		const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
		if (pState != NULL)
		{
			m_iLoopControlAnimLayer = pState->GetLoopControlAnimLayerIndex();
			m_AnimLoopDuration = GetAnimLoopDuration(_pContext);

			m_bHasAnimation = false;
			m_bSkipForceKeep = pState->GetFlags(1) & CHAR_STATEFLAGHI_SKIPFORCEKEEP ? 1 : 0;
			/*if (m_iState == 1726)
				ConOut(CStrF("ForceKeep: %d",m_bSkipForceKeep));*/
			for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
			{
				int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

				const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
				if (pAnimLayer == NULL)
					continue;

				int16 iAnim = pAnimLayer->GetAnimIndex();
				if (iAnim != AG2_ANIMINDEX_NULL)
				{
					m_bHasAnimation = true;
					break;
				}
			}

			if (pState->GetFlags(1) & CHAR_STATEFLAGHI_SYNCANIM)
				EnterState_InitSyncAnims(_pContext);

			m_Priority = pState->GetPriority();
		}
		else
		{
			// m_iState was just overwritten from the move token and does not
			// resolve in this animgraph.  Leaving m_bHasAnimation set would
			// let the render thread dereference the NULL state.
			AG2_ReportBadState("UnpackSIP", pAnimGraph, m_iAnimGraph, m_iState);
			m_bHasAnimation = false;
		}

		if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) != 0)
		{
			m_BlendInStartTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay);

			if (m_Enter_AnimBlendDuration < 0.0f)
			{
				m_BlendInEndTime = AG2I_LINKEDTIME;
				m_InvBlendInDuration = m_Enter_AnimBlendDuration;
			}
			else
			{
				m_BlendInEndTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay +  m_Enter_AnimBlendDuration);
				m_InvBlendInDuration = (m_Enter_AnimBlendDuration > 0) ? (1.0f / m_Enter_AnimBlendDuration) : DEFAULT_INVBLENDINDURATION;
			}
			//AdjustAnimTimeOffset(_pContext);
		}
		else
		{
			m_BlendInStartTime = AG2I_UNDEFINEDTIME;
			m_BlendInEndTime = AG2I_UNDEFINEDTIME;
			m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;
		}
	}

	//const CXRAG2_Action* pLeaveAction = m_pAG2I->GetAction(m_iLeaveAction);
	const CXRAG2_MoveToken* pLeaveMoveToken = pAnimGraph->GetMoveToken(m_iLeaveMoveToken);
	if (pLeaveMoveToken)
	{
		m_Leave_AnimBlendDuration = pLeaveMoveToken->GetAnimBlendDuration();
		m_Leave_AnimBlendDelay = pLeaveMoveToken->GetAnimBlendDelay();

		if (m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) != 0)
		{
			m_BlendOutStartTime = m_LeaveTime + CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay);

			if (m_Leave_AnimBlendDuration < 0.0f)
			{
				m_BlendOutEndTime = AG2I_LINKEDTIME;
				m_InvBlendOutDuration = m_Leave_AnimBlendDuration;
			}
			else
			{
				m_BlendOutEndTime = m_LeaveTime + CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay +  m_Leave_AnimBlendDuration);
				m_InvBlendOutDuration = (m_Leave_AnimBlendDuration > 0) ? (1.0f / m_Leave_AnimBlendDuration) : DEFAULT_INVBLENDOUTDURATION;
			}
		}
		else
		{
			m_BlendOutStartTime = AG2I_UNDEFINEDTIME;
			m_BlendOutEndTime = AG2I_UNDEFINEDTIME;
			m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;
		}
	}
}

bool CWAG2I_StateInstance::OnCreateClientUpdate(uint8*& _pData, const CWAG2I_StateInstance* _pStateInstMirror) const
{

	// Enqueue time
	// Enter time (!?)
	// Leave time
	// Enter movetoken
	// Leave movetoken
	// Animgraph id

	uint8& CopyFlags = _pData[0];
	_pData++;
	CopyFlags = 0;//m_DirtyFlag;

	// Already lies in siid
	if (!_pStateInstMirror->m_EnterTime.AlmostEqual(m_EnterTime))
	{
		CopyFlags |= AG2I_STATEINST_DIRTYFLAG_ENTERTIME;
		PTR_PUTCMTIME(_pData, m_EnterTime);
	}

	if (!_pStateInstMirror->m_LeaveTime.AlmostEqual(m_LeaveTime))
	{
		CopyFlags |= AG2I_STATEINST_DIRTYFLAG_LEAVETIME;
		PTR_PUTCMTIME(_pData, m_LeaveTime);
	}

	if (_pStateInstMirror->m_iLeaveMoveToken != m_iLeaveMoveToken)
	{
		CopyFlags |= AG2I_STATEINST_DIRTYFLAG_LEAVEMOVETOKEN;
		PTR_PUTINT16(_pData, m_iLeaveMoveToken);
	}

	if (m_bHasSyncAnim && _pStateInstMirror->m_SyncAnimScale != m_SyncAnimScale)
	{
		CopyFlags |= AG2I_STATEINST_DIRTYFLAG_SYNCANIMSCALE;
		PTR_PUTFP32(_pData, m_SyncAnimScale);
	}

	return (CopyFlags != 0);
}

void CWAG2I_StateInstance::OnClientUpdate(CWAG2I_Context* _pContext, CWAG2I_SIID _SIID, const uint8*& _pData)
{
	PTR_GETINT8(_pData, m_DirtyFlag);

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ANIMGRAPHID)
	{
		//PTR_GETINT8(_pData, m_iAnimGraph);
		m_iAnimGraph = _SIID.m_iAnimGraph;
	}

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENQUEUETIME)
	{
		//PTR_GETCMTIME(_pData, m_EnqueueTime);
		m_EnqueueTime = _SIID.m_EnqueueTime;
	}

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENTERTIME)
		PTR_GETCMTIME(_pData, m_EnterTime);

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_LEAVETIME)
		PTR_GETCMTIME(_pData, m_LeaveTime);

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENTERMOVETOKEN)
	{
		//PTR_GETINT16(_pData, m_iEnterMoveToken);
		m_iEnterMoveToken = _SIID.m_iEnterMoveToken;
		m_pAG2I->AcquireAllResources(_pContext);
		const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
		M_ASSERT(pAnimGraph,"Invalid animgraph");
		// If entermovetoken is present, setup the rest
		const CXRAG2_MoveToken* pMoveToken = m_pAG2I->GetMoveToken(m_iEnterMoveToken, m_iAnimGraph);
		if (pMoveToken && pAnimGraph)
		{
			m_Enter_AnimBlendDuration = pMoveToken->GetAnimBlendDuration();
			m_Enter_AnimBlendDelay = pMoveToken->GetAnimBlendDelay();
			m_iState = pMoveToken->GetTargetStateIndex() & AG2_TARGETSTATE_INDEXMASK;

			const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
			if (pState != NULL)
			{
				m_iLoopControlAnimLayer = pState->GetLoopControlAnimLayerIndex();
				m_AnimLoopDuration = GetAnimLoopDuration(_pContext);

				m_bHasAnimation = false;
				m_bSkipForceKeep = pState->GetFlags(1) & CHAR_STATEFLAGHI_SKIPFORCEKEEP ? 1 : 0;
				/*if (m_iState == 1726)
				ConOut(CStrF("ForceKeep: %d",m_bSkipForceKeep));*/
				for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
				{
					int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

					const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
					if (pAnimLayer == NULL)
						continue;

					int16 iAnim = pAnimLayer->GetAnimIndex();
					if (iAnim != AG2_ANIMINDEX_NULL)
					{
						m_bHasAnimation = true;
						break;
					}
				}

				if (pState->GetFlags(1) & CHAR_STATEFLAGHI_SYNCANIM)
					EnterState_InitSyncAnims(_pContext);

				m_Priority = pState->GetPriority();
			}
			else
			{
				// Same as in UnpackSIP: an unresolvable state must clear the
				// "has animation" flag, otherwise the client render path
				// dereferences NULL.
				AG2_ReportBadState("OnClientUpdate", pAnimGraph, m_iAnimGraph, m_iState);
				m_bHasAnimation = false;
			}

			if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) != 0)
			{
				m_BlendInStartTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay);

				if (m_Enter_AnimBlendDuration < 0.0f)
				{
					m_BlendInEndTime = AG2I_LINKEDTIME;
					m_InvBlendInDuration = m_Enter_AnimBlendDuration;
				}
				else
				{
					m_BlendInEndTime = m_EnterTime + CMTime::CreateFromSeconds(m_Enter_AnimBlendDelay +  m_Enter_AnimBlendDuration);
					m_InvBlendInDuration = (m_Enter_AnimBlendDuration > 0) ? (1.0f / m_Enter_AnimBlendDuration) : DEFAULT_INVBLENDINDURATION;
				}
				//AdjustAnimTimeOffset(_pContext);
			}
			else
			{
				m_BlendInStartTime = AG2I_UNDEFINEDTIME;
				m_BlendInEndTime = AG2I_UNDEFINEDTIME;
				m_InvBlendInDuration = DEFAULT_INVBLENDINDURATION;
			}
		}
	}

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_LEAVEMOVETOKEN)
	{
		PTR_GETINT16(_pData, m_iLeaveMoveToken);
		m_pAG2I->AcquireAllResources(_pContext);
		const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
		M_ASSERT(pAnimGraph,"Invalid animgraph");
		// If leavemovetoken is present, setup the rest
		const CXRAG2_MoveToken* pLeaveMoveToken = pAnimGraph ? pAnimGraph->GetMoveToken(m_iLeaveMoveToken) : NULL;
		if (pLeaveMoveToken)
		{
			m_Leave_AnimBlendDuration = pLeaveMoveToken->GetAnimBlendDuration();
			m_Leave_AnimBlendDelay = pLeaveMoveToken->GetAnimBlendDelay();

			if (m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) != 0)
			{
				m_BlendOutStartTime = m_LeaveTime + CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay);

				if (m_Leave_AnimBlendDuration < 0.0f)
				{
					m_BlendOutEndTime = AG2I_LINKEDTIME;
					m_InvBlendOutDuration = m_Leave_AnimBlendDuration;
				}
				else
				{
					m_BlendOutEndTime = m_LeaveTime + CMTime::CreateFromSeconds(m_Leave_AnimBlendDelay +  m_Leave_AnimBlendDuration);
					m_InvBlendOutDuration = (m_Leave_AnimBlendDuration > 0) ? (1.0f / m_Leave_AnimBlendDuration) : DEFAULT_INVBLENDOUTDURATION;
				}
			}
			else
			{
				m_BlendOutStartTime = AG2I_UNDEFINEDTIME;
				m_BlendOutEndTime = AG2I_UNDEFINEDTIME;
				m_InvBlendOutDuration = DEFAULT_INVBLENDOUTDURATION;
			}
		}
	}

	if (m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_SYNCANIMSCALE)
	{
		PTR_GETFP32(_pData,m_SyncAnimScale);
	}
}

void CWAG2I_StateInstance::Write(CCFile* _pFile) const
{
	m_EnqueueTime.Write(_pFile);
	m_EnterTime.Write(_pFile);
	m_LeaveTime.Write(_pFile);
	m_BlendInStartTime.Write(_pFile);
	m_BlendInEndTime.Write(_pFile);
	m_BlendOutStartTime.Write(_pFile);
	m_BlendOutEndTime.Write(_pFile);

	// Flags
	uint16 Flags = 0;
	Flags |= m_bHasAnimation << 0;
	Flags |= m_ProceedCondition_bDefault << 1;
	Flags |= m_bSkipForceKeep << 2;
	Flags |= m_bGotLeaveState << 3;
	Flags |= m_bGotLeaveStateInit << 4;
	Flags |= m_bGotEnterStateInit << 5;
	Flags |= m_bGotTerminate << 6;
	Flags |= m_bHasSyncAnim << 7;
	Flags |= m_bHasPerfectMovement << 8;
	_pFile->WriteLE(Flags);

	_pFile->WriteLE(m_AnimTime);

	_pFile->WriteLE(m_AnimLoopDuration);
	_pFile->WriteLE(m_TimeScale);

	_pFile->WriteLE(m_Enter_AnimBlendDuration);
	_pFile->WriteLE(m_Enter_AnimBlendDelay);
	_pFile->WriteLE(m_Enter_AnimTimeOffset);

	_pFile->WriteLE(m_InvBlendInDuration);


	_pFile->WriteLE(m_Leave_AnimBlendDuration);
	_pFile->WriteLE(m_Leave_AnimBlendDelay);

	_pFile->WriteLE(m_InvBlendOutDuration);

	_pFile->WriteLE(m_ProceedCondition_Constant);

	if (m_bHasSyncAnim)
	{
		int8 NumSyncPoints = m_SyncPoints1.m_lPoints.Len();
		_pFile->WriteLE(NumSyncPoints);
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->WriteLE(m_SyncPoints1.m_lPoints[i].m_Type);
			_pFile->WriteLE(m_SyncPoints1.m_lPoints[i].m_Time);
			_pFile->WriteLE(m_SyncPoints2.m_lPoints[i].m_Type);
			_pFile->WriteLE(m_SyncPoints2.m_lPoints[i].m_Time);
		}
		NumSyncPoints++;
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->WriteLE(m_lTimeScales1[i]);
			_pFile->WriteLE(m_lTimeScales2[i]);
		}
		NumSyncPoints++;
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->WriteLE(m_lTimes[i]);
		}
		_pFile->WriteLE(m_SyncAnimScale);
		_pFile->WriteLE(m_AnchorTime);
		_pFile->WriteLE(m_Duration1);
		_pFile->WriteLE(m_Duration2);
	}
	TAP_RCD<const int16> lEventKeys = m_liLastEventKey;
	int16 NumKeys = lEventKeys.Len();
	_pFile->WriteLE(NumKeys);
	for (int32 i = 0; i < NumKeys; i++)
	{
		_pFile->WriteLE(lEventKeys[i]);
	}
	lEventKeys = m_lNumLoops;
	NumKeys = lEventKeys.Len();
	_pFile->WriteLE(NumKeys);
	for (int32 i = 0; i < NumKeys; i++)
	{
		_pFile->WriteLE(lEventKeys[i]);
	}

	_pFile->WriteLE(m_iState);
	_pFile->WriteLE(m_iEnterMoveToken);
	_pFile->WriteLE(m_iLeaveMoveToken);

	_pFile->WriteLE(m_Flags);
	_pFile->WriteLE(m_Priority);
	_pFile->WriteLE(m_iLoopControlAnimLayer);
	_pFile->WriteLE(m_ProceedCondition_iProperty);
	_pFile->WriteLE(m_ProceedCondition_PropertyType);
	_pFile->WriteLE(m_ProceedCondition_iOperator);
	_pFile->WriteLE(m_iAnimGraph);

	_pFile->WriteLE(m_LeaveStateFrom);
}

void CWAG2I_StateInstance::Read(CCFile* _pFile)
{
	m_EnqueueTime.Read(_pFile);
	m_EnterTime.Read(_pFile);
	m_LeaveTime.Read(_pFile);
	m_BlendInStartTime.Read(_pFile);
	m_BlendInEndTime.Read(_pFile);
	m_BlendOutStartTime.Read(_pFile);
	m_BlendOutEndTime.Read(_pFile);

	// Flags
	uint16 Flags;
	_pFile->ReadLE(Flags);
	m_bHasAnimation = (Flags & 1 << 0) != 0;
	m_ProceedCondition_bDefault = (Flags & 1 << 1) != 0;
	m_bSkipForceKeep = (Flags & 1 << 2) != 0;
	m_bGotLeaveState = (Flags & 1 << 3) != 0;
	m_bGotLeaveStateInit = (Flags & 1 << 4) != 0;
	m_bGotEnterStateInit = (Flags & 1 << 5) != 0;
	m_bGotTerminate = (Flags & 1 << 6) != 0;
	m_bHasSyncAnim = (Flags & 1 << 7) != 0;
	m_bHasPerfectMovement = (Flags & 1 << 8) != 0;

	_pFile->ReadLE(m_AnimTime);

	_pFile->ReadLE(m_AnimLoopDuration);
	_pFile->ReadLE(m_TimeScale);

	_pFile->ReadLE(m_Enter_AnimBlendDuration);
	_pFile->ReadLE(m_Enter_AnimBlendDelay);
	_pFile->ReadLE(m_Enter_AnimTimeOffset);

	_pFile->ReadLE(m_InvBlendInDuration);

	_pFile->ReadLE(m_Leave_AnimBlendDuration);
	_pFile->ReadLE(m_Leave_AnimBlendDelay);

	_pFile->ReadLE(m_InvBlendOutDuration);

	_pFile->ReadLE(m_ProceedCondition_Constant);

	if (m_bHasSyncAnim)
	{
		int8 NumSyncPoints;
		_pFile->ReadLE(NumSyncPoints);
		m_SyncPoints1.m_lPoints.SetLen(NumSyncPoints);
		m_SyncPoints2.m_lPoints.SetLen(NumSyncPoints);
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->ReadLE(m_SyncPoints1.m_lPoints[i].m_Type);
			_pFile->ReadLE(m_SyncPoints1.m_lPoints[i].m_Time);
			_pFile->ReadLE(m_SyncPoints2.m_lPoints[i].m_Type);
			_pFile->ReadLE(m_SyncPoints2.m_lPoints[i].m_Time);
		}
		NumSyncPoints++;
		m_lTimeScales1.SetLen(NumSyncPoints);
		m_lTimeScales2.SetLen(NumSyncPoints);
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->ReadLE(m_lTimeScales1[i]);
			_pFile->ReadLE(m_lTimeScales2[i]);
		}
		NumSyncPoints++;
		m_lTimes.SetLen(NumSyncPoints);
		for (int32 i = 0; i < NumSyncPoints; i++)
		{
			_pFile->ReadLE(m_lTimes[i]);
		}
		_pFile->WriteLE(m_SyncAnimScale);
		_pFile->WriteLE(m_AnchorTime);
		_pFile->WriteLE(m_Duration1);
		_pFile->WriteLE(m_Duration2);
	}
	
	int16 NumKeys;
	_pFile->ReadLE(NumKeys);
	m_liLastEventKey.SetLen(NumKeys);
	TAP_RCD<int16> lEventKeys = m_liLastEventKey;
	for (int32 i = 0; i < NumKeys; i++)
	{
		_pFile->ReadLE(lEventKeys[i]);
	}
	_pFile->ReadLE(NumKeys);
	m_lNumLoops.SetLen(NumKeys);
	lEventKeys = m_lNumLoops;;
	for (int32 i = 0; i < NumKeys; i++)
	{
		_pFile->ReadLE(lEventKeys[i]);
	}

	_pFile->ReadLE(m_iState);
	_pFile->ReadLE(m_iEnterMoveToken);
	_pFile->ReadLE(m_iLeaveMoveToken);

	_pFile->ReadLE(m_Flags);
	_pFile->ReadLE(m_Priority);
	_pFile->ReadLE(m_iLoopControlAnimLayer);
	_pFile->ReadLE(m_ProceedCondition_iProperty);
	_pFile->ReadLE(m_ProceedCondition_PropertyType);
	_pFile->ReadLE(m_ProceedCondition_iOperator);
	_pFile->ReadLE(m_iAnimGraph);

	_pFile->ReadLE(m_LeaveStateFrom);
}

void CWAG2I_StateInstance::UpdateFromMirror(CWAG2I_Context* _pContext, CWAG2I_StateInstance* _pMirror)
{
	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ANIMGRAPHID)
		m_iAnimGraph = _pMirror->m_iAnimGraph;

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENQUEUETIME)
		m_EnqueueTime = _pMirror->m_EnqueueTime;

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENTERTIME)
		m_EnterTime = _pMirror->m_EnterTime;

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_LEAVETIME)
		m_LeaveTime = _pMirror->m_LeaveTime;

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_ENTERMOVETOKEN)
	{
		m_iEnterMoveToken = _pMirror->m_iEnterMoveToken;
		m_Enter_AnimBlendDuration = _pMirror->m_Enter_AnimBlendDuration;
		m_Enter_AnimBlendDelay = _pMirror->m_Enter_AnimBlendDelay;
		m_iState = _pMirror->m_iState;

		m_iLoopControlAnimLayer = _pMirror->m_iLoopControlAnimLayer;
		m_AnimLoopDuration = _pMirror->m_AnimLoopDuration;

		m_bHasAnimation = _pMirror->m_bHasAnimation;
		m_bSkipForceKeep = _pMirror->m_bSkipForceKeep;

		m_Priority = _pMirror->m_Priority;

		m_BlendInStartTime = _pMirror->m_BlendInStartTime;
		m_BlendInEndTime = _pMirror->m_BlendInEndTime;
		m_InvBlendInDuration = _pMirror->m_InvBlendInDuration;
	}

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_LEAVEMOVETOKEN)
	{
		m_iLeaveMoveToken = _pMirror->m_iLeaveMoveToken;
		m_Leave_AnimBlendDuration = _pMirror->m_Leave_AnimBlendDuration;
		m_Leave_AnimBlendDelay = _pMirror->m_Leave_AnimBlendDelay;

		m_BlendOutStartTime = _pMirror->m_BlendOutStartTime;
		m_BlendOutEndTime = _pMirror->m_BlendOutEndTime;
		m_InvBlendOutDuration = _pMirror->m_InvBlendOutDuration;
	}

	if (_pMirror->m_DirtyFlag & AG2I_STATEINST_DIRTYFLAG_SYNCANIMSCALE)
	{
		if (m_bHasSyncAnim)
			UpdateSyncAnimScale(_pContext,_pMirror->m_SyncAnimScale);
	}

	// Reset dirtyflag
	_pMirror->m_DirtyFlag = 0;
}

//--------------------------------------------------------------------------------

void CWAG2I_StateInstance::GetAnimLayers(const CWAG2I_Context* _pContext, bool _bBlendOut, CXR_AnimLayer* _pLayers, int& _nLayers, bool _ForceKeepLayers) const
{
	MSCOPE(CWAG2I_StateInstance::GetAnimLayers, CWAG2I);

	if (_nLayers < 1)
		return;

	int nMaxLayers = _nLayers;
	_nLayers = 0;

	if (!m_bHasAnimation)
		return;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph, "Invalid Animgraph");
	if (!pAnimGraph)
		return;

	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return;

	fp32 Blend, BlendIn, BlendOut;

	{
	if (!_bBlendOut || m_bDontBlendOut || (m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) == 0))
	{
			if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
			else if (m_InvBlendInDuration > 0)
				BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
			else
				BlendIn = 1.0f;

			BlendOut = 1.0f;

		//ConOutL(CStrF("BlendIn = %3.3f", Blend));
	}
	else
	{
		if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
		else if (m_InvBlendInDuration > 0)
			BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
		else
			BlendIn = 1.0f;

		if (m_InvBlendOutDuration < 0.0f)
			BlendOut = m_InvBlendOutDuration;
		else
			BlendOut = 1.0f - Clamp01((_pContext->m_GameTime - m_BlendOutStartTime).GetTime() * m_InvBlendOutDuration);

		//ConOutL(CStrF("BlendIn = %3.3f, BlendOut = %3.3f, Blend = %3.3f", BlendIn, BlendOut, Blend));
	}
	}

	Blend = BlendIn * BlendOut; // Smoother than Min(BlendIn, BlendOut)

	if (Blend == 0 && !_ForceKeepLayers)
		return;

	M_ASSERT(m_pAG2I->m_pEvaluator, "!");
	m_pAG2I->m_pEvaluator->AG2_RefreshStateInstanceProperties(_pContext, this);

	int nLayers = 0;
	const CAG2AnimLayerIndex BaseLayer = pState->GetBaseAnimLayerIndex();
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		if (nLayers >= nMaxLayers)
			break;

		int16 iAnimLayer = BaseLayer + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::GetAnimLayers : Invalid animlayer");
		if (!pAnimLayer)
			continue;
		// Check if it's a "grip" layer
		CAG2AnimFlags Flags = pAnimLayer->GetAnimFlags();
		if (Flags & CXR_ANIMLAYER_VALUECOMPARE)
		{
			// Ok, compare int value in "m_iMergeOperator" with value in "m_iTimeControlProperty"
			// if it's not the same continue
			if ((uint8)m_pAG2I->m_pEvaluator->GetPropertyInt(pAnimLayer->GetMergeOperator()) != pAnimLayer->GetTimeControlProperty())
				continue;
		}

		fp32 LayerBlend = Blend * pAnimLayer->GetOpacity();

		if (LayerBlend == 0.0f && !_ForceKeepLayers)
			continue;

		int16 iAnim = pAnimLayer->GetAnimIndex();
//		ConOutL(CStrF("iAnim %d", iAnim));
		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (!pAnimLayerSeq)
			continue;

		CMTime ContinousTime;
		CMTime LoopedTime;
		CMTime Zero = CMTime::CreateFromSeconds(0.0f);
		fp32 TimeScale;
		if (m_bHasSyncAnim && !(Flags & CXR_ANIMLAYER_VALUECOMPARE))
		{
			fp32 Time;
			GetSyncAnimTime(_pContext,jAnimLayer,Time,TimeScale);
			ContinousTime = LoopedTime = CMTime::CreateFromSeconds(Time);
			LayerBlend *= (jAnimLayer == 0  ? 1.0f /*- SyncAnimScale*/ : m_SyncAnimScale);
		}
		else if (m_bHasAdaptiveTimeScale && !(Flags & CXR_ANIMLAYER_VALUECOMPARE))
		{
			fp32 Time;
			GetAdaptiveTime(_pContext,jAnimLayer,Time,TimeScale);
			ContinousTime = LoopedTime = CMTime::CreateFromSeconds(Time);
		}
		else 
		{
//			int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
			TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
			if (!(Flags & CXR_ANIMLAYER_VALUECOMPARE) && pAnimLayer->GetTimeControlProperty())
				ContinousTime = CMTime::CreateFromSeconds(pAnimLayerSeq->GetDuration() * m_pAG2I->m_pEvaluator->GetPropertyFloat(pAnimLayer->GetTimeControlProperty()));
			else
				ContinousTime = _pContext->m_GameTime - m_EnterTime;//m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();

			ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) + ContinousTime.Scale(TimeScale);
			LoopedTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);
		}
		if (LoopedTime.Compare(Zero) < 0.0f)
			LoopedTime = Zero;

		// Страховка от NaN во времени слоя. Сравнение выше NaN не ловит
		// (любое сравнение с NaN ложно), а клип, посчитанный в NaN, отдаёт
		// первый кадр -- поза замирает молча. Источник NaN найден и закрыт
		// (Riddick_SyncSliceOK), но цена проверки нулевая, а цена молчания
		// -- несколько прогонов вслепую.
		{
			const fp32 LT = LoopedTime.GetTime();
			if (!(LT > -1.0e9f && LT < 1.0e9f))
			{
				static int s_n = 0;
				if (s_n < 8)
				{
					++s_n;
					fprintf(stderr, "[ANIMNAN] layer time not finite: iState=%d iAnim=%d scale=%.3f -> clamped to 0\n",
						(int)m_iState, (int)iAnim, TimeScale);
					fflush(stderr);
				}
				LoopedTime = Zero;
			}
		}

#ifdef	AG2_DEBUG
		bool bLocalDebug = false;
		if (bLocalDebug)
			ConOutL(CStrF("AnimLayer: pObj %X, StateInstance %d, GameTime %3.3f, iAnim %d, AnimAnchorTime %3.3f, LoopedTime %3.3f, Blend %3.2f, BI %3.3f - %3.3f, BO %3.3f - %3.3f",
						_pContext->m_pObj, _iSI, _pContext->m_GameTime, pAnimLayer->GetAnimIndex(), m_EnterTime.GetTime(), LoopedTime.GetTime(), LayerBlend,
						m_BlendInStartTime, m_BlendInEndTime.GetTime(), m_BlendOutStartTime.GetTime(), m_BlendOutEndTime.GetTime()));
#endif
		uint32 StateFlags = pState->GetFlags(0);

		_pLayers[nLayers].Create3(pAnimLayerSeq, LoopedTime, TimeScale, Sinc(LayerBlend), pAnimLayer->GetBaseJointIndex(), Flags);

		if (StateFlags & CHAR_STATEFLAG_LAYERADJUSTFOROFFSET)
			_pLayers[nLayers].m_TimeOffset = m_Enter_AnimTimeOffset;

#ifdef AG2_DEBUG
		_pLayers[nLayers].m_DebugMessage = CStrF("iState %d, iAnim %d, iBaseJoint %d, iStateInstance %d, GameTime %3.3f, AnimAnchorTime %3.3f, LoopedTime %3.3f, Blend %3.2f, BI %3.3f - %3.3f, BO %3.3f - %3.3f",
			m_iState, pAnimLayer->GetAnimIndex(), pAnimLayer->GetBaseJointIndex(), _iSI, _pContext->m_GameTime, m_EnterTime, LoopedTime, LayerBlend,
			m_BlendInStartTime, m_BlendInEndTime, m_BlendOutStartTime, m_BlendOutEndTime);
#endif
		// RIDDICK_DBG_ANIMTIME=1 -- продвигается ли ВРЕМЯ СЛОЯ.
		//
		// Зачем именно здесь. Штатная трасса анимграфа
		// (RIDDICK_AG2_DEBUGFLAGS) уже доказала: граф живой, состояния
		// сменяются (EXPLORE_IDLE -> WALKFWD -> RUNFWD -> TURN...), у
		// каждого состояния резолвится анимация (iAnim 74 и т.п.), игровое
		// время идёт. То есть до этой точки всё цело.
		//
		// А наблюдается «персонаж скользит в позе первого кадра». Это ровно
		// то, что даёт застрявший LoopedTime: поза берётся в LoopedTime
		// (Create3 кладёт его в m_Time), а корневое движение -- как РАЗНОСТЬ
		// EvalTrack0(m_Time) и EvalTrack0(m_Time + TimeSpan) (WAG2I.cpp,
		// GetAnimVelocity). Если LoopedTime не растёт, разность каждый тик
		// одна и та же и НЕнулевая: персонаж едет с постоянной скоростью, а
		// поза стоит. Это же объясняет замер из
		// Docs/Research_MoveSpeed_Report.md, где скорость от анимации вышла
		// в 3-6 раз больше расчётной -- окно всегда берётся с крутого начала
		// клипа.
		//
		// Печатается и на сервере, и на клиенте: рисуется клиентский
		// инстанс, а трасса выше собиралась серверная, так что расхождение
		// между ними -- отдельная проверяемая версия.
		{
			static int s_On = -1;
			if (s_On < 0)
			{
				const char* e = getenv("RIDDICK_DBG_ANIMTIME");
				s_On = (e && *e && *e != '0') ? 1 : 0;
			}
			if (s_On && jAnimLayer == 0 && _pContext->m_pObj)
			{
				// Три первых объекта, каждый 15-й вызов, потолок 240 строк
				static int s_lObj[3] = { -1, -1, -1 };
				static int s_lCount[3] = { 0, 0, 0 };
				static int s_nLines = 0;
				const int iObject = _pContext->m_pObj->m_iObject;
				int iSlot = -1;
				for (int i = 0; i < 3; i++)
				{
					if (s_lObj[i] == iObject) { iSlot = i; break; }
					if (s_lObj[i] == -1) { s_lObj[i] = iObject; iSlot = i; break; }
				}
				if (iSlot >= 0 && (s_lCount[iSlot]++ % 15) == 0 && s_nLines < 240)
				{
					++s_nLines;
					const bool bServer = _pContext->m_pWPhysState ? _pContext->m_pWPhysState->IsServer() : false;
					fprintf(stderr,
						"[ANIMT] %s obj=%d iAnim=%d gt=%.3f enter=%.3f contin=%.3f looped=%.3f scale=%.3f dur=%.3f blend=%.2f\n",
						bServer ? "srv" : "cli", iObject, (int)iAnim,
						_pContext->m_GameTime.GetTime(), m_EnterTime.GetTime(),
						ContinousTime.GetTime(), LoopedTime.GetTime(),
						TimeScale, pAnimLayerSeq->GetDuration(), LayerBlend);
					fflush(stderr);
				}
			}
		}

		M_ASSERT(0x7fc00000 != (uint32&)_pLayers[nLayers].m_Time, "!");
		_pLayers[nLayers++].m_ContinousTime = ContinousTime;

//		_pLayers[nLayers++].Create3(pAnimLayerSeq, AnimTime, pAnimLayer->GetTimeScale(), pAnimLayer->GetMergeOperator(), LayerBlend, pAnimLayer->GetBaseJointIndex(), Flags);
	}

	_nLayers = nLayers;
}

void CWAG2I_StateInstance::GetValueCompareLayers(const CWAG2I_Context* _pContext, CXR_AnimLayer* _pLayers, int& _nLayers, int32 _Value) const
{
	MSCOPE(CWAG2I_StateInstance::GetValueCompareLayers, CWAG2I);

	if (_nLayers < 1)
		return;

	int nMaxLayers = _nLayers;
	_nLayers = 0;

	if (!m_bHasAnimation)
		return;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph, "Invalid Animgraph");
	if (!pAnimGraph)
		return;

	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return;

	fp32 Blend, BlendIn, BlendOut;

	{
		if (m_bDontBlendOut || m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		{
			if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
			else if (m_InvBlendInDuration > 0)
				BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
			else
				BlendIn = 1.0f;

			BlendOut = 1.0f;

			//ConOutL(CStrF("BlendIn = %3.3f", Blend));
		}
		else
		{
			if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
			else if (m_InvBlendInDuration > 0)
				BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
			else
				BlendIn = 1.0f;

			if (m_InvBlendOutDuration < 0.0f)
				BlendOut = m_InvBlendOutDuration;
			else
				BlendOut = 1.0f - Clamp01((_pContext->m_GameTime - m_BlendOutStartTime).GetTime() * m_InvBlendOutDuration);

			//ConOutL(CStrF("BlendIn = %3.3f, BlendOut = %3.3f, Blend = %3.3f", BlendIn, BlendOut, Blend));
		}
	}

	if ((BlendIn < 0.0f) && (BlendOut < 0.0f))
		Blend = -3.0f;
	else
		Blend = BlendIn * BlendOut; // Smoother than Min(BlendIn, BlendOut)

	if (Blend == 0.0f)
		return;

	int nLayers = 0;
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		if (nLayers >= nMaxLayers)
			break;

		int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::GetAnimLayerSeqs : Invalid animlayer");
		if (!pAnimLayer)
			continue;
		uint32 Flags = pAnimLayer->GetAnimFlags();
		// Skip value compare layers for now
		if (Flags & CXR_ANIMLAYER_VALUECOMPARE)
		{
			// Ok, compare int value in "m_iMergeOperator" with value in "m_iTimeControlProperty"
			// if it's not the same continue
			if (((uint8)_Value) != pAnimLayer->GetTimeControlProperty())
				continue;
		}
		else
		{
			continue;
		}

		int16 iAnim = pAnimLayer->GetAnimIndex();
		//		ConOutL(CStrF("iAnim %d", iAnim));
		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (!pAnimLayerSeq)
			continue;

		M_ASSERT(m_pAG2I->m_pEvaluator, "!");
		m_pAG2I->m_pEvaluator->AG2_RefreshStateInstanceProperties(_pContext, this);

		CMTime ContinousTime;
		CMTime LoopedTime;
		CMTime Zero = CMTime::CreateFromSeconds(0.0f);
		fp32 TimeScale;
		if (m_bHasSyncAnim)
		{
			fp32 Time;
			// Only get anim events from the dominating layer
			if ((jAnimLayer == 0 && m_SyncAnimScale > 0.5f) || 
				(jAnimLayer > 0 && m_SyncAnimScale <= 0.5f))
				continue;
			GetSyncAnimTime(_pContext,jAnimLayer,Time,TimeScale);
			ContinousTime = LoopedTime = CMTime::CreateFromSeconds(Time);
		}
		else
		{
			//			int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
			TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
			if (!(Flags & CXR_ANIMLAYER_VALUECOMPARE) && pAnimLayer->GetTimeControlProperty())
				ContinousTime = CMTime::CreateFromSeconds(pAnimLayerSeq->GetDuration() * m_pAG2I->m_pEvaluator->GetPropertyFloat(pAnimLayer->GetTimeControlProperty()));
			else
				ContinousTime = _pContext->m_GameTime - m_EnterTime;//m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();
			ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) + ContinousTime.Scale(TimeScale);
			LoopedTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);
		}

		if (LoopedTime.Compare(Zero) < 0.0f)
			LoopedTime = Zero;

		// Страховка от NaN во времени слоя. Сравнение выше NaN не ловит
		// (любое сравнение с NaN ложно), а клип, посчитанный в NaN, отдаёт
		// первый кадр -- поза замирает молча. Источник NaN найден и закрыт
		// (Riddick_SyncSliceOK), но цена проверки нулевая, а цена молчания
		// -- несколько прогонов вслепую.
		{
			const fp32 LT = LoopedTime.GetTime();
			if (!(LT > -1.0e9f && LT < 1.0e9f))
			{
				static int s_n = 0;
				if (s_n < 8)
				{
					++s_n;
					fprintf(stderr, "[ANIMNAN] layer time not finite: iState=%d iAnim=%d scale=%.3f -> clamped to 0\n",
						(int)m_iState, (int)iAnim, TimeScale);
					fflush(stderr);
				}
				LoopedTime = Zero;
			}
		}

		uint32 StateFlags = pState->GetFlags(0);

		_pLayers[nLayers].Create3(pAnimLayerSeq, LoopedTime, TimeScale, Sinc(Blend), pAnimLayer->GetBaseJointIndex(), Flags);
		if (StateFlags & CHAR_STATEFLAG_LAYERADJUSTFOROFFSET)
			_pLayers[nLayers].m_TimeOffset = m_Enter_AnimTimeOffset;

		M_ASSERT(0x7fc00000 != (uint32&)_pLayers[nLayers].m_Time, "!");
		_pLayers[nLayers++].m_ContinousTime = ContinousTime;
	}

	_nLayers = nLayers;
}

void CWAG2I_StateInstance::GetAnimLayerSeqs(const CWAG2I_Context* _pContext, CXR_AnimLayer* _pLayers, int& _nLayers) const
{
	MSCOPE(CWAG2I_StateInstance::GetAnimLayerSeqs, CWAG2I);

	if (_nLayers < 1)
		return;

	int nMaxLayers = _nLayers;
	_nLayers = 0;

	if (!m_bHasAnimation)
		return;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph, "Invalid Animgraph");
	if (!pAnimGraph)
		return;

	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return;

	int nLayers = 0;
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		if (nLayers >= nMaxLayers)
			break;

		int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::GetAnimLayerSeqs : Invalid animlayer");
		if (!pAnimLayer)
			continue;
		uint32 Flags = pAnimLayer->GetAnimFlags();
		// Skip value compare layers for now
		if (Flags & CXR_ANIMLAYER_VALUECOMPARE)
		{
			// Ok, compare int value in "m_iMergeOperator" with value in "m_iTimeControlProperty"
			// if it's not the same continue
			if ((uint8)m_pAG2I->m_pEvaluator->GetPropertyInt(pAnimLayer->GetMergeOperator()) != pAnimLayer->GetTimeControlProperty())
				continue;
		}

		int16 iAnim = pAnimLayer->GetAnimIndex();
		//		ConOutL(CStrF("iAnim %d", iAnim));
		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (!pAnimLayerSeq)
			continue;

		M_ASSERT(m_pAG2I->m_pEvaluator, "!");
		m_pAG2I->m_pEvaluator->AG2_RefreshStateInstanceProperties(_pContext, this);

		CMTime ContinousTime;
		CMTime LoopedTime;
		CMTime Zero = CMTime::CreateFromSeconds(0.0f);
		fp32 TimeScale;
		if (m_bHasSyncAnim)
		{
			fp32 Time;
			// Only get anim events from the dominating layer
			if ((jAnimLayer == 0 && m_SyncAnimScale > 0.5f) || 
				(jAnimLayer > 0 && m_SyncAnimScale <= 0.5f))
				continue;
			GetSyncAnimTime(_pContext,jAnimLayer,Time,TimeScale);
			ContinousTime = LoopedTime = CMTime::CreateFromSeconds(Time);
		}
		else if (m_bHasAdaptiveTimeScale)
		{
			fp32 Time;
			GetAdaptiveTime(_pContext,jAnimLayer,Time,TimeScale);
			ContinousTime = LoopedTime = CMTime::CreateFromSeconds(Time);
		}
		else
		{
//			int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
			TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
			if (!(Flags & CXR_ANIMLAYER_VALUECOMPARE) && pAnimLayer->GetTimeControlProperty())
				ContinousTime = CMTime::CreateFromSeconds(pAnimLayerSeq->GetDuration() * m_pAG2I->m_pEvaluator->GetPropertyFloat(pAnimLayer->GetTimeControlProperty()));
			else
				ContinousTime = _pContext->m_GameTime - m_EnterTime;//m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();
			ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) + ContinousTime.Scale(TimeScale);
			LoopedTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);
		}

		if (LoopedTime.Compare(Zero) < 0.0f)
			LoopedTime = Zero;

		// Страховка от NaN во времени слоя. Сравнение выше NaN не ловит
		// (любое сравнение с NaN ложно), а клип, посчитанный в NaN, отдаёт
		// первый кадр -- поза замирает молча. Источник NaN найден и закрыт
		// (Riddick_SyncSliceOK), но цена проверки нулевая, а цена молчания
		// -- несколько прогонов вслепую.
		{
			const fp32 LT = LoopedTime.GetTime();
			if (!(LT > -1.0e9f && LT < 1.0e9f))
			{
				static int s_n = 0;
				if (s_n < 8)
				{
					++s_n;
					fprintf(stderr, "[ANIMNAN] layer time not finite: iState=%d iAnim=%d scale=%.3f -> clamped to 0\n",
						(int)m_iState, (int)iAnim, TimeScale);
					fflush(stderr);
				}
				LoopedTime = Zero;
			}
		}

		uint32 StateFlags = pState->GetFlags(0);

		_pLayers[nLayers].Create3(pAnimLayerSeq, LoopedTime, TimeScale, 1.0f, pAnimLayer->GetBaseJointIndex(), Flags);
		if (StateFlags & CHAR_STATEFLAG_LAYERADJUSTFOROFFSET)
			_pLayers[nLayers].m_TimeOffset = m_Enter_AnimTimeOffset;

		M_ASSERT(0x7fc00000 != (uint32&)_pLayers[nLayers].m_Time, "!");
		_pLayers[nLayers++].m_ContinousTime = ContinousTime;
	}

	_nLayers = nLayers;
}

void CWAG2I_StateInstance::GetEventLayers(const CWAG2I_Context* _pContext, CEventLayer* _pLayers, int& _nLayers)
{
	MSCOPE(CWAG2I_StateInstance::GetAnimLayerSeqs, CWAG2I);

	if (_nLayers < 1)
		return;

	int nMaxLayers = _nLayers;
	_nLayers = 0;

	if (!m_bHasAnimation)
		return;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph, "Invalid Animgraph");
	if (!pAnimGraph)
		return;

	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return;

	int nLayers = 0;
	int NumAnimLayers = pState->GetNumAnimLayers();
	// This shouldn't happen
	if (m_liLastEventKey.Len() < NumAnimLayers)
	{
		ConOutL("CWAG2I_StateInstance::GetEventLayers: WARNING EVENTKEY NOT IN SYNC WITH LAYERS");
		return;
	}
	for (int jAnimLayer = 0; jAnimLayer < NumAnimLayers; jAnimLayer++)
	{
		if (nLayers >= nMaxLayers)
			break;

		int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		M_ASSERT(pAnimLayer,"CWAG2I_StateInstance::GetAnimLayerSeqs : Invalid animlayer");
		if (!pAnimLayer)
			continue;
		uint32 Flags = pAnimLayer->GetAnimFlags();
		// Skip value compare layers for now
		if (Flags & CXR_ANIMLAYER_VALUECOMPARE)
		{
			// Ok, compare int value in "m_iMergeOperator" with value in "m_iTimeControlProperty"
			// if it's not the same continue
			if ((uint8)m_pAG2I->m_pEvaluator->GetPropertyInt(pAnimLayer->GetMergeOperator()) != pAnimLayer->GetTimeControlProperty())
				continue;
		}

		int16 iAnim = pAnimLayer->GetAnimIndex();
		//		ConOutL(CStrF("iAnim %d", iAnim));
		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (!pAnimLayerSeq)
			continue;

		M_ASSERT(m_pAG2I->m_pEvaluator, "!");
		m_pAG2I->m_pEvaluator->AG2_RefreshStateInstanceProperties(_pContext, this);

		CMTime ContinousTime;
		CMTime LoopedTime;
		CMTime Zero = CMTime::CreateFromSeconds(0.0f);
		fp32 TimeScale;
		int NumLoops = 0;
		if (m_bHasSyncAnim)
		{
			fp32 Time;
			// Only get anim events from the dominating layer
			if ((jAnimLayer == 0 && m_SyncAnimScale > 0.5f) || 
				(jAnimLayer > 0 && m_SyncAnimScale <= 0.5f))
				continue;
			GetSyncAnimTime(_pContext,jAnimLayer,Time,TimeScale);
			LoopedTime = CMTime::CreateFromSeconds(Time);
			CMTime Temp = (_pContext->m_GameTime - m_EnterTime).Scale(m_TimeScale) + CMTime::CreateFromSeconds(m_AnchorTime + m_Enter_AnimTimeOffset);
			NumLoops = Temp.GetNumModulus(pAnimLayerSeq->GetDuration());
		}
		else if (m_bHasAdaptiveTimeScale)
		{
			//GetAdaptiveTime(_pContext,jAnimLayer,Time,TimeScale);
			TimeScale = m_SyncAnimScale;
			CMTime Time = (_pContext->m_GameTime - m_EnterTime).Scale(TimeScale) + CMTime::CreateFromSeconds(m_AnchorTime + m_Enter_AnimTimeOffset);
			LoopedTime = Time.Modulus(pAnimLayerSeq->GetDuration());
			NumLoops = Time.GetNumModulus(pAnimLayerSeq->GetDuration());
		}
		else
		{
			//			int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
			TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
			if (!(Flags & CXR_ANIMLAYER_VALUECOMPARE) && pAnimLayer->GetTimeControlProperty())
				ContinousTime = CMTime::CreateFromSeconds(pAnimLayerSeq->GetDuration() * m_pAG2I->m_pEvaluator->GetPropertyFloat(pAnimLayer->GetTimeControlProperty()));
			else
				ContinousTime = _pContext->m_GameTime - m_EnterTime;//m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();
			ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) + ContinousTime.Scale(TimeScale);
			LoopedTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);
			fp32 Duration = pAnimLayerSeq->GetDuration();
			if (Duration > 0.0f && pAnimLayerSeq->GetLoopType() == ANIM_SEQ_LOOPTYPE_CONTINUOUS)
				NumLoops = ContinousTime.GetNumModulus(Duration);
		}

		if (LoopedTime.Compare(Zero) < 0.0f)
			LoopedTime = Zero;

		// Страховка от NaN во времени слоя. Сравнение выше NaN не ловит
		// (любое сравнение с NaN ложно), а клип, посчитанный в NaN, отдаёт
		// первый кадр -- поза замирает молча. Источник NaN найден и закрыт
		// (Riddick_SyncSliceOK), но цена проверки нулевая, а цена молчания
		// -- несколько прогонов вслепую.
		{
			const fp32 LT = LoopedTime.GetTime();
			if (!(LT > -1.0e9f && LT < 1.0e9f))
			{
				static int s_n = 0;
				if (s_n < 8)
				{
					++s_n;
					fprintf(stderr, "[ANIMNAN] layer time not finite: iState=%d iAnim=%d scale=%.3f -> clamped to 0\n",
						(int)m_iState, (int)iAnim, TimeScale);
					fflush(stderr);
				}
				LoopedTime = Zero;
			}
		}

		uint32 StateFlags = pState->GetFlags(0);

		_pLayers[nLayers].m_Layer.Create3(pAnimLayerSeq, LoopedTime, TimeScale, 1.0f, pAnimLayer->GetBaseJointIndex(), Flags);
		if (StateFlags & CHAR_STATEFLAG_LAYERADJUSTFOROFFSET)
			_pLayers[nLayers].m_Layer.m_TimeOffset = m_Enter_AnimTimeOffset;
		_pLayers[nLayers].m_Layer.m_ContinousTime = LoopedTime - CMTime::CreateFromTicks(4,_pContext->m_pWPhysState->GetGameTickTime());
/*		if (ContinousTime.GetTime() > pAnimLayerSeq->GetDuration())
			_pLayers[nLayers].m_Layer.m_ContinousTime = CMTime::CreateFromSeconds(0.0f);
		else
			_pLayers[nLayers].m_Layer.m_ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset);*/
		M_ASSERT(0x7fc00000 != (uint32&)_pLayers[nLayers].m_Layer.m_Time, "!");
		_pLayers[nLayers++].m_pKey = &m_liLastEventKey[jAnimLayer];
		if (pAnimLayerSeq->GetLoopType() == ANIM_SEQ_LOOPTYPE_CONTINUOUS && 
			NumLoops != m_lNumLoops[jAnimLayer] && LoopedTime.GetTime() > _pContext->m_TimeSpan)
		{
			// Set new loop number and reset key
			m_lNumLoops[jAnimLayer] = NumLoops;
			m_liLastEventKey[jAnimLayer] = 0;
		}
	}

	_nLayers = nLayers;
}

bool CWAG2I_StateInstance::GetSpecificAnimLayer(const CWAG2I_Context* _pContext, CXR_AnimLayer& _Layer, int _iSI, int32 _iAnim, int32 _StartTick) const
{
	MSCOPE(CWAG2I_StateInstance::GetAnimLayers, CWAG2I);

	fp32 TickTime = _pContext->m_pWPhysState->GetGameTickTime();
	CMTime StartTick = CMTime::CreateFromTicks(_StartTick-1, TickTime);
	if (!m_bHasAnimation || m_EnterTime.Compare(StartTick) < 0)
		return false;

	CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph,"Invalid animgraph");
	if (!pAnimGraph)
		return false;
	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return false;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return false;

	fp32 Blend, BlendIn, BlendOut;

	{
		if ((m_LeaveTime.Compare(AG2I_UNDEFINEDTIME) == 0))
		{
			if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
			else if (m_InvBlendInDuration > 0)
				BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
			else
				BlendIn = 1.0f;

			BlendOut = 1.0f;

			//ConOutL(CStrF("BlendIn = %3.3f", Blend));
		}
		else
		{
			if (m_InvBlendInDuration < 0.0f)
				BlendIn = m_InvBlendInDuration;
			else if (m_InvBlendInDuration > 0)
				BlendIn = Clamp01((_pContext->m_GameTime - m_BlendInStartTime).GetTime() * m_InvBlendInDuration);
			else
				BlendIn = 1.0f;

			if (m_InvBlendOutDuration < 0.0f)
				BlendOut = m_InvBlendOutDuration;
			else
				BlendOut = 1.0f - Clamp01((_pContext->m_GameTime - m_BlendOutStartTime).GetTime() * m_InvBlendOutDuration);

			//ConOutL(CStrF("BlendIn = %3.3f, BlendOut = %3.3f, Blend = %3.3f", BlendIn, BlendOut, Blend));
		}
	}

	if ((BlendIn < 0.0f) && (BlendOut < 0.0f))
		Blend = -3.0f;
	else
		Blend = BlendIn * BlendOut; // Smoother than Min(BlendIn, BlendOut)

	if (Blend == 0)
		return false;

//	int nLayers = 0;
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		int16 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		if (!pAnimLayer)
			continue;

		int16 iAnim = pAnimLayer->GetAnimIndex();
		if (iAnim != _iAnim)
			continue;

		const CXR_Anim_SequenceData* pAnimLayerSeq = pAnimGraph->GetAnimSequenceData(_pContext->m_pWorldData, iAnim);
		if (!pAnimLayerSeq)
			continue;

		fp32 LayerBlend = Blend * pAnimLayer->GetOpacity();

		if (LayerBlend == 0.0f)
			continue;

		M_ASSERT(m_pAG2I->m_pEvaluator, "!");
		m_pAG2I->m_pEvaluator->AG2_RefreshStateInstanceProperties(_pContext, this);

//		int16 iTimeControlProperty = pAnimLayer->GetTimeControlProperty();
		fp32 TimeScale = pAnimLayer->GetTimeScale() * m_TimeScale;
		CMTime ContinousTime;
		if (!(pAnimLayer->m_AnimFlags & CXR_ANIMLAYER_VALUECOMPARE) && pAnimLayer->GetTimeControlProperty())
			ContinousTime = CMTime::CreateFromSeconds(pAnimLayerSeq->GetDuration() * m_pAG2I->m_pEvaluator->GetPropertyFloat(pAnimLayer->GetTimeControlProperty()));
		else
			ContinousTime = _pContext->m_GameTime - m_EnterTime;//m_pAG2I->m_pEvaluator->AG2_EvaluateCondition(iTimeControlProperty, AG2_PROPERTYTYPE_CONDITION).GetTime();
		ContinousTime = CMTime::CreateFromSeconds(pAnimLayer->GetTimeOffset() + m_Enter_AnimTimeOffset) + ContinousTime.Scale(TimeScale);
		CMTime LoopedTime = pAnimLayerSeq->GetLoopedTime(ContinousTime);

		uint32 Flags = pAnimLayer->GetAnimFlags();
		uint32 StateFlags = pState->GetFlags(0);

		_Layer.Create3(pAnimLayerSeq, LoopedTime, TimeScale, Sinc(LayerBlend), pAnimLayer->GetBaseJointIndex(), Flags);

		if (StateFlags & CHAR_STATEFLAG_LAYERADJUSTFOROFFSET)
			_Layer.m_TimeOffset = m_Enter_AnimTimeOffset;

		M_ASSERT(0x7fc00000 != (uint32&)_Layer.m_Time, "!");
		_Layer.m_ContinousTime = ContinousTime;

		return true;
	}

	return false;
}

bool CWAG2I_StateInstance::HasSpecificAnimation(int32 _iAnim) const
{
	MSCOPESHORT(CWAG2I_StateInstance::HasSpecificAnimation);

	if (!m_bHasAnimation)
		return false;

	const CXRAG2* pAnimGraph = m_pAG2I->GetAnimGraph(m_iAnimGraph);
	M_ASSERT(pAnimGraph,"Invalid animgraph");
	if (!pAnimGraph)
		return false;
	const CXRAG2_State* pState = pAnimGraph->GetState(m_iState);
	if (!pState)
	{
		AG2_ReportBadState(__FUNCTION__, pAnimGraph, m_iAnimGraph, m_iState);
		return false;
	}

	if (m_EnterTime.Compare(AG2I_UNDEFINEDTIME) == 0)
		return false;

//	int nLayers = 0;
	for (int jAnimLayer = 0; jAnimLayer < pState->GetNumAnimLayers(); jAnimLayer++)
	{
		int32 iAnimLayer = pState->GetBaseAnimLayerIndex() + jAnimLayer;

		const CXRAG2_AnimLayer* pAnimLayer = pAnimGraph->GetAnimLayer(iAnimLayer);
		if (pAnimLayer == NULL)
			continue;

		if (pAnimLayer->GetAnimIndex() == _iAnim)
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------

#if	defined(M_RTM) && defined( AG2_DEBUG )
#warning "M_RTM and AG2_DEBUG at the same time (slow code)"
#endif

//--------------------------------------------------------------------------------
