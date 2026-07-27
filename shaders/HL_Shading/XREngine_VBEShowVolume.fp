 /*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Single-texture shader for linear blending

	Author:			Anders Ekermo

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
	*alpha	0x0001
}

*generate
{
	*gen 0
	*gen alpha
}

*texture
{
    *tex3D_0		Sampler0
}

*param
{
}

*attrib
{
	*texcoord0	tc0
}

*output
{
    *color      oCol
}

*source
{
   //     *INCLUDE "XR_FPUtil.fph"
}

*main
{
	*do
	"
		float4 col = texture3D(Sampler0, tc0.xyz);
	"
	*if_alpha
	{
		*do
		"
			col = splat4(col.a);
		"
	}

	*do2
	"
	    oCol = col;
	"
}

