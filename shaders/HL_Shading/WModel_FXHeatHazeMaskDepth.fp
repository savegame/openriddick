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

	*env	HazeParam			// < Speed , Frequency , Min , Max >
	*env	DepthParam			// < Radius , _ , _ , _ >
	*env	ProjMat0
	*env	ProjMat3
}

*texture
{
	*tex2D_0	ScreenTexture
	*tex2D_1	NormalTexture
	*tex2D_2	MaskTexture
	*tex2D_3	DepthTexture
}

*source
{
	*INCLUDE "XR_FPDepth.fph"
	*INCLUDE "XR_FPUtil.fph"
}

*main
{
	*do_0
	"
		// Calculate distortion strength depending on distance from camera [Min, Max]
		vec4 t = vec4(1.0, 0.0, ScreenCoord.z, 1.0);
		float Strength = min(HazeParam.z, (1.0 / max(HazeParam.w, dot(t, ProjMat3))) * dot(t, ProjMat0));
	
		// Retrive normal texture from scrolled texture coordinates
		vec4 Normal = tex2D(NormalTexture, TCMapping0.xy + (HazeParam.xy * splat2(FXParam1.x)));
		vec3 TCOffset0 = ConvertNormalTexel(Normal);

		// Apply mask on distortion strength
		vec4 Mask = tex2D(MaskTexture, TCMapping0.xy);
		Strength *= Mask.x * FXParam1.y;

		// Apply depth-blend on distortion strength
		vec4 depth = tex2D(DepthTexture, (ScreenCoord.xy / ScreenCoord.w) * FXParam0.xy + FXParam0.zw);
		float d_res = ConvertDepth(depth,VPConst);
		Strength *= saturate(abs(d_res - ScreenCoord.z) * DepthParam.x);

		// Retrive framebuffer pixel from offseted texture coordinate
		vec2 TCScreen = (ScreenCoord.xy / ScreenCoord.w) * FXParam0.xy + FXParam0.zw;
		vec4 Result = tex2D(ScreenTexture, TCScreen.xy + (TCOffset0.xy * splat2(Strength)));
	"
	
	*out
	"
		oCol = Result;
	"
}
