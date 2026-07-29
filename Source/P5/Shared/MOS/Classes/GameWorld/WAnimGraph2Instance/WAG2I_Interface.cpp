//--------------------------------------------------------------------------------

#include "PCH.h"

#include <stdio.h>    // RIDDICK_DBG_AG2FX diagnostic
#include <stdlib.h>   // getenv

#include "WAG2I.h"
#include "WAG2_ClientData.h"

//--------------------------------------------------------------------------------

bool CWAG2I::EvaluateCondition(const CWAG2I_Context* _pContext, const CXRAG2_ConditionNodeV2* _pNode, fp32& _TimeFraction)
{
	//MSCOPESHORT(CWAG2I::EvaluateCondition);
	M_ASSERT(m_pEvaluator, "!");
	// FIXME DIVIDE INTO FLOAT/INT/BOOL!!!!!!
	CXRAG2_ICallbackParams Params;
	CAG2Val Constant;
	int32 PropertyType = _pNode->GetPropertyType();
	switch(PropertyType)
	{
	case AG2_PROPERTYTYPE_CONDITION:
	case AG2_PROPERTYTYPE_FLOAT:
		Constant = CAG2Val::From(_pNode->GetConstantFloat());
		break;
	case AG2_PROPERTYTYPE_FUNCTION:
	case AG2_PROPERTYTYPE_INT:
		Constant = CAG2Val::From(_pNode->GetConstantInt());
		break;
	case AG2_PROPERTYTYPE_BOOL:
		Constant = CAG2Val::From(_pNode->GetConstantBool() != 0);
		break;
	}
	if (PropertyType == AG2_PROPERTYTYPE_CONDITION)
	{
		return m_pEvaluator->AG2_EvaluateCondition(_pContext, _pNode->GetProperty(),_pNode->GetPropertyType(), _pNode->GetOperator(), Constant, _TimeFraction);
	}
	else
	{
		return m_pEvaluator->AG2_DoProperty(_pContext, _pNode->GetProperty(),_pNode->GetPropertyType(), _pNode->GetOperator(), Constant, _TimeFraction);
	}
}

//--------------------------------------------------------------------------------

// RIDDICK_DBG_AG2FX=1: trace AnimGraph2 effect invocations.
//
// Why this is the interesting place: everything the game world does "by
// script" at an animation keyframe goes through here. A valve that animates
// but never opens its door, a turret that plays its fire animation without
// spawning a projectile, a weapon that never appears in a hand -- all of
// those are AG2 effects (Effect_ActionCutsceneSwitch, Effect_SwitchWeapon,
// Effect_ActivateItem ...) fired from an action or a reaction. If this trace
// stays silent while animations play, the break is upstream of the effect
// dispatch; if effect IDs do appear, the break is inside the specific effect.
static bool AG2FX_Enabled()
{
	static int s_On = -1;
	if (s_On < 0)
	{
		const char* e = getenv("RIDDICK_DBG_AG2FX");
		s_On = (e && *e && *e != '0') ? 1 : 0;
	}
	return s_On != 0;
}

void CWAG2I::InvokeEffects(const CWAG2I_Context* _pContext, CAG2AnimGraphID _iAnimGraph, int16 _iBaseEffectInstance, uint8 _nEffectInstances)
{
	MSCOPESHORT(CWAG2I::InvokeEffects);
	const CXRAG2* pAnimGraph = GetAnimGraph(_iAnimGraph);
	M_ASSERT(m_pEvaluator && pAnimGraph, "!");
	if (!m_pEvaluator || !pAnimGraph)
		return;

	for (int jEffectInstance = 0; jEffectInstance < _nEffectInstances; jEffectInstance++)
	{
		uint32 iEffectInstance = _iBaseEffectInstance + jEffectInstance;
		const CXRAG2_EffectInstance* pEffectInstance = pAnimGraph->GetEffectInstance(iEffectInstance);
		// GetEffectInstance() returns NULL for an out-of-range index; the
		// original code dereferenced it unconditionally.
		if (!pEffectInstance)
			continue;
		uint32 EffectID = pEffectInstance->m_ID;
		const CXRAG2_ICallbackParams& IParams = pAnimGraph->GetICallbackParams(pEffectInstance->m_iParams, pEffectInstance->m_nParams);

		if (AG2FX_Enabled())
		{
			static int s_nLogged = 0;
			if (s_nLogged < 600)
			{
				++s_nLogged;
				fprintf(stderr, "[AG2FX] effect id=%u nParams=%d p0=%d obj=%d\n",
					(unsigned)EffectID, (int)pEffectInstance->m_nParams,
					IParams.GetNumParams() > 0 ? (int)IParams.GetParam(0) : -1,
					(_pContext && _pContext->m_pObj) ? (int)_pContext->m_pObj->m_iObject : -1);
				fflush(stderr);
			}
		}

		m_pEvaluator->AG2_InvokeEffect(_pContext, EffectID, &IParams);
	}
}

//--------------------------------------------------------------------------------
