 /*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Dual texture lerp

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2008
					
	History:
\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
}

*texture
{
    *tex2D_0	Sampler0
    *tex2D_1	Sampler1
}

*param
{
    *env0     eWHRcp0
    *env1     eWHRcp1
    *env2     eLerp
}

*attrib
{
	*texcoord0 tc0
	*texcoord1 tc1
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
"
    float4 col = lerp(
		SampleFPTexLinear2D(Sampler0, tc0.xy, eWHRcp0.xy), 
		SampleFPTexLinear2D(Sampler1, tc1.xy, eWHRcp1.xy),
		eLerp.a);
	col.rgb *= eLerp.rgb;
	oCol = col;
"

