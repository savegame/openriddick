/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Glow 4.0
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
	*type  hls
    *flags nodebug //dopreparse
    *name XREngine_Glow4
}

*flags
{
	*manualbilinear 1
}

*generate
{
	*gen 0
	*gen manualbilinear
}

*param
{
	*envx		UVMinMax
	*envx		eEndMip_UV
	*envx		GlareBias
	*envx		GlareScale
	*envx		GlareGamma
	*envx		GlareInputClamp
}

*texture
{
	*tex2D_0	Sampler_Screen
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
	*INCLUDE "XR_FPUtil.fph"

	*doeet
	{
		*if_manualbilinear
		"
			float4 GlowSample(sampler2D _Tex,float2 _TC,float _Lod,float2 _WHRcp)
			{
					float2 Part = _WHRcp * pow(2.0,_Lod);
					_TC -= (Part * 0.5);

					// Determine fraction of pixels
					float2 MinTC = mod(_TC,Part);
					float2 Frac = MinTC / Part;

					// Make sure to sample at the center of each pixel so shader won't be broken for
					// Platforms where linear filtering actually exists
					_TC += (Part * 0.5) - MinTC;

					// Sample surrounding pixels
					float4 Clr0 = texture2DLod(_Tex,_TC,_Lod);
					float4 Clr1 = texture2DLod(_Tex,float2(_TC.x+Part.x,_TC.y),_Lod);
					float4 Clr2 = texture2DLod(_Tex,float2(_TC.x,_TC.y+Part.y),_Lod);
					float4 Clr3 = texture2DLod(_Tex,_TC+Part,_Lod);

					// Interpolate colors manually
					Clr0 = lerp(Clr0,Clr1,splat4(Frac.x));
					Clr1 = lerp(Clr2,Clr3,splat4(Frac.x));
					return lerp(Clr0,Clr1,splat4(Frac.y));
			}
		"
		*ifnot_manualbilinear
		"
			float4 GlowSample(sampler2D _Tex, float2 _tc, float _Lod, float2 _WHRcp)
			{
				return SampleFPTexLinear2DLod(_Tex, _tc, _Lod, _WHRcp);
			}
		"
	}
}

*main
{
	*dosomething
	"
		float endmip = eEndMip_UV.x;
		vec2 DimRcp = eEndMip_UV.zw;
		
/*		float3 mip2 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 7.0),DimRcp).rgb;
		float3 mip3 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 6.0),DimRcp).rgb;
		float3 mip4 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 5.0),DimRcp).rgb;*/
		
		float3 mip5 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 4.0),DimRcp).rgb;
		float3 mip6 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 3.0),DimRcp).rgb;
		float3 mip7 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 2.0),DimRcp).rgb;
		float3 mip8 = GlowSample(Sampler_Screen, tc0.xy, max(0.0, endmip - 1.0),DimRcp).rgb;
		float3 mip9 = GlowSample(Sampler_Screen, tc0.xy, endmip,DimRcp).rgb;
		mip5 = min(mip5, GlareInputClamp.rgb);
		mip6 = min(mip6, GlareInputClamp.rgb);
		mip7 = min(mip7, GlareInputClamp.rgb);
		mip8 = min(mip8, GlareInputClamp.rgb);
		mip9 = min(mip9, GlareInputClamp.rgb);	
		mip5 = mip5 * mip5;
		mip6 = mip6 * mip6;
		mip7 = mip7 * mip7;
		mip8 = mip8 * mip8;
		mip9 = mip9 * mip9;
//		TexGlow = (mip2 + mip3 + mip4 + mip5 + mip6 + mip7 + mip8 + mip9) * 0.2;
//		TexGlow *= 0.5;
		float3 TexGlow = (mip5 + mip6 + mip7 + mip8 + mip9) * 0.2;
		TexGlow = max(splat3(0.0), TexGlow + GlareBias.rgb);
		TexGlow *= GlareScale.rgb;
		TexGlow.r = pow(TexGlow.r, 0.5 * GlareGamma.r);
		TexGlow.g = pow(TexGlow.g, 0.5 * GlareGamma.g);
		TexGlow.b = pow(TexGlow.b, 0.5 * GlareGamma.b);
/*		TexGlow.r = sqrt(TexGlow.r);
		TexGlow.g = sqrt(TexGlow.g);
		TexGlow.b = sqrt(TexGlow.b);
*/
		oCol.rgb = TexGlow;
	//	oCol.rgb = splat3(0.0);
	
	//	oCol.rgb = GlowSample(Sampler_Screen, tc0.xy, 0.0,DimRcp).rgb;
		oCol.a = 1.0;
	"
}
