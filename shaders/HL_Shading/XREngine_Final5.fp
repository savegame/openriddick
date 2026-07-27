/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for XREngine final framebuffer paste
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2007, 2008
					
	Comments:
					Motion blur
					Depth of field
					Exposure
					Color correction
					Glare
					Grain & Jitter
					Vignetting
					
					All in one go! *
					
					*) Some preparation needed

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
    *name XREngine_Final5
}

*flags
{
	*motionblur	0x0001
	*exposure 0x0002
	*rgbmap 0x0004
	*glow 0x0008
	*dof 0x0010
	*motionblurhq 0x0020
}

*generate
{
	*permute motionblur+exposure+rgbmap+glow+dof+motionblurhq
	{
		*gen 0
	}
}

*param
{
	*env0 Exposure
	*env1 GlowClamp
	*env2 ScreenDimRcp		// dUV.x, dUV.y, "720p ratio", 1 / "720p ratio"
	*env3 PreExposure
	*env4 GrainOffsetScale
	*env5 RGBVolumeScale
	*env6 CCDimRcp
	*env7 MVLowClamp
	*env8 UVRangeMap		// (tc.xy + UVRangeMap.xy)*UVRangeMap.zw => [0..1, 0..1]
	*env9 ScreenClamp
	*env10 dUV

	*if_motionblur
	{
		*env UVOfs0
		*env UVOfs1
		*env UVOfs2
		*env UVOfs3
	}
}

*texture
{
    *tex2D_0		Sampler_Screen
    *tex2D_1		Sampler_Glow
    *tex2D_2		Sampler_RGBMap
    *tex2D_3		Sampler_MVRef
    *tex2D_4		Sampler_MV
    *tex2D_5		Sampler_DOF
    *tex2D_6		Sampler_Grain
    *tex2D_7		Sampler_DOFBlur
    *tex3D_8		Sampler_RGBVolumeMap
    *tex2D_9		Sampler_Grain2
}

*attrib
{
	*texcoord0 tc0
	*texcoord1 tc1
//	*texcoord2 tc2
	*texcoord3 tc3
}

*output
{
    *color      oCol
}

*source
{
        *INCLUDE "XR_FPUtil.fph"
	*do
	"
		#define NUM_POISSON_TAPS 8
		
		CONST_ARRAY_START(float2, g_Poisson, NUM_POISSON_TAPS)
		float2( 0.100000, 0.300000 ),
		float2( 0.527837,-0.085868 ),
		float2(-0.040088, 0.536087 ),
		float2(-0.670445,-0.179949 ),
		float2(-0.419418,-0.616039 ),
		float2( 0.440453,-0.639399 ),
		float2(-0.757088, 0.349334 ),
		float2( 0.574619, 0.685879 )
		CONST_ARRAY_END;

		float2 ConvertMV(float2 _mv)
		{
			_mv = (_mv.xy - splat2(0.5)) * splat2(2.0);
#ifdef target_glsl
			_mv.y = -_mv.y;
#endif
			return _mv;
		}
	"
}

