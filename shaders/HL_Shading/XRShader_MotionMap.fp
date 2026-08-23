 /*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for CXR_Shader::RenderMotionVectorsAndDOF
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2005

	History:		
		06/11/24	Created
		08/07/22	Converted to HLSL
\*____________________________________________________________________________________________*/

*_head_
{
	*type hls
	*flags 0
	*name XRShader_MotionMap
}

*flags
{
	*motionmap	0x0001
	*dofmap		0x0002
}

*generate
{
	// Motion map only
	*gen motionmap
	
	// Depth of field only
	*gen dofmap
	
	// Motion and depth of field
	*gen motionmap+dofmap
}

*param
{
	*env DOFParam0		// 1.0/(Focus-Near), 1.0/(Far-Focus), Focus
	*env DOFParam1		// Near[0, 0.5], Far[0.5, 1]
}

*attrib
{
	*texcoord0 VPPos0
	*texcoord1 VPPos1
}

*output
{
	*color oCol
}

*source
{
	*do
	"
		float CalculateDepthOfField(float _ViewspaceDepth, float3 _DOFParam, float2 _NearFarClamp)
		{
			float d = _ViewspaceDepth - _DOFParam.z;
			if (_ViewspaceDepth < _DOFParam.z)
			{
				// Inbetween near and focus in range [-1, 0]
				d = d * _DOFParam.x;
			}
			else
			{
				// Inbetween focus and far in range [0, 1]
				d = d * _DOFParam.y;
			}

			// Scale and bias into [0, 1] range and clamp near/far to a maximum blurriness
			return clamp((d * 0.5) + 0.5, _NearFarClamp.x, _NearFarClamp.y);
		}
	"
}

*main
{
	*if_motionmap
	"
		float3 RcpVPPos0 = VPPos0.xyz * splat3(1.0 / VPPos0.z);
		float3 RcpVPPos1 = VPPos1.xyz * splat3(1.0 / VPPos1.z);
		float3 dVPPos = RcpVPPos1 - RcpVPPos0;
#ifdef target_glsl
		float LenRcp = min(1.0, 0.5 / max(0.0001, length(dVPPos)));	// Limit MV length if it exceeds 0.5, but keeping direction intact.
		oCol.rgb = dVPPos * LenRcp + splat3(0.5);
#else
		oCol.rgb = saturate(dVPPos + splat3(0.5));
#endif
		oCol.a = 0.0;
	"
	*ifnot_motionmap
	"
		oCol = splat4(0.0);
	"

	*if_dofmap
	"
		oCol.b = CalculateDepthOfField(VPPos0.z, DOFParam0.xyz, DOFParam1.xy);
	"
}

