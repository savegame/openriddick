/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Glow prep.
					
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
	*envX		eClampMin
	*envX		eClampMax
	*envX		eScissorRect
}

*texture
{
	*tex2D_0		sampler_tex0
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
	
	*doeet
	"
	"
}

*main
{
	*doeeet
	"
		float4 tex0 = texture2D(sampler_tex0, tc_mapping.xy);
		float isinside = 1.0;
		if ((tc_mapping.x < eScissorRect.x) ||
			(tc_mapping.y < eScissorRect.y) ||
			(tc_mapping.x > eScissorRect.z) ||
			(tc_mapping.y > eScissorRect.w))
			isinside = 0.0;
		oCol = lerp(float4(0.0, 0.0, 0.0, 0.0), max(eClampMin, min(eClampMax, tex0)), splat4(isinside));
//		oCol = splat4(isinside);
	"
}
