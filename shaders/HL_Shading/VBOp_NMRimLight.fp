/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			VBOp_NMRimLight
					
	Author:			Patrik Willbo
					
	Copyright:		Starbreeze AB 2007
	
	Description:	Program for CXR_VBOperator_NMRimLight
					
	History:
		07-11-30	Created file
		08-06-03	Converted shader to hls
\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
    *name VBOp_NMRimLight
}

*flags
{
	*MaskMap			0x0001
	*FlipV				0x0002
	*AlphaToCoverage	0x0004
	*ProjCoord			0x0008
	*SmoothStep			0x0010
}

*generate
{
    *permute MaskMap+FlipV+AlphaToCoverage+ProjCoord+SmoothStep
	{
		*gen 0
	}
}

*param
{
	*env0 Color
	*env1 Scroll
	*if_ProjCoord
	{
		*env Const_Proj0
		*env Const_Proj1
		*env Const_Proj2
		*env Const_Proj3
	}
	*if_SmoothStep
	{
		*env RimProp
	}
}

*texture
{
    *tex2D_0	TextureNormal
    *tex2D_1	TextureMask
    *tex2D_2	TextureDiffuse
}

*attrib
{
	*texcoord0 TexCoord
	*texcoord1 PixelPosition
	*texcoord2 IPTSEV
}

*output
{
    *color      oCol
}

*source
{
	*INCLUDE "XR_FPUtil.fph"
}

*main
{
	*if_MaskMap
	{
		*if_ProjCoord
		{
			*do
			"
				vec4 tcProj = vec4(
					dot(PixelPosition, Const_Proj0),
					dot(PixelPosition, Const_Proj1),
					dot(PixelPosition, Const_Proj2),
					dot(PixelPosition, Const_Proj3));
					
				vec2 Coord = tcProj.xy * splat2(1.0 / tcProj.w);
			"
			
			*if_FlipV
			"
				Coord.y = 1.0 - Coord.y;
			"
		}
		
		*ifnot_ProjCoord
		"
			vec2 Coord = TexCoord.xy;
		"
		
		*doscroll
		"
			Coord = (Coord * Scroll.zw) + Scroll.xy;
		"
	}

	*fetchtextures
	{
		*Normalmap
		"
			vec4 NormalMapTexel = texture2D(TextureNormal, TexCoord.xy);
		"
		
		*if_MaskMap
		"
			vec4 MaskMapTexel = texture2D(TextureMask, Coord);
		"
	}

	*donormalmaptsev
	"
		NormalMapTexel.rgb = ConvertNormalTexel(NormalMapTexel);
		vec3 TSEV = normalize(IPTSEV.xyz);
	"
	
	*dorimlight
	{
		*do
		"
			float rim = 1.0 - dot(NormalMapTexel.rgb, TSEV);
		"
		
		*if_SmoothStep
		"
			rim *= RimProp.x;
			rim = smoothstep(1.0 - RimProp.y, 1.0, rim);
		"
		
		*ifnot_SmoothStep
		"
			rim *= rim;
		"
		
		*if_MaskMap
		{
			*do
			"
				oCol = (rim * Color) * MaskMapTexel;
			"
			
			*ifnot_AlphaToCoverage
			"
				oCol.a = MaskMapTexel.a;
			"
		}
		
		*ifnot_MaskMap
		"
			oCol = rim * Color;
		"
	}
	
	*if_AlphaToCoverage
	"
		// Fetch diffuse texture for alphatocoverage
		oCol.a = texture2D(TextureDiffuse, TexCoord.xy).a;
	"
}
