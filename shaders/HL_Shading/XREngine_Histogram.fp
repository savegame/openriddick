/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Histogram
					
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
	*envX		eRange
	*envX		eColor
}

*texture
{
	*tex2D_0	sampler_tex0
}

*attrib
{
	*texcoord0	tc_mapping
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
	*doeeet
	"
		float4 tex0 = texture2D(sampler_tex0, tc_mapping.xy);
		float i = dot(tex0.rgb, float3(0.3086, 0.6094, 0.0820));
		float4 result;
		result.rgb = eColor.rgb;
		result.a = ((i >= eRange.x) && (i < (eRange.y))) ? 1.0 : 0.0;
		oCol = result;
	"
}
