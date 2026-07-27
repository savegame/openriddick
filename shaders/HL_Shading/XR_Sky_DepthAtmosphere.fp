/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Sky light scatter
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
}

*flags
{
	*lightscatter		1
}

*generate
{
	*gen 0
}

*param
{
    *env0       VPParam
    *env1       VPConst
    *env2       VPScale
    *env3		AtmRange
}

*texture
{
	*tex3D_0		texture_atm
	*tex2D_1		texture_depth
}

*attrib
{
    *texcoord3	tc_screen
}

*output
{
    *color      oCol
}


*source
{
        *INCLUDE "XR_FPDepth.fph"
	*INCLUDE "XR_FPUtil.fph"
	
	*doeet
	"
	"
}

*main
{
	*doeeet
	"
		float2 tc_screen_p = tc_screen.xy * splat2(1.0 / tc_screen.w);
		float4 tex_depth = texture2D(texture_depth, tc_screen_p);
		float depth = ConvertDepth(tex_depth, VPConst);
		float4 atm = texture3D(texture_atm, float3(tc_screen_p.x, tc_screen_p.y, depth * AtmRange.y));
	//	float4 vpos = ConvertDepthUVToPos(depth, tc_screen_p, VPScale);
		float4 final = float4(0.31, 0.01, 0.05, 0.75);
		final = atm;
		
		oCol = final;
	"
}
