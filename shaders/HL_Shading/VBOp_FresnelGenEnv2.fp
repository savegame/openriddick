/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Fresnel + Bump mapped environment mapping (FresnelGenEnv surface operator)
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2004
					
	History:
		2007-09-03:	Combined VBOp_Fresnel and VBOp_GenEnv2 programs to save an extra pass
					and get around the DestAlphaBlend issues with them.

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*fog			0x0001
	*fogcube 		0x0002
	*fogsky 		0x0004
	*multinormal	0x0008
}

*generate
{
	*gen 0
	*gen multinormal
	*permute fogcube+fogsky
	{
		*gen fog
		*gen fog + multinormal
	}
}

*output
{
    *color oCol
}

*attrib
{
    *color      vCol
    *texcoord0  TCMapping0
    *texcoord1	tcTSLV
    *texcoord2  tc1
    *texcoord3  tc2
    *texcoord4  tc3
    *if_fog
    {
        *texcoord6    ScreenCoord
        *texcoord7    Fog1
    }
}

*param
{
	*if_fog
    {
        *env   FogConst0
        *env   FogConst1
        *env   FogConst2
        *env   FogConst3
    }
}

*texture
{
	*tex2D_0	sampler_Normal0
	*texCUBE_1	sampler_Cube
	*if_multinormal
	{
		*tex2D_2	sampler_Normal1
	}
	*if_fog
     {
         *tex2D_6 FogTex        //Can't rename these
         *texCUBE_7 FogCubeTex
     }
}

*source
{
	*INCLUDE "XR_FPUtil.fph"
	
	*if_fog
	{
		*INCLUDE "Include_XREngine_Fog_HL.fph"
	}
}

*main
{
	*do1
	"
		float4 tex_normal = texture2D(sampler_Normal0, TCMapping0.xy);
		float3 normal = ConvertNormalTexel(tex_normal);
		
		float3 ftemp = tcTSLV.xyz * splat3(1.0 / sqrt(dot(tcTSLV.xyz,tcTSLV.xyz)));
	"
	
	*if_multinormal
	"
		float4 tex_normalmulti = texture2D(sampler_Normal1, TCMapping0.xy);
		float3 normalmulti = ConvertNormalTexel(tex_normalmulti);
		float AlphaScale = saturate(1.0 - dot(normalmulti, ftemp));
	"
	
	*ifnot_multinormal
	"
		float AlphaScale = saturate(1.0 - dot(normal, ftemp));
	"

	*do2
	"
		float3 u = float3(dot(tc1.xyz, normal), dot(tc2.xyz, normal), dot(tc3.xyz, normal));
		float3 e = float3(tc1.w, tc2.w, tc3.w);
		
		float3 texcoord = u * splat3((1.0 / dot(u, u)) * dot(e, u) * 2.0) - e;
		float4 tex_cube = texCUBE(sampler_Cube, texcoord);
		
		float4 Result = tex_cube * vCol;
		Result.a = vCol.a * AlphaScale;
	"
	
	*if_fog
	{
		*dofog
		"
			float4 FogResult;
			float FogAlphaScale;
			DoFog(FogConst0,FogConst1,FogConst2,FogConst3,ScreenCoord,Fog1,FogResult,FogAlphaScale);
			
			Result.rgb = lerp(Result.rgb, FogResult.rgb, FogResult.a);
			Result.a *= FogAlphaScale;
		"
	}
	
	*out
	"
		oCol = Result;
	"
}
