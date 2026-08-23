/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Render depth into RGB
					
	Author:			Jim Kjellin
					
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
	*usetexture		1
}

*generate
{
	*gen 0
	*gen usetexture
}

*param
{
	*env0	VPParam	// (Front, Back, 1 / Back, 0)
	*env1	VPConst	// (2 * Back, Back + Front, 2 * Back * Front, 2 * (Back - Front))
	*env2	viewscale
}

*attrib
{
	*if_usetexture
	{
		*texcoord0	tc0 // model * view (no projection)
	}
	*texcoord1	pixelinfo_v	// model * view (no projection)
}

*texture
{
	*if_usetexture
	{
		*tex2D_0		texture_alpha
	}
}

*output
{
	*color	oCol
}

*source
{
        *INCLUDE "XR_FPDepth.fph"
	*INCLUDE "XR_FPUtil.fph"
}

*main
{
	*dostuff
	"
		float4 fniss;

		fniss.rgb = ConvertDepthToPixel(pixelinfo_v.z, VPConst);
	"

	*if_usetexture
	"
		fniss.a = tex2D(texture_alpha, tc0.xy).a;
	"
	
	*ifnot_usetexture
	"
		fniss.a = 1.0;
	"
	
	*dostuff
	"
		oCol = fniss;
	"
}
