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

*texture
{
    *tex2D_0		Tex0
}

*param
{
    *env0     InvWH
    *env1     inColor
}

*attrib
{
	*texcoord0 tc0
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
    oCol = SampleFPTexLinear2D(Tex0, tc0.xy, InvWH.xy) * inColor;
"

