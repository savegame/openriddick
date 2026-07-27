/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			SSAO Poisson
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
	*type  hls
    *flags nodebug //dopreparse
    *name SSAO Poisson
}

*flags
{
	*blur_y 1
	*normalmap 2
	*rgbblur 4
}

*generate
{
	*gen 0
	*gen blur_y
	*gen normalmap
	*gen normalmap + blur_y
}

*param
{
	*envx		e_dUV
	*envx		e_UVMinMax
    *envx       e_GrainScaleOffset
	*envx		e_Scales
    *envx       e_VPParam
    *envx       e_VPConst
    *envx       e_VPScale
}

*texture
{
	*tex2D_0		sampler_tex0
	*tex2D_1		sampler_depth
	*tex2D_2		sampler_normalmap
	*tex2D_3		sampler_surfacenormalmap
	*tex2D_4		sampler_grain
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
		#define NUM_POISSON_TAPS 6
		
//		const float BlurRadius = 3.0;

		CONST_ARRAY_START(float2, g_Poisson, NUM_POISSON_TAPS)
			float2(-0.736902, -0.181753), 
			float2(-0.682553, 0.730836), 
			float2(-0.398527, 0.282052), 
			float2(-0.005502, -0.053257), 
			float2(0.031082, -0.667084), 
			float2(0.408940, 0.324645)
		CONST_ARRAY_END;
			 		
/*		CONST_ARRAY_START(float2, g_Poisson, NUM_POISSON_TAPS)
			float2( 0.100000, 0.300000 ),
			float2( 0.527837,-0.085868 ),
			float2(-0.040088, 0.536087 ),
			float2(-0.170445,-0.179949 ),
			float2(-0.419418,-0.616039 ),
			float2( 0.440453,-0.639399 ),
			float2(-0.757088, 0.349334 ),
			float2( 0.574619, 0.685879 )
		CONST_ARRAY_END;
*/
		
		float ConvertDepth2(float4 _tex)
		{
			return _tex.w * 256.0;
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
		
		float2 DiscTC(float2 _tc, float2 _sincos, float _Radius)
		{
			float2 tc = float2(
				_tc.x * _sincos.y + _tc.y * _sincos.x, 
				-_tc.x * _sincos.x + _tc.y * _sincos.y);
			return tc * _Radius;
		}

		float2 MapTC(float2 _tc,float4 _tc_mapping)
		{
			return clamp(_tc*e_dUV.xy + _tc_mapping.xy, e_UVMinMax.xy, e_UVMinMax.zw);
		}
	"
}

*main
{
	*ifnot_blur_y
	"
		float2 dxy = float2(e_dUV.x, 0.0);
		bool bComposite = false;
	"
	*if_blur_y
	"
		float2 dxy = float2(0.0, e_dUV.y);
		bool bComposite = true;
	"

	*dosomething
	"
		float4 texdepth = texture2D(sampler_depth, tc_depth.xy);

		float4 texgrain = texture2D(sampler_grain, tc_depth.xy*e_GrainScaleOffset.xy + e_GrainScaleOffset.zw);
	//	oCol = texgrain;
	//	return;

	//	float rot = frac(tc_mapping.x * 23.176258 + tc_mapping.y*17.9813) * 3.1415 * 2.0;
	//	rot = 3.1415 * 0.5;
		float rot = frac(texgrain.x * 17.128766);

		float2 sc = float2(sin(rot), cos(rot));

		float radius = e_Scales.y;
		float2 p0 = DiscTC(g_Poisson[0], sc, radius);
		float2 p1 = DiscTC(g_Poisson[1], sc, radius);
		float2 p2 = DiscTC(g_Poisson[2], sc, radius);
		float2 p3 = DiscTC(g_Poisson[3], sc, radius);
		float2 p4 = DiscTC(g_Poisson[4], sc, radius);
		float2 p5 = DiscTC(g_Poisson[5], sc, radius);
	//	float2 p6 = DiscTC(g_Poisson[6], sc, radius);
	//	float2 p7 = DiscTC(g_Poisson[7], sc, radius);
		float4 tex0 = texture2D(sampler_tex0, MapTC(p0,tc_mapping));
		float4 tex1 = texture2D(sampler_tex0, MapTC(p1,tc_mapping));
		float4 tex2 = texture2D(sampler_tex0, MapTC(p2,tc_mapping));
		float4 tex3 = texture2D(sampler_tex0, MapTC(p3,tc_mapping));
		float4 tex4 = texture2D(sampler_tex0, MapTC(p4,tc_mapping));
		float4 tex5 = texture2D(sampler_tex0, MapTC(p5,tc_mapping));
	//	float4 tex6 = texture2D(sampler_tex0, MapTC(p6,tc_mapping));
	//	float4 tex7 = texture2D(sampler_tex0, MapTC(p7,tc_mapping));

		float depthref = ConvertDepth(texdepth, e_VPConst);
		float scale = 1.0 / (depthref / 30.0);

		float4 sum = float4(0.0, 0.0, 0.0, 0.0);

		float4 closest = float4(0.0, 0.0, 0.0, 1000000000);
		float s = e_Scales.x;
	"
	
	*if_rgbblur
	"
		sum += BlurWithDepth(tex0, 1.0, depthref, scale, closest);
		sum += BlurWithDepth(tex1, 1.0, depthref, scale, closest);
		sum += BlurWithDepth(tex2, 1.0, depthref, scale, closest);
		sum += BlurWithDepth(tex3, 1.0, depthref, scale, closest);
		sum += BlurWithDepth(tex4, 1.0, depthref, scale, closest);
		sum += BlurWithDepth(tex5, 1.0, depthref, scale, closest);
	//	sum += BlurWithDepth(tex6, 1.0, depthref, scale, closest);
	//	sum += BlurWithDepth(tex7, 1.0, depthref, scale, closest);
	"

	*ifnot_rgbblur
	"
		sum += BlurWithDepth(tex0, 1.0, depthref + s*dot(p0, tex0.yz), scale, closest);
		sum += BlurWithDepth(tex1, 1.0, depthref + s*dot(p1, tex1.yz), scale, closest);
		sum += BlurWithDepth(tex2, 1.0, depthref + s*dot(p2, tex2.yz), scale, closest);
		sum += BlurWithDepth(tex3, 1.0, depthref + s*dot(p3, tex3.yz), scale, closest);
		sum += BlurWithDepth(tex4, 1.0, depthref + s*dot(p4, tex4.yz), scale, closest);
		sum += BlurWithDepth(tex5, 1.0, depthref + s*dot(p5, tex5.yz), scale, closest);
	//	sum += BlurWithDepth(tex6, 1.0, depthref + s*dot(p6, tex6.yz), scale, closest);
	//	sum += BlurWithDepth(tex7, 1.0, depthref + s*dot(p7, tex7.yz), scale, closest);
	"
		
	*do2
	"		
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
		else
		{
		}
			
		oCol.rgb = sum.rgb;
		
	//	oCol = texture2D(sampler_tex0, tc_mapping.xy);
		
	//	oCol.rgb = splat3(depthref - ConvertDepth2(texc)) * 4.25;
	//	oCol.rgb = splat3(depthref/256.0);
	//	oCol.rgb = texdepth.rgb;
	//	oCol.rgb = texc.rgb;
		oCol.a = depthref/256.0;
	"
}
