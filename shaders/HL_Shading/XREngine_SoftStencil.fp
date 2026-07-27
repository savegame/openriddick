/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Soft stencil
					
	Author:			Mangus Högdahl
					
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

*param
{
	*envx		dUV
	*envx		GrainScale
    *envx       VPParam
    *envx       VPConst
    *envx       VPScale
}

*texture
{
	*tex2D_0		sampler_shadow
	*tex2D_1		sampler_depth
	*tex2D_2		sampler_grain
}

*attrib
{
    *texcoord0	tc_screen
}

*output
{
    *color      oCol
}


*source
{
	*INCLUDE "XR_FPUtil.fph"
	*INCLUDE "XR_FPDepth.fph"
	
	*doeet
	"
	"
}

*main
{
	*doeeet
	"
		float2 tc_grain = tc_screen.xy * GrainScale.xy + GrainScale.zw;
		float tex_grain = texture2D(sampler_grain, tc_grain.xy).r;
		float rot = frac(tex_grain * 7163.127) * 2.0 * 3.1415;

		float4 tex_depth = texture2D(sampler_depth, tc_screen.xy);
		float depth = ConvertDepth(tex_depth, VPConst);
		float d_dx = dFdx(depth);
		float d_dy = dFdy(depth);
		float2 r0 = normalize(float2(d_dy, d_dx));
		float2 r1 = float2(-r0.y, r0.x);
		
		float maxdxdy = max(abs(d_dx), abs(d_dy));
		r1 = r1 / max(1.0, maxdxdy);

		float radius = min(8.0, 512.0 / depth);
		
		float4 tex_s0 = texture2D(sampler_shadow, tc_screen.xy);

		float poissonx[8] = float[8](0.3, 0.527, -0.040, -0.670, -0.419, 0.440, -0.757, 0.574);
		float poissony[8] = float[8](0.0, -0.085, 0.536,  -0.179, -0.616, -0.639, 0.349, 0.685);
		
		float wsum = 0.0;
		float shadow = 0.0;
		for(int i = 0; i < 1; i++)
		{
			float cr = cos(rot);
			float sr = sin(rot);
							
			for(int t = 0; t < 8; t++)
			{
				float2 dv = float2(cr*poissonx[t] + sr*poissony[t],
								-sr*poissonx[t] + cr*poissony[t]);
								
				dv = float2(r0.x * dv.x +  r1.x * dv.y, 
							r0.y * dv.x +  r1.y * dv.y);
								
				float2 tc = dv.xy*radius*dUV.xy + tc_screen.xy;
				float s = texture2D(sampler_shadow, tc.xy).a;
				float d = ConvertDepth(texture2D(sampler_depth, tc.xy), VPConst);
				float w = saturate(1.0 - abs(d - depth) * 0.125);
				s *= w;
				shadow += s;
				wsum += w;
			}
			
			rot += 1.73;
		}
		
		shadow *= 1.0 / wsum;
		shadow = wsum > 0.0 ? shadow : tex_s0.a;
		shadow = lerp(shadow, tex_s0.a, saturate(1.0 - wsum*0.5));
		shadow = shadow * shadow;

		oCol = float4(1.0, 1.0, 1.0, shadow);
		
		
	"
}
