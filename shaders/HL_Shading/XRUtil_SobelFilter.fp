/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			XRUtil_SobelFilter
					
	Author:			Patrik Willbo
					
	Copyright:		Starbreeze AB 2008
	
	Description:	Sobel filter
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
    *name XRUtil_SobelFilter
}

*flags
{
	*clampuv	0x0001
	*ext		0x0002
}

*generate
{
	*permute clampuv+ext
	{
		*gen 0
	}
}

*param
{
    *env0 PixelUV
    *env1 ClampUV
    *env2 BackColor
    *env3 EdgeColor
    *if_ext
    {
		*env ExtColor
    }
}

*texture
{
    *tex2D_0	Texture
    *if_ext
    {
		*tex2D_1	TextureExt
	}
}

*attrib
{
    *texcoord0 TexCoord
    *if_ext
    {
		*texcoord1 TexCoordExt
    }
}

*output
{
    *color      oCol
}

*main
{
	*if_clampuv
	"
		float minu = max(TexCoord.x-PixelUV.x, ClampUV.x);
		float minv = max(TexCoord.y-PixelUV.y, ClampUV.y);
		float maxu = min(TexCoord.x+PixelUV.x, ClampUV.z);
		float maxv = min(TexCoord.y+PixelUV.y, ClampUV.w);
	"
	
	*ifnot_clampuv
	"
		float minu = TexCoord.x;
		float minv = TexCoord.y;
		float maxu = TexCoord.x;
		float maxv = TexCoord.y;
	"
	
	*do0
	"
		// Sample
		vec3 s00 = texture2D(Texture, vec2(minu, minv)).rgb;
		vec3 s01 = texture2D(Texture, vec2(TexCoord.x, minv)).rgb;
		vec3 s02 = texture2D(Texture, vec2(maxu, minv)).rgb;
		
		vec3 s10 = texture2D(Texture, vec2(minu, TexCoord.y)).rgb;
		vec3 s12 = texture2D(Texture, vec2(maxu, TexCoord.y)).rgb;
		
		vec3 s20 = texture2D(Texture, vec2(minu, maxv)).rgb;
		vec3 s21 = texture2D(Texture, vec2(TexCoord.x, maxv)).rgb;
		vec3 s22 = texture2D(Texture, vec2(maxu, maxv)).rgb;

		// Sobel filter in X and Y directions
		vec3 SobelX = s00 + 2.0 * s10 + s20 - s02 - 2.0 * s12 - s22;
		vec3 SobelY = s00 + 2.0 * s01 + s02 - s20 - 2.0 * s21 - s22;
		
		// Find edge
		vec3 Threshold = vec3(0.07, 0.07, 0.07) * 0.07;
		vec3 Edge = SobelX * SobelX + SobelY * SobelY;
		
		// Check thresholds
		Edge.r = ((Edge.x > Threshold.x) ? 1.0 : 0.0);
		Edge.g = ((Edge.y > Threshold.y) ? 1.0 : 0.0);
		Edge.b = ((Edge.z > Threshold.z) ? 1.0 : 0.0);
		
		float d = min(1.0, dot(Edge, splat3(1.0)));
		vec4 Color = BackColor + (d * EdgeColor);
	"
	
	*ifnot_ext
	"
		// Write output
		oCol.rgb = Color.rgb;
		oCol.a = 0.0;
	"
	
	*if_ext
	"
		vec4 MaskTexel = min(splat4(1.0), texture2D(Texture, TexCoord.xy));
		vec4 ExtTexel  = min(splat4(1.0), texture2D(TextureExt, TexCoordExt.xy));
		
		vec4 ExtMask = min(splat4(1.0), ((splat4(1.0) - ExtTexel) * ((1.0 - d) * MaskTexel)) * 2.0);
		oCol.rgb = Color.rgb + min(ExtColor, (ExtColor * ExtMask.r) + (ExtColor * ExtMask.g) + (ExtColor * ExtMask.b)).rgb;
		oCol.a = 0.0;
		
		//oCol.rgb = splat3(min(1.0, ExtMask.r * 2.0));
	"
}
