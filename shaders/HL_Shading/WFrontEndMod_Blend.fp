/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Blending used in gui
					
	Author:			
					
	Copyright:		Starbreeze AB 2008
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
	*type hls
	*flags nodebug
}

*param
{
	*env0 SrcDstBlend
}

*attrib
{
	*texcoord0 TexCoordSrc
	*texcoord1 TexCoordDst
}

*texture
{
	*tex2D_0	TextureSrc
	*tex2D_1	TextureDst
}

*output
{
	*color	outColor
}

*source
{
        *INCLUDE "XR_FPUtil.fph"
}

*main
{
	*do
	"
		float4 ColorSrc = SampleFPTexLinear2D(TextureSrc, TexCoordSrc.xy, SrcDstBlend.zw);
		float4 ColorDst = SampleFPTexLinear2D(TextureDst, TexCoordDst.xy, SrcDstBlend.zw);

		outColor = (ColorDst * splat4(SrcDstBlend.y)) + (ColorSrc * splat4(SrcDstBlend.x));
	"
}
