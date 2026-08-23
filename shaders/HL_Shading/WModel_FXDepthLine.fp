/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Depth line used in scanner
					
	Author:
					
	Copyright:		Starbreeze AB 2008
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type hls
    *flags nodebug
}

*flags
{
}

*generate
{
	*gen 0
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

	*env	EnvColor			// < R , G , B , A >
	*env	GlowRadius			// < x , y , _ , _ >
}

*texture
{
	*tex2D_0	DepthScreen
}

*output
{
	*color	oCol
}

*source
{
        *INCLUDE "XR_FPDepth.fph"
}

*main
{
	*do
	"
		vec4 depth = tex2D(DepthScreen, (ScreenCoord.xy / ScreenCoord.w) * FXParam0.xy + FXParam0.zw);
		float d_res = ConvertDepth(depth,VPConst);

		float amount = 1.0 - abs(abs(d_res - ScreenCoord.z) - GlowRadius.x) * GlowRadius.y;
		clip(amount);

		oCol = EnvColor * splat4(amount);
	"
}
