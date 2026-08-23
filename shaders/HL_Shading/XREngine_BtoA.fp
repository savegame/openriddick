/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Output B in A
					
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
	*doeet
	"
	"
}

*main
{
	*doeeet
	"
		float4 tex0 = texture2D(sampler_tex0, tc_mapping.xy);
		oCol = float4(1.0, 1.0, 1.0, tex0.b);
	"
}