*main
{
	*doeet1
	"
		const float basepoissonradius = 0.75;   

		float4 TexScreen = SampleFPTexLinear2DLod(Sampler_Screen, tc0.xy, 0.0,ScreenDimRcp.xy);
		
		float4 final = TexScreen;
		final.a = 1.0;
		final.rgb *= splat3(PreExposure.y);
		final.rgb = final.rgb * final.rgb;

		float Ratio720p = ScreenDimRcp.z;
		float Ratio720pRcp = ScreenDimRcp.w;

		float4 grainfull = texture2D(Sampler_Grain2, tc0.xy * GrainOffsetScale.zw + GrainOffsetScale.xy);
		float grain = grainfull.x;
	"
	
	*if_glow
	{
		*if_motionblurhq
		"
			const int nSamples = 7;
			const int nPass = 1;
			const float MVLodScale = 5.0;
		"
		*ifnot_motionblurhq
		"
			const int nSamples = 3;
			const int nPass = 1;
			const float MVLodScale = 10.0;
		"
		
		*if_motionblur
		"
			float2 tc_mvlow = clamp(tc0.xy, MVLowClamp.xy, MVLowClamp.zw);
			//float4 TexMV = texture2D(Sampler_MV, tc_mvlow.xy);
			float4 TexMV = SampleFPTexLinear2D(Sampler_MV, tc_mvlow.xy,ScreenDimRcp.xy * 4.0);
			float2 MV = ConvertMV(TexMV.xy);
		"
		*ifnot_motionblur
		"
			float4 TexMV = splat4(0.0);
			float2 MV = splat2(0.0);
			float2 UVOfs0 = splat2(0.0);
			float2 UVOfs1 = splat2(0.0);
			float2 UVOfs2 = splat2(0.0);
			float2 UVOfs3 = splat2(0.0);
		"
		*do1
		"
			float2 lMVOffset[9];
			lMVOffset[0] = float2(0.0, 0.0);
			lMVOffset[1] = MV.xy * UVOfs1.xy;
			lMVOffset[2] = -MV.xy * UVOfs1.xy;
			lMVOffset[3] = MV.xy * UVOfs0.xy;
			lMVOffset[4] = MV.xy * UVOfs2.xy;
			lMVOffset[5] = -MV.xy * UVOfs0.xy;
			lMVOffset[6] = -MV.xy * UVOfs2.xy;
			lMVOffset[7] = MV.xy * UVOfs3.xy;
			lMVOffset[8] = -MV.xy * UVOfs3.xy;
		"
		
		*if_dof
		{
			*do
			"
				float4 TexMVRef = texture2D(Sampler_MVRef, tc0.xy);
			"
			*if_motionblur
			"
				// Don't sample same pixel twice.
				float DoFZRef = TexMVRef.b;
			"
			*ifnot_motionblur
			"
				float DoFZRef = texture2D(Sampler_MVRef, tc0.xy).b;
			"

			
			*do2
			"
				// ----------------------------------------------------------------------------
				// Motion Blur, DoF, Jitter
				// ----------------------------------------------------------------------------
				
				float dofbasemip = PreExposure.w;

				float4 tcbase = float4(tc0.x, tc0.y, tc0.x, tc0.y);

				float4 dofout = splat4(0.0);
				float4 dofouth = splat4(0.0);

				float MVLod = clamp(log2(length(MV.xy)*MVLodScale*Ratio720p),0.0,5.0);
		
				float dofnear = saturate(1.0 - 2.0*TexMV.b);
				float doffar = saturate(2.0*DoFZRef - 1.0);
				float doffarblur = saturate(2.0*TexMV.a - 1.0);
				float dofness = lerp(doffar, doffarblur, saturate(max(doffar, dofnear) * 2.0));
				dofness = max(dofness, dofnear);

				float dofrelmip = log2(max(dofness, 0.00000001) * 1.0/32.0f);				
				float doflod = max(MVLod, 8.0 + dofbasemip + dofrelmip);
					
				float2 hradius = ScreenDimRcp.xy * splat2(basepoissonradius * exp2(max(0.0, doflod - 1.0)));	// 
				
				for(int iPass = 0; iPass < nPass; iPass++)
				{
					float rot = frac(grain*337.76123) * 2.0 * 3.1415 + float(iPass*1.76867);
					float cr = cos(rot);
					float sr = sin(rot);
					
					for(int t = 0; t < nSamples; t++)
					{
						float2 dv = float2(cr*g_Poisson[t].x + sr*g_Poisson[t].y,
										-sr*g_Poisson[t].x + cr*g_Poisson[t].y);
										
						float2 tcdof = hradius*dv.xy + tcbase.xy;
						tcdof.xy += lMVOffset[t].xy;
						tcdof = clamp(tcdof, ScreenClamp.xy, ScreenClamp.zw);

						float4 TexH = SampleFPTexLinear2DLodComplex(Sampler_Screen, tcdof.xy, doflod,ScreenDimRcp.xy);
						TexH.rgb *= splat3(PreExposure.y);
						TexH.rgb = TexH.rgb * TexH.rgb;
						
						dofouth.rgb += TexH.rgb;
					}
				}
				
				dofout.rgb *= 1.0 / float(nPass*nSamples);
				dofouth.rgb *= 1.0 / float(nPass*nSamples);
				
				float MVMag = saturate(length(MV.xy) * 2.0);
				dofout.rgb = lerp(dofouth.rgb, dofout.rgb, splat3(saturate((dofness-0.5)*2.0)));
				dofout.rgb = dofouth.rgb;
				final.rgb = dofout.rgb;
			"
		}
		
		*ifnot_dof
		{
			*do
			{
				*do
				"
					// ----------------------------------------------------------------------------
					// Motion Blur, Jitter
					// ----------------------------------------------------------------------------
					
					float2 hradius = ScreenDimRcp.xy*splat2(basepoissonradius);

					float MVLod = clamp(log2(length(MV.xy)*MVLodScale*Ratio720p),0.0,5.0);
					float4 mbout = splat4(0.0);
					for(int iPass = 0; iPass < nPass; iPass++)
					{
						float rot = frac(grain*337.76123) * 2.0 * 3.1415 + float(iPass*1.76867);
						float cr = cos(rot);
						float sr = sin(rot);
						
						for(int t = 0; t < nSamples; t++)
						{
							float2 dv = float2(cr*g_Poisson[t].x + sr*g_Poisson[t].y,
											-sr*g_Poisson[t].x + cr*g_Poisson[t].y);
							float2 tcdof = hradius*dv.xy + tc0.xy;
							tcdof.xy += lMVOffset[t].xy;
							tcdof = clamp(tcdof, ScreenClamp.xy, ScreenClamp.zw);

							float4 TexH = SampleFPTexLinear2DLodComplex(Sampler_Screen, tcdof.xy, MVLod,ScreenDimRcp.xy);
							TexH.rgb *= splat3(PreExposure.y);
							TexH.rgb = TexH.rgb * TexH.rgb;
							
							mbout.rgb += TexH.rgb;
						}
					}
					
					mbout.rgb *= 1.0 / float(nPass*nSamples);
					final.rgb = mbout.rgb;
				"
			}
		
		}
	}

	*if_glow
	"
		float4 TexGlow = saturate(SampleFPTexLinear2D(Sampler_Glow, min(max(tc1.xy, GlowClamp.xy), GlowClamp.zw), ScreenDimRcp.xy * 4.0));
		TexGlow.rgb *= PreExposure.y * 0.5;
		TexGlow.rgb = TexGlow.rgb * TexGlow.rgb;
		final.rgb = final.rgb + TexGlow.rgb - saturate(final.rgb * TexGlow.rgb);
	"

	*ifnot_glow
	"
		final.rgb = final.rgb * final.rgb;
	"

	*if_exposure
	{
		*doshared
		"
			// (1 - exp(-light*exposure))
			float3 e = min(-final.rgb * Exposure.rgb, splat3(-0.0000001));
	
			// Vignetting (should be more configurable I suppose)
			float2 v = ((tc0.xy + UVRangeMap.xy)*UVRangeMap.zw  - splat2(0.5)) * splat2(1.414213562373);
	//		float i = 1.0 - exp2(-(1.0 - length(v) * 1.0) * 3.0) * 0.7;
			float i = 1.4 - exp2(-(1.0 - length(v) * 1.0) * 7.0) * 1.3;
			e *= i;
			
			final.rgb = lerp(final.rgb, splat3(1.0) - float3(exp(e.r), exp(e.g), exp(e.b)), splat3(Exposure.a));	
		"
	}

	*dogammaspace
	"
		final.rgb = max(final.rgb, splat3(0.00000001));
		final.rgb = final.rgb * float3(rsqrt(final.r), rsqrt(final.g), rsqrt(final.b));
	"

	*if_rgbmap
	"
		// Color correction
		{
			final = saturate(final);			
			float3 tcrgb = final.rgb * splat3(RGBVolumeScale.y) + splat3(RGBVolumeScale.x);
			float3 s0 = SampleFPTexLinear3D(Sampler_RGBVolumeMap, tcrgb, CCDimRcp.zzz).rgb;
			final.rgb = s0;
			
		}
	"

	*dodither
	"
		{
			final.rgb = saturate(final.rgb + grainfull.rgb*0.05 - splat3(0.025));
	
		}
	"
	
	*doend
	"	
		oCol = final;
	"
}
