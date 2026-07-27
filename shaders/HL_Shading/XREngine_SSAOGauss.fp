/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			SSAO Gauss
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
	*type  hls
    *flags nodebug //dopreparse
    *name SSAO Gauss
}

*flags
{
	*blur_y 1
	*slopecompensation	2
}

*generate
{
	*gen 0
	*gen blur_y
	*gen slopecompensation
	*gen slopecompensation + blur_y
}

*param
{
	*envx		dUV
	*envx		UVMinMax
	*envx		eScale
    *envx       VPParam
    *envx       VPConst
    *envx       VPScale
}

*texture
{
	*tex2D_0		sampler_tex0
	*tex2D_1		sampler_depth
	*tex2D_2		sampler_normalmap
}

*attrib
{
	*texcoord0	tc_mapping
	*texcoord1	tc_depth
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
		float ConvertDepth2(float4 _tex)
		{
			return _tex.w * 256.0;
		}
		
		float4 Blur(float4 _Tex, float _w, float _dref, float _scale, inout float4 _closest)
		{
			float ddiff = abs(_dref - ConvertDepth2(_Tex));
			float w = saturate(1.0 - ddiff * _scale);
			if (w < _closest.w)
			{
				_closest.w = w;
				_closest.rgb = _Tex.rgb;
			}
			w = sqrt(w);
			float4 ret;
			ret.rgb = _Tex.rgb * w;
			ret.a = w;
			return ret;
		}
		
		float4 BlurWithDepth(float4 _Tex, float _w, float _dref, float _scale, inout float4 _closest)
		{
			float ddiff = abs(_dref - ConvertDepth2(_Tex));
			float w = saturate(1.0 - ddiff * _scale);
			if (w < _closest.w)
			{
				_closest.w = w;
				_closest.rgb = _Tex.rgb;
			}
			w = sqrt(w);
			float4 ret;
			ret.rgb = _Tex.rgb * w;
			ret.a = w;
			return ret;
		}
	"
}

