/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR:EFBB post processing effects -
	                        High-level version

	Author:			Anders Ekermo; Original by Magnus Högdahl

	Copyright:		Starbreeze AB 2008

	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*samples8
}

*generate
{
	*gen samples8
	*gen 0
}

*output
{
    *color oCol
}

*attrib
{
    *texcoord0  tc0
    *texcoord1  tc1
    *texcoord2  tc2
    *texcoord3  tc3
    *if_samples8
    {
        *texcoord4  tc4
        *texcoord5  tc5
        *texcoord6  tc6
        *texcoord7  tc7
    }
}

*param
{
    *env       InColor
    *env       TcScr
}

*texture
{
     *tex2D_0     DiffuseTex
}

*source
{
        *INCLUDE "XR_FPUtil.fph"
}

*main
{
	*begin
	"
		vec4 Result;

		vec4 col0,col1,col2,col3;
		col0 = SampleFPTexLinear2D(DiffuseTex,tc0.xy,TcScr.xy)*splat4(4.0);
		col1 = SampleFPTexLinear2D(DiffuseTex,tc1.xy,TcScr.xy)*splat4(3.0);
		col2 = SampleFPTexLinear2D(DiffuseTex,tc2.xy,TcScr.xy)*splat4(2.5);
		col3 = SampleFPTexLinear2D(DiffuseTex,tc3.xy,TcScr.xy)*splat4(2.0);

		Result = (col0 + col1 + col2 + col3) * InColor;
	"

	*if_samples8
	"
		col0 = SampleFPTexLinear2D(DiffuseTex,tc4.xy,TcScr.xy)*splat4(1.5);
		col1 = SampleFPTexLinear2D(DiffuseTex,tc5.xy,TcScr.xy)*splat4(1.25);
		col2 = SampleFPTexLinear2D(DiffuseTex,tc6.xy,TcScr.xy);
		col3 = SampleFPTexLinear2D(DiffuseTex,tc7.xy,TcScr.xy);

	    Result += (col0 + col1 + col2 + col3) * InColor;
	"

	*out
	"
		Result.a = InColor.a;
		oCol = Result;
	"
}
