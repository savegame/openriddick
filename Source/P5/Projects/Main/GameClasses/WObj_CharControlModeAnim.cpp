#include "PCH.h"

#include <stdio.h>	// [MOVE] RIDDICK_DBG_MOVE report
#include <stdlib.h>	// getenv

#include "WObj_Char.h"

#define PLAYER_PHYS_COS_MAX_SLOPE 0.69465837045899728665640629942269f // 46 degrees
CVec3Dfp32 CWObject_Character::Char_ControlMode_Anim2(const CSelection& _Selection, CWObject_CoreData* _pObj, CWorld_PhysState* _pWPhysState, 
													int16& _Flags)
{
	MSCOPESHORT(CWObject_Character::Char_ControlMode_Anim);

	CWO_Character_ClientData *pCD = GetClientData(_pObj);
	M_ASSERT(pCD,"CWObject_Character::Char_ControlMode_Anim2: CLIENDATA INVALID");
	CWAG2I_Context AGContext(_pObj, _pWPhysState, pCD->m_GameTime);
	CVec3Dfp32 MoveVelocity(0.0f,0.0f,0.0f);
	CQuatfp32 RotVelocity;
	RotVelocity.Unit();
	CVec3Dfp32 VRet(0.0f,0.0f,0.0f);
	
	if (pCD->m_Phys_Flags & PLAYER_PHYSFLAGS_IMMOBILE)
		return VRet;

	if (pCD->m_AnimGraph2.GetPropertyBool(PROPERTY_BOOL_PERFECTPLACEMENT) && pCD->m_AnimGraph2.GetExactPositionState() == -1)
	{
		// Move towards destination
		const fp32 Factor = 0.3f;
		CMat4Dfp32 DestPos = pCD->m_AnimGraph2.GetDestination();
		CMat4Dfp32 CurrentPos = _pObj->GetPositionMatrix();
		CQuatfp32 CurrentRot, TargetRot,DeltaRot;
		pCD->m_AnimGraph2.GetUpVector().SetRow(CurrentPos,2);
		CurrentPos.RecreateMatrix(2,0);
		CurrentRot.Create(CurrentPos);
		DeltaRot = CurrentRot;
		DeltaRot.Inverse();
		TargetRot.Create(DestPos);
		DeltaRot = TargetRot * DeltaRot;
		TargetRot.Unit();
		TargetRot.Lerp(DeltaRot,Factor,CurrentRot);

		CVec3Dfp32 DeltaPos = DestPos.GetRow(3) - CurrentPos.GetRow(3);
		_pWPhysState->Object_SetVelocity(_pObj->m_iObject, DeltaPos * Factor);

		CAxisRotfp32 RotVelocityAR;
		CurrentRot.Normalize();
		RotVelocityAR.Create(CurrentRot);
		_pWPhysState->Object_SetRotVelocity(_pObj->m_iObject, RotVelocityAR);

		return 0;
	}
	else if (pCD->m_AnimGraph2.GetExactPositionState() != -1)
	{
		if (pCD->m_AnimGraph2.GetStateFlagsLo() & AG2_STATEFLAG_APPLYTURNCORRECTION)
		{
			CAxisRotfp32 RotVelAR;
			pCD->m_AnimGraph2.GetAG2I()->GetRotVelocityToDest(&AGContext, MoveVelocity, RotVelAR, pCD->m_AnimGraph2.GetExactPositionState(), pCD->m_TurnCorrectionTargetAngle);
			pCD->m_AnimRotVel = RotVelAR.m_Angle;
		}
		else
		{
			// Determine what type of exact position state...
			if (pCD->m_AnimGraph2.GetStateFlagsHi() & AG2_STATEFLAGHI_EXACTSTARTPOSITION)
				pCD->m_AnimGraph2.GetAG2I()->GetAnimVelocityFromDestination(&AGContext,MoveVelocity,RotVelocity,pCD->m_AnimGraph2.GetExactPositionState());
			else
				pCD->m_AnimGraph2.GetAG2I()->GetAnimVelocityToDestination(&AGContext,MoveVelocity,RotVelocity,pCD->m_AnimGraph2.GetExactPositionState());
		}

		_pWPhysState->Object_SetVelocity(_pObj->m_iObject, MoveVelocity);
		RotVelocity.Normalize();
		_pWPhysState->Object_SetRotVelocity(_pObj->m_iObject, RotVelocity);
		
		return 0;
	}
	
	int32 Res = pCD->m_AnimGraph2.GetAG2I()->GetAnimVelocity(&AGContext, MoveVelocity, RotVelocity,0);

	// RIDDICK_DBG_MOVE=1: the live root-motion path for a walking character.
	//
	// Everything the AI-driven walk does goes through here: GetAnimVelocity
	// samples the animation's move track, the result is stored as velocity in
	// units-per-TICK, and WServer_Phys.cpp integrates it once per tick without
	// any dt. So the only two numbers that matter are printed together: what
	// the animation asked for, and how far the object actually moved since the
	// previous tick.
	//
	// Reading it: |anim| is the requested step per tick, |real| the achieved
	// one. Multiply by the tick rate (30 Hz, verified by [RATE]) to get
	// units/second and compare against the character's authored speed. A
	// constant ratio of 1.5 between them -- or between either of them and the
	// authored speed -- points at a leftover 20 Hz constant on the producer
	// side; equal values that are simply too large point at the animation data
	// or the character's speed keys instead.
	{
		static int s_On = -1;
		if (s_On < 0)
		{
			const char* e = getenv("RIDDICK_DBG_MOVE");
			s_On = (e && *e && *e != '0') ? 1 : 0;
		}
		if (s_On)
		{
			static int16 s_lObj[8] = { 0 };
			static uint8 s_lCount[8] = { 0 };
			static CVec3Dfp32 s_lPrevPos[8];
			int iSlot = -1;
			for (int i = 0; i < 8; i++)
			{
				if (s_lObj[i] == _pObj->m_iObject) { iSlot = i; break; }
				if (s_lObj[i] == 0)
				{
					s_lObj[i] = _pObj->m_iObject;
					s_lPrevPos[i] = _pObj->GetPosition();
					iSlot = i;
					break;
				}
			}
			if (iSlot >= 0 && s_lCount[iSlot] < 30)
			{
				s_lCount[iSlot]++;
				const CVec3Dfp32 Pos = _pObj->GetPosition();
				const fp32 RealStep = (Pos - s_lPrevPos[iSlot]).Length();
				s_lPrevPos[iSlot] = Pos;
				// Прогон 32: фасинг и фактический курс в тех же единицах,
				// что MOVEANGLE у графа (градусы, GetAngle()*360). Если
				// face стабилен, а velang через каждые ~0.3 c переворачивается
				// на 180 -- клип своим корневым движением возит персонажа
				// туда-сюда (выбор клипа противоречит текущему движению);
				// если face сам прыгает на 180 -- сломан разворот тела.
				CVec3Dfp32 FaceDir = CVec3Dfp32::GetMatrixRow(_pObj->GetPositionMatrix(), 0);
				const fp32 FaceAng = CVec2Dfp32(FaceDir.k[0], FaceDir.k[1]).GetAngle() * 360.0f;
				const CVec3Dfp32 RealVel = _pObj->GetMoveVelocity();
				fp32 VelAng = -1000.0f;
				if (CVec2Dfp32(RealVel.k[0], RealVel.k[1]).Length() > 0.01f)
					VelAng = CVec2Dfp32(RealVel.k[0], RealVel.k[1]).GetAngle() * 360.0f;
				fprintf(stderr, "[MOVE] obj=%d res=%d anim=%.3f real=%.3f "
					"animV=(%.2f %.2f %.2f) face=%.0f velang=%.0f rotw=%.3f pos=(%.1f %.1f %.1f)\n",
					(int)_pObj->m_iObject, (int)Res, MoveVelocity.Length(), RealStep,
					MoveVelocity.k[0], MoveVelocity.k[1], MoveVelocity.k[2],
					FaceAng, VelAng, RotVelocity.k[3],
					Pos.k[0], Pos.k[1], Pos.k[2]);
				fflush(stderr);
			}
		}
	}
	if (Res == 0)
	{
		MoveVelocity = 0;
		RotVelocity.Unit();
	}
	else if (Res == 2)
	{
		// Exact position override
		_pWPhysState->Object_SetVelocity(_pObj->m_iObject, MoveVelocity);
		RotVelocity.Normalize();
		_pWPhysState->Object_SetRotVelocity(_pObj->m_iObject, RotVelocity);
		return 0;
	}

	bool bOnGround = false;
	if ((_pObj->m_ClientFlags & (PLAYER_CLIENTFLAGS_GHOST | PLAYER_CLIENTFLAGS_NOGRAVITY)) ||
		(pCD->m_Phys_Flags & PLAYER_PHYSFLAGS_NOGRAVITY) ||
		(pCD->m_AnimGraph2.GetStateFlagsLo() & AG2_STATEFLAG_NOGRAVITY))
	{
		bOnGround = true;
	}
	else
	{
		// Check if player is standing on the ground or not.
		CCollisionInfo CInfo;
		CMat4Dfp32 GroundPos = _pObj->GetPositionMatrix();
		CVec3Dfp32::GetMatrixRow(GroundPos, 3) += CVec3Dfp32(0,0,-0.02f);

		// NOTE: Assuming char origin is at the bottom of it's box  -mh
		CWO_PhysicsState PhysState(_pObj->GetPhysState());
		PhysState.m_Prim[0].m_DimZ = 1;
		PhysState.m_Prim[0].m_Offset[2] = 1;

		bOnGround = _pWPhysState->Phys_IntersectWorld(&_Selection, PhysState, _pObj->GetPositionMatrix(), GroundPos, _pObj->m_iObject, &CInfo);
		if (!CInfo.m_bIsValid)
			bOnGround = false;

		// Player is standing on the ground.
		if (bOnGround)
		{
			CVec3Dfp32 Normal = CInfo.m_Plane.n;

			// Project velocity parallell to ground plane.
			CVec3Dfp32 NewMoveVelocity = MoveVelocity - (Normal * (Normal * MoveVelocity));

			// If slope is small enough, scale to keep velocity, but along the ground plane.
			fp32 NewVeloSqr = NewMoveVelocity.LengthSqr();
			if ((Normal.k[2] > PLAYER_PHYS_COS_MAX_SLOPE) && (NewVeloSqr > 0))
			{
				fp32 OldVelo = MoveVelocity.Length();
				NewMoveVelocity *= OldVelo * M_InvSqrt(NewVeloSqr);
			}

			MoveVelocity = NewMoveVelocity;
		}
	}

	if (!bOnGround)
		_Flags |= 2;
	else if (pCD->m_iPlayer == -1 || Char_GetPhysType(_pObj) != PLAYER_PHYS_DEAD)// Turn off gravity when standing on ground.
		_Flags |= 1;

	// State Constants
	int32 StateFlags = 0;
	fp32 AnimMoveScale = 1;
	fp32 AnimRotScale = 1;

//	fp32 TurnScale = 0;
//	fp32 AvoidTurnScale = 0;
//	fp32 TurnToTargetScale = 0;

//	fp32 TurnAngleCenter = 0;
//	fp32 TurnAngleMinRange = 0;
//	fp32 TurnAngleMaxRange = 0;

	fp32 MaxTurnAngle = 0;
	int32 AnimPhysMoveType = 0;
	MoveVelocity *= AnimMoveScale;

	CAxisRotfp32 RotVelocityAR;
	if( AnimRotScale != 1.0f )
	{
		//JK-NOTE: Normalize is to fix crappy float precision
		RotVelocity.Normalize();
		RotVelocityAR.Create(RotVelocity);
		RotVelocityAR.m_Angle *= AnimRotScale;

		RotVelocityAR.CreateQuaternion(RotVelocity);
	}

	fp32 TotalTurnAngle = 0;

	if ((TotalTurnAngle != 0) && (MaxTurnAngle > 0))
	{
		// Both 
		TotalTurnAngle = ClampRange(TotalTurnAngle + MaxTurnAngle, MaxTurnAngle * 2.0f) - MaxTurnAngle;
		CQuatfp32 TotalTurnAngleQ; TotalTurnAngleQ.Create(CVec3Dfp32(0, 0, 1), TotalTurnAngle);
		RotVelocity *= TotalTurnAngleQ;
	}

	if ((!bOnGround) && !(StateFlags & AG2_STATEFLAG_NOGRAVITY))
		MoveVelocity[2] = _pWPhysState->Object_GetVelocity(_pObj->m_iObject)[2];
	
	if (!(pCD->m_AnimGraph2.GetStateFlagsLo() & (AG2_STATEFLAG_BEHAVIORENTER|AG2_STATEFLAG_BEHAVIOREXIT|AG2_STATEFLAG_BEHAVIORACTIVE)))
		MoveVelocity += pCD->m_AnimGraph2.GetPhysImpulse();
	// Modify the physimpulse velocity
	if (pCD->m_AnimGraph2.GetLastPhysImpulseAdjust() < pCD->m_GameTick)//bOnGround || pCD->m_Phys_nInAirFrames < 3)
	{
		CVec3Dfp32 v = pCD->m_AnimGraph2.GetPhysImpulse();
		CVec3Dfp32 DVel(0.0f,0.0f,0.0f);
		if (!bOnGround)
		{
			pCD->m_AnimGraph2.m_PhysImpulse.k[2] = 0.0f;
		}
		else
		{
			const fp32 dV2LenSqr = v.LengthSqr();
			if (dV2LenSqr > Sqr(0.001f))
				DVel = -v.Normalize() * Min(M_Sqrt(dV2LenSqr) * (1.0f / 2.0f), 2.6f);
		}
		pCD->m_AnimGraph2.m_PhysImpulse += DVel;
		pCD->m_AnimGraph2.GetLastPhysImpulseAdjust() = pCD->m_GameTick;
	}

	//JK-NOTE: Normalize is to fix crappy float precision
	RotVelocity.Normalize();
	RotVelocityAR.Create(RotVelocity);

	if (((MoveVelocity[0] != 0) || (MoveVelocity[1] != 0) || (MoveVelocity[2] != 0)) ||
		(RotVelocityAR.m_Angle != 0))
	{
		//		pCD->m_ExtraFlags = pCD->m_ExtraFlags | PLAYER_EXTRAFLAGS_FORCEMOVEPHYSICAL;
	}

	// Temporary test, should make this a little nicer

	// COMPENSATE FOR MOVING WHILE ROTATING

	// If we're applying turn correction, use something else than this...
	if ((pCD->m_AnimGraph2.GetStateFlagsLo() & AG2_STATEFLAG_USETURNCORRECTION) && 
		!pCD->m_AnimGraph2.GetPropertyBool(PROPERTY_BOOL_DISABLETURNCORRECTION))
	{
		// Find our own velocity instead..
		pCD->m_AnimRotVel = RotVelocityAR.m_Angle * RotVelocityAR.m_Axis.k[2];
		RotVelocityAR.Unit();
		// Rotate move velocity to current offset
		fp32 AngleOffset = pCD->m_TurnCorrectionTargetAngle;
		CVec3Dfp32 VelRotate(0,0,AngleOffset);
		CMat4Dfp32 Rotate;
		VelRotate.CreateMatrixFromAngles(2,Rotate);
		MoveVelocity = MoveVelocity * Rotate;
	}

	if (/*MoveVelocity.LengthSqr() > 2.0f &&*/ (AnimPhysMoveType != 0))
	{
		fp32 AngleOffset = pCD->m_AnimGraph2.GetPropertyFloat(PROPERTY_FLOAT_MOVEANGLEUNITCONTROL);/*(_pWPhysState->IsServer() ?  :
																	 LERP(pCD->m_TurnCorrectionLastAngle,pCD->m_TurnCorrectionTargetAngle,0.5f));*/
		AngleOffset -= GetAnimPhysAldreadyAdjusted(AnimPhysMoveType);
		CVec3Dfp32 VelRotate(0,0,AngleOffset);
		CMat4Dfp32 Rotate;
		VelRotate.CreateMatrixFromAngles(2,Rotate);
		MoveVelocity = MoveVelocity * Rotate;
	}

	// Modify velocity with the scale of the character
	fp32 VelocityScaleModifier = pCD->m_CharGlobalScale * (1.0f - pCD->m_CharSkeletonScale);
	MoveVelocity = MoveVelocity * VelocityScaleModifier;

	_pWPhysState->Object_SetVelocity(_pObj->m_iObject, MoveVelocity);
	_pWPhysState->Object_SetRotVelocity(_pObj->m_iObject, RotVelocityAR);

	return 0;
}
