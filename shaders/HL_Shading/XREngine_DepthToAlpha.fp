/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Depth to alpha
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
}

*flags
{
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
}

*texture
{
	*tex2D_0		texture_depth
}

*attrib
{
    *texcoord0	tc_screen
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
		float4 tex_depth = texture2D(texture_depth, tc_screen.xy);
		float depth = ConvertDepth(tex_depth, VPConst);
		depth = min(depth * 0.00390625, 8.0);
		float4 final = float4(1.0, 1.0, 1.0, depth);
		
		oCol = final;
	"
}
