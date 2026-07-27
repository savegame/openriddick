/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Depth line used in scanner
					
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

*attrib
{
	*color		vCol
	*texcoord0	TCMapping0
	*texcoord1	PixelPosition
	*texcoord2	IPTSEyeVec
	*texcoord3	ProjCoord
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

	*env	ColorParam			// < R , G , B , A >
}

*texture
{
	*tex2D_0	ScreenTexture
	*tex2D_1	DiffuseTexture
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
		vec4 ScreenColor = tex2D(ScreenTexture, (ScreenCoord.xy / ScreenCoord.w) * FXParam0.xy + FXParam0.zw);
		vec4 Normal = vec4(1.0, 0.0, 0.0, 0.0);
		
		vec4 Diffuse = tex2D(DiffuseTexture, TCMapping0.xy);
		
		vec4 TSEyeVec = normalize(IPTSEyeVec);
		vec4 Result = ColorParam * splat4(saturate(1.0 - abs(dot(Normal, TSEyeVec))));
		Result.a *= FXParam1.y * Diffuse.a;

		oCol = Result;
	"
}
