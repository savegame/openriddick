/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			

	Author:			

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
}

*generate
{
	*gen 0
}

*output
{
	*color		oCol
}

*attrib
{
	*color		vCol
	*texcoord0	TCMapping0
	*texcoord6	ScreenCoord
	*texcoord7	Fog1
}

*param
{
	// Viewport parameters
	*env	VPParam				// < Front , Back , 1.0 / Back , 0 >
	*env	VPConst				// < 2.0 * Back , Back + Front , 2.0 * Back * Front , 2.0 * (Back - Front) >
	*env	VPScale				// < VPScaleX , VPScaleY , ScreenW / ScaleX , ScreenH / ScaleY >

	// Pre-defined parameters
	*env	FXParam0			// < PixelU , PixelV , CenterU , CenterV >
	*env	FXParam1			// < AnimTime , EvalFade , _ , _ >

	*env	DepthBlendParam		// < Radius , _ , _ , _ >
	*env	ColorParam			// < R , G , B , A >
}

*texture
{
	*tex2D_0     DepthTexture
	*tex2D_1     DiffuseTexture
}

*source
{
	*INCLUDE "XR_FPDepth.fph"
}

*main
{
	*do_0
	"
		vec4 Result = tex2D(DiffuseTexture, TCMapping0.xy);
		vec4 depth = tex2D(DepthTexture, (ScreenCoord.xy / ScreenCoord.w) * FXParam0.xy + FXParam0.zw);
		
		float d_res = ConvertDepth(depth,VPConst);
		Result *= saturate(splat4(abs(d_res - ScreenCoord.z) * DepthBlendParam.x));
		
		Result.a *= FXParam1.y;
	"
	
	*out
	"
		oCol = Result;
	"
}
