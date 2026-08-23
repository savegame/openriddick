/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Cube map blend
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
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
    *env0       blend
}

*texture
{
	*texCube_0		texture_cube0
	*texCube_1		texture_cube1
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
	*doeet
	"
	"
}

*main
{
	*doeeet
	"
		float4 texel_cube0 = textureCube(texture_cube0, tc0.xyz);
		float4 texel_cube1 = textureCube(texture_cube1, tc0.xyz);
		oCol = lerp(texel_cube0, texel_cube1, blend);
	"
}
