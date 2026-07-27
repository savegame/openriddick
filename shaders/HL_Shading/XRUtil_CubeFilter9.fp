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
    *env0       kernelstep
}

*texture
{
	*texCube_0		texture_cube0
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
		float3 xp0 = normalize(cross(tc0.xyz, float3(0,0,1)));		
		xp0 = xp0 * kernelstep.xyz * splat3(2.0);
		float3 xp1 = cross(tc0.xyz, xp0);
		float4 texel_00 = textureCube(texture_cube0, tc0.	xyz - xp0 - xp1);
		float4 texel_10 = textureCube(texture_cube0, tc0.xyz       - xp1);
		float4 texel_20 = textureCube(texture_cube0, tc0.xyz + xp0 - xp1);
		
		float4 texel_01 = textureCube(texture_cube0, tc0.xyz - xp0);
		float4 texel_11 = textureCube(texture_cube0, tc0.xyz);
		float4 texel_21 = textureCube(texture_cube0, tc0.xyz + xp0);
		
		float4 texel_02 = textureCube(texture_cube0, tc0.xyz - xp0 + xp1);
		float4 texel_12 = textureCube(texture_cube0, tc0.xyz       + xp1);
		float4 texel_22 = textureCube(texture_cube0, tc0.xyz + xp0 + xp1);
		
		oCol = (texel_00 + texel_10 + texel_20 + texel_01 + texel_11 + texel_21 + texel_02 + texel_12 + texel_22) * splat4(0.1111111111111);
	"
}