*main
{
	*ifnot_blur_y
	"
		float2 dxy = float2(dUV.x, 0.0);
		float2 dxypix = float2(1.0, 0.0);
		bool bComposite = false;
	"
	*if_blur_y
	"
		float2 dxy = float2(0.0, dUV.y);
		float2 dxypix = float2(0.0, 1.0);
		bool bComposite = true;
	"
	
	*if_slopecompensation
	{
		*do
		"
			float4 texdepth = texture2D(sampler_depth, tc_depth.xy);
			float depthref = ConvertDepth(texdepth, VPConst);
			float scale = 1.0 / (depthref / 30.0);
					
			float2 p0 = splat2(0.0);
			float2 p1 = dxypix*1.0;
			float2 p2 = dxypix*2.0;
			float2 p3 = dxypix*3.0;
			
			float4 texp0 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*p0, UVMinMax.xy, UVMinMax.zw));
			float4 texp1 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*p1, UVMinMax.xy, UVMinMax.zw));
			float4 texp2 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*p2, UVMinMax.xy, UVMinMax.zw));
			float4 texp3 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*p3, UVMinMax.xy, UVMinMax.zw));
			float4 texn1 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*p1, UVMinMax.xy, UVMinMax.zw));
			float4 texn2 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*p2, UVMinMax.xy, UVMinMax.zw));
			float4 texn3 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*p3, UVMinMax.xy, UVMinMax.zw));
			
			float4 closest = float4(0.0, 0.0, 0.0, 1000000000);
			float s = eScale.x;
			float4 sum = splat4(0.0);
			sum += BlurWithDepth(texp0, 1.0, depthref + s*dot(p0, texp0.yz), scale, closest);
			sum += BlurWithDepth(texp1, 1.0, depthref + s*dot(p1, texp1.yz), scale, closest);
			sum += BlurWithDepth(texp2, 1.0, depthref + s*dot(p2, texp2.yz), scale, closest);
			sum += BlurWithDepth(texp3, 1.0, depthref + s*dot(p3, texp3.yz), scale, closest);
			sum += BlurWithDepth(texn1, 1.0, depthref - s*dot(p1, texn1.yz), scale, closest);
			sum += BlurWithDepth(texn2, 1.0, depthref - s*dot(p2, texn2.yz), scale, closest);
			sum += BlurWithDepth(texn3, 1.0, depthref - s*dot(p3, texn3.yz), scale, closest);
			
			if (sum.w == 0.0)
			{
				sum.rgb = closest.rgb;
				sum.a = 1.0;
			}
			sum.rgb /= sum.w;
			if (bComposite)
			{
				sum.gb = splat2(sum.r);	
			}
			oCol.rgb = sum.rgb;
		//	oCol.rgb = texp0.rgb;
			oCol.a = depthref/256.0;
		"

	}
	*ifnot_slopecompensation
	{
		*dosomething
		"
			float4 texdepth = texture2D(sampler_depth, tc_depth.xy);
			
			float4 texc = texture2D(sampler_tex0, clamp(tc_mapping.xy, UVMinMax.xy, UVMinMax.zw));	
			float4 texp0 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy, UVMinMax.xy, UVMinMax.zw));
			float4 texp1 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*2.0, UVMinMax.xy, UVMinMax.zw));
			float4 texp2 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*3.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn0 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy, UVMinMax.xy, UVMinMax.zw));
			float4 texn1 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*2.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn2 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*3.0, UVMinMax.xy, UVMinMax.zw));

			float4 texp3 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*4.0, UVMinMax.xy, UVMinMax.zw));
			float4 texp4 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*5.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn3 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*4.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn4 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*5.0, UVMinMax.xy, UVMinMax.zw));
			
			float4 texp5 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*6.0, UVMinMax.xy, UVMinMax.zw));
			float4 texp6 = texture2D(sampler_tex0, clamp(tc_mapping.xy + dxy*7.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn5 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*6.0, UVMinMax.xy, UVMinMax.zw));
			float4 texn6 = texture2D(sampler_tex0, clamp(tc_mapping.xy - dxy*7.0, UVMinMax.xy, UVMinMax.zw));
			
			float depthref = ConvertDepth(texdepth, VPConst);
		//	float depthref = texc.w;
			float scale = 1.0 / (depthref / 120.0);
			
			float4 sum = float4(0.0, 0.0, 0.0, 0.0000);
	//		sum.rgb = texc.rgb;
	//		sum.w = 1.0;

			float4 closest = float4(0.0, 0.0, 0.0, 1000000000);

			sum += Blur(texc, 1.0, depthref, scale, closest);
			
			sum += Blur(texp0, 1.0, depthref, scale, closest);
			sum += Blur(texp1, 1.0, depthref, scale, closest);
			sum += Blur(texp2, 1.0, depthref, scale, closest);
			sum += Blur(texn0, 1.0, depthref, scale, closest);
			sum += Blur(texn1, 1.0, depthref, scale, closest);
			sum += Blur(texn2, 1.0, depthref, scale, closest);

			sum += Blur(texp3, 1.0, depthref, scale, closest);
			sum += Blur(texp4, 1.0, depthref, scale, closest);
			sum += Blur(texn3, 1.0, depthref, scale, closest);
			sum += Blur(texn4, 1.0, depthref, scale, closest);
			
			sum += Blur(texp5, 1.0, depthref, scale, closest);
			sum += Blur(texp6, 1.0, depthref, scale, closest);
			sum += Blur(texn5, 1.0, depthref, scale, closest);
			sum += Blur(texn6, 1.0, depthref, scale, closest);
			
			if (sum.w == 0.0)
			{
				sum.rgb = closest.rgb;
				sum.a = 1.0;
			}
			
	/*		sum += Blur(texp0, 0.75, depthref);
			sum += Blur(texp1, 0.50, depthref);
			sum += Blur(texp2, 0.25, depthref);
			sum += Blur(texn0, 0.75, depthref);
			sum += Blur(texn1, 0.50, depthref);
			sum += Blur(texn2, 0.25, depthref);*/

			sum.rgb /= sum.w;
			oCol.rgb = sum.rgb;
		//	oCol.rgb = splat3(depthref - ConvertDepth2(texc)) * 4.25;
		//	oCol.rgb = splat3(depthref/256.0);
		//	oCol.rgb = texdepth.rgb;
		//	oCol.rgb = texc.rgb;
			oCol.a = depthref/256.0;
		"
	}
}
