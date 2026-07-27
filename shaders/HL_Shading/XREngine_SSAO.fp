/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Screen space ambient occlusion
					
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
    *envx       VPDepthScale
    *envx       VPParam
    *envx       VPConst
    *envx       VPScale
}

*texture
{
	*tex2D_0		sampler_depth
	*tex2D_1		sampler_grain
}

*attrib
{
    *color		vCol
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
//rot = 0.0;
		float4 tex_depth = texture2D(sampler_depth, tc_screen.xy);
		float depth = ConvertDepth(tex_depth, VPConst);
		float3 posv = ConvertDepthUVToPos(depth, tc_screen.xy, VPDepthScale).xyz;
		float3 n = float3(0.0, 0.0, -1.0);
		if (0 == 1)
		{
			float3 posdx = dFdx(posv);
			float3 posdy = dFdy(posv);
			const float ddepthclamp = 4.0;
			posdx.z = clamp(posdx.z, -ddepthclamp, ddepthclamp);
			posdy.z = clamp(posdy.z, -ddepthclamp, ddepthclamp);
			n = cross(posdx, posdy);
		}
		else
		{
/*			float d0 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(dUV.x, 0.0)), VPConst) - depth;
			float d1 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(0.0, dUV.y)), VPConst) - depth;
			float d2 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(-dUV.x, 0.0)), VPConst) - depth;
			float d3 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(0.0, -dUV.y)), VPConst) - depth;
			d0 = ((d0+d2) < 0.0) ? (d2 < d0 ? d0 : d2) : (d2 > d0 ? d0 : d2);
			d1 = ((d1+d3) < 0.0) ? (d3 < d1 ? d1 : d3) : (d3 > d1 ? d1 : d3);
			float3 posdx = ConvertDepthUVToPos(depth + d0, tc_screen.xy + float2(dUV.x, 0.0), VPDepthScale).xyz - posv;
			float3 posdy = ConvertDepthUVToPos(depth + d1, tc_screen.xy + float2(0.0, dUV.y), VPDepthScale).xyz - posv;
*/			
			float d0 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(dUV.x, 0.0)), VPConst);
			float d1 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(0.0, dUV.y)), VPConst);
			float d2 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(-dUV.x, 0.0)), VPConst);
			float d3 = ConvertDepth(texture2D(sampler_depth, tc_screen.xy + float2(0.0, -dUV.y)), VPConst);
			float3 posdx = ConvertDepthUVToPos(depth + d0-d2, tc_screen.xy + float2(dUV.x*2.0, 0.0), VPDepthScale).xyz - posv;
			float3 posdy = ConvertDepthUVToPos(depth + d1-d3, tc_screen.xy + float2(0.0, dUV.y*2.0), VPDepthScale).xyz - posv;
			const float ddepthclamp = 16.0;
			posdx.z = clamp(posdx.z, -ddepthclamp, ddepthclamp);
			posdy.z = clamp(posdy.z, -ddepthclamp, ddepthclamp);
			n = cross(posdx, posdy);
		}
//		n = (dot(n, n) > 0.000001) ? n : float3(0.0, 0.0, 1.0);
		n = normalize(n);
//		n = float3(0.0, 0.0, -1.0);
//		n = n*0.5 + splat3(0.5);
//		oCol = float4(n.x, n.y, n.z, 1.0);
//		return;
		
	
		float radius = 4.0; //min(16.0, 512.0 / depth);
		float depthscale = max(2.0, min(256.0, depth * 0.125));
		float depthscalercp = 1.0 / depthscale;

		float poissonx[8] = float[8](0.3, 0.527, -0.040, -0.670, -0.419, 0.440, -0.757, 0.574);
		float poissony[8] = float[8](0.0, -0.085, 0.536,  -0.179, -0.616, -0.639, 0.349, 0.685);
		
		float wsum = 0.0;
		float ao = 0.0;
		for(int i = 0; i < 2; i++)
		{
			float cr = cos(rot) * radius * (float(i) + 1.0);
			float sr = sin(rot) * radius * (float(i) + 1.0);
							
			for(int t = 0; t < 8; t++)
			{
				float2 dv = float2(cr*poissonx[t] + sr*poissony[t],
								-sr*poissonx[t] + cr*poissony[t]);
								
								
				float2 tc = dv.xy * radius * dUV.xy + tc_screen.xy;
				float d = ConvertDepth(texture2D(sampler_depth, tc.xy), VPConst);// + frac(tex_grain.x * 123.3 + cr*78623.7) * 32.0 - 16.0;
				float3 posv2 = ConvertDepthUVToPos(d, tc.xy, VPDepthScale).xyz;

				if (0 == 0)
				{
					float3 v = posv2 - posv;
			//		v.z = clamp(v.z, -32.0, 32.0);

					float3 v_norm = normalize(v);
					float w = saturate(dot(v_norm, n));
			//		w = min(w, 0.5) * 2.0;
					w = w * w;
				
					float o = saturate((d - depth) * depthscalercp) + saturate((depth - d - 16.0) * depthscalercp / 2.0);
					o = saturate(o);
					ao += o*w + 0.5*(1.0-w);
					wsum += 1.0;
				}
				else
				{
					d -= depth;
					d *= depthscalercp*1.0;
					
		/*			d = abs(d + 1.0) - 1.0;
					d *= (d < 0.0) ? 1.0 : 0.20;
					d = min(1.0, d);
		*/			
		//			d *= (d > 0.0) ? 0.25 : 1.0;
					d = min(1.0, abs(d + 1.0) - 1.0);
					
					ao += d;
					wsum += 1.0;
				}
			}
			
			rot += 1.73;
		}
		
		ao *= 1.0 / wsum;
//		ao = wsum > 0.0 ? ao : 0.0;		
//		float shadow = 1.0 - ao;
		ao = wsum > 0.0 ? ao : 1.0;		
		float shadow = (saturate(ao * 2.0 + 0.02));
//		float shadow = sqr(saturate((ao * 1.0 + 1.0)));
		shadow = max(0.0, shadow);

//shadow = 1.0;
		oCol = float4(shadow, shadow, shadow, 1.0) * vCol;
//		oCol.rgb = n * 0.25 + splat3(0.25);
	"
}
