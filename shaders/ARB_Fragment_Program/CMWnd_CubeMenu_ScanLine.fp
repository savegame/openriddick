/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Takes a regular colored texture and attempts to create a mask to use when
					creating the multiply color
					
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
	*env0 UVScale
	*env1 UVNoiseScale
}

*attrib
{
	*texcoord0 TexCoordMask
}

*texture
{
	*tex2D_0	TextureMask
	*tex2D_1	TextureInterlace
	*tex2D_2	TextureNoise
}

*output
{
	*color	outColor
}

*main
{
	*do
	"
		float2 TexCoordInterlace = TexCoordMask.xy * UVScale.xy;
		float2 TexCoordNoise = (TexCoordMask.xy * UVNoiseScale.zw) + UVNoiseScale.xy;
		
		float4 ColorMask = tex2D(TextureMask, TexCoordMask.xy);
		float4 ColorInterlace = tex2D(TextureInterlace, TexCoordInterlace);
		float4 ColorNoise = tex2D(TextureNoise, TexCoordNoise);
		
		ColorMask.a = 0.0;
		float Mask = 1.0 - min(1.0, dot(ColorMask, ColorMask) * 100.0);
		float MulColor = ColorInterlace.r - (ColorNoise.r * 0.1);

		outColor = splat4( lerp(min(1.0, MulColor + Mask), 1.0, UVScale.w) );
	"
}
