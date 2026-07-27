/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Screen space ambient occlusion
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2008, 2009
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*quality0	1
	*quality1	2
	*quality2	4
	*normalmap	8
}

*generate
{
	*permute normalmap
	{
		*gen quality0
		*gen quality1
		*gen quality2
	}
}

*param
{
	*envx		e_dUV			// dU, dV, 1/dU, 1/dV
	*envx		e_GrainScale
    *envx       e_VPDepthScale
    *envx       e_VPParam
    *envx       e_VPConst
    *envx       e_VPScale
    *envx		e_Radius
    *envx		e_Attenuation

    *if_normalmap
    {
		*envx	e_W2VMat0
		*envx	e_W2VMat1
		*envx	e_W2VMat2
		*envx	e_W2VMat3
    }
}

*texture
{
	*tex2D_0		sampler_depthmipmapped
	*tex2D_1		sampler_grain
	*tex2D_2		sampler_depth
	*tex2D_3		sampler_normalmap
}

*attrib
{
//    *color		a_Col
    *texcoord0	a_tc_screen
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
		float g_maxr;
		float g_maxrrcp;
		float g_maxr2;
		
		bool bUseDepthMipMap = false;
		
		float ConvertDepth2(float4 _tex, float4 _VPConst)
		{
			return _tex.r * 256.0;
		}

		float3 SampleViewPos(float2 _uv)
		{
			float depth = ConvertDepth(texture2D(sampler_depth, _uv), e_VPConst);
			float3 posv = ConvertDepthUVToPos(depth, _uv, e_VPDepthScale).xyz;
			return posv;
		}

		float3 SampleViewPosMipMap(float2 _uv, float _mip)
		{
			float depth = ConvertDepth2(texture2DLod(sampler_depthmipmapped, _uv, _mip), e_VPConst);
			float3 posv = ConvertDepthUVToPos(depth, _uv, e_VPDepthScale).xyz;
			return posv;
		}

		float2 SnapUV(float2 _uv)
		{
			return _uv - (frac(float2(_uv.x * e_dUV.z, _uv.y * e_dUV.w)) - splat2(0.5)) * e_dUV.xy;
		}

		float2 SnapdUV(float2 _duv)
		{
			return round(float2(_duv.x * e_dUV.z, _duv.y * e_dUV.w)) * e_dUV.xy;
		}
		
		float3 MinMag(float3 _v0, float3 _v1)
		{
			return dot(_v0, _v0) < dot(_v1, _v1) ? _v0 : _v1;
		}

		float Tangent(float3 _v)
		{
			return -_v.z / length(_v.xy);
		}

		float Tangent(float3 _p0, float3 _p1)
		{
			return Tangent(_p1 - _p0);
		}

		float StepRay(float3 _dposdx, float3 _dposdy, float3 _pos, float3 _n, float2 _uv, float2 _duv, float _nSteps, float _StepOffset)
		{
			float3 vray = _dposdx*_duv.x + _dposdy*_duv.y;
			float horiz_tan = Tangent(vray);
			float horiz_sin = horiz_tan * rsqrt(1.0 + sqr(horiz_tan));

			float ao = 0.0;
			for(float iStep = 1.0; iStep <= _nSteps; iStep += 1.0)
			{
				float k = pow((iStep + _StepOffset) / _nSteps, e_Attenuation.z) * _nSteps;
				float2 uv = SnapUV(_uv + _duv * k);

				float3 sample_pos;
				if (bUseDepthMipMap)
				{
					float lod = min(3.0, iStep-1.0);
					sample_pos = SampleViewPosMipMap(uv, lod);
				}
				else
					sample_pos = SampleViewPos(uv);
		        
				float3 v = sample_pos - _pos;
				float r2  = lengthsqr(v);
				if (r2 < g_maxr2) 
				{
					float sample_tan = Tangent(_pos, sample_pos);
					if(sample_tan > horiz_tan) 
					{
						float sample_sin = sample_tan * rsqrt(1.0 + sqr(sample_tan));
						float rnorm = sqrt(r2) * g_maxrrcp;
						v *= rsqrt(r2);
						ao += pow(1.0 - rnorm, e_Attenuation.x) * (sample_sin - horiz_sin) * saturate(dot(v, _n));
						horiz_tan = sample_tan;
						horiz_sin = sample_sin;
					}
				} 
			}

			return ao;
		}

	"
}

*main
{
	*if_quality0
	"
		float nRays = 10;
		float nSteps = 5;
	"
	*if_quality1
	"
		float nRays = 10;
		float nSteps = 5;
	"
	*if_quality2
	"
		float nRays = 12;
		float nSteps = 6;
	"
		
	*doeeet
	"	
		float2 tc_screen = SnapUV(a_tc_screen.xy);
	
		float2 tc_grain = tc_screen.xy * e_GrainScale.xy + e_GrainScale.zw;
		float tex_grain = texture2D(sampler_grain, tc_grain.xy).r;
		float rot = frac(tex_grain * 7163.127) * 2.0 * 3.1415;
		
		float depth = ConvertDepth(texture2D(sampler_depth, tc_screen.xy), e_VPConst);
		float3 posv = ConvertDepthUVToPos(depth, tc_screen.xy, e_VPDepthScale).xyz;
			
		float3 n = float3(0.0, 0.0, -1.0);
		
		float3 dposdx, dposdy;
	"
	*ifnot_normalmap
	"
		{
			float3 p0 = SampleViewPos(tc_screen.xy + float2(e_dUV.x, 0.0));
			float3 p1 = SampleViewPos(tc_screen.xy + float2(0.0, e_dUV.y));
			float3 p2 = SampleViewPos(tc_screen.xy + float2(-e_dUV.x, 0.0));
			float3 p3 = SampleViewPos(tc_screen.xy + float2(0.0, -e_dUV.y));
			dposdx = MinMag(p0 - posv, posv - p2);
			dposdy = MinMag(p1 - posv, posv - p3);
			
	//		const float ddepthclamp = 16.0;
	//		dposdx.z = clamp(dposdx.z, -ddepthclamp, ddepthclamp);
	//		dposdy.z = clamp(dposdy.z, -ddepthclamp, ddepthclamp);
			n = cross(dposdx, dposdy);
		}
	"
	*if_normalmap
	"
			float4 texnormalmap = texture2D(sampler_normalmap, tc_screen);
			float3 nw = (texnormalmap.rgb - splat3(0.5)) * 2.0;
			n = e_W2VMat0.rgb * splat3(nw.x) +
				e_W2VMat1.rgb * splat3(nw.y) +
				e_W2VMat2.rgb * splat3(nw.z);
			{
				float2 dpos = (-e_dUV.xy * e_VPDepthScale.zw + e_VPDepthScale.xy*0.0) * splat2(depth);
				float3 y0 = ConvertDepthUVToPos(depth - n.y*dpos.y, tc_screen.xy + float2(0.0, e_dUV.y), e_VPDepthScale).xyz;
				float3 y1 = ConvertDepthUVToPos(depth + n.y*dpos.y, tc_screen.xy + float2(0.0, -e_dUV.y), e_VPDepthScale).xyz;
				float3 x0 = ConvertDepthUVToPos(depth + n.x*dpos.y, tc_screen.xy + float2(e_dUV.x, 0.0), e_VPDepthScale).xyz;
				float3 x1 = ConvertDepthUVToPos(depth - n.x*dpos.y, tc_screen.xy + float2(-e_dUV.x, 0.0), e_VPDepthScale).xyz;
				dposdx = MinMag(x0 - posv, posv - x1);
				dposdy = MinMag(y0 - posv, posv - y1);
			}
	"

	*do2
	"
		n = normalize(n);

/*		{
			float3 tang_n = n;
			tang_n.z = min(-0.01, tang_n.z);
			float3 tang_u = cross(tang_n, float3(1, 0, 0));
			float3 tang_v = cross(tang_u, tang_n);
			tang_n = normalize(tang_n);
			tang_u = normalize(tang_u);
			tang_v = normalize(tang_v);	
		}
*/

		float ao = 0.0;
		g_maxr = lerp(e_Radius.x, e_Radius.y, saturate(depth*e_Radius.z));
		g_maxr2 = g_maxr*g_maxr;
		g_maxrrcp = 1.0 / g_maxr;
		float r2d = 0.25 * g_maxr * e_VPScale.x / posv.z;

		float step = r2d / nSteps;
		float rotbase = tex_grain * 31.15627;

		for(float r = 0; r < nRays; r += 1.0)
		{
			float rot = r * (1.0 / nRays) * 2.0 * 3.1415 + rotbase;
			float2 duv = e_dUV.xy * (float2(cos(rot), sin(rot)) * step);
			float rayao = StepRay(dposdx, dposdy, posv, n, tc_screen.xy, duv, nSteps, frac(tex_grain*17.0));
			ao += rayao;
		}

		ao *= 1.0 / nRays;
		ao = lerp(ao, 0.0, sqr(saturate(depth*e_Radius.w)));
		ao = saturate(1.0 - ao);
		ao = pow(ao, e_Attenuation.y);

		oCol = float4(ao, dposdx.z, dposdy.z, depth / 256.0);

//		oCol.rgb = splat3(depth16/1024.0);
//		oCol.rgb = tang_n * 0.5 + splat3(0.5);
//		oCol.rgb = dposdy * splat3(0.5) + splat3(0.5);
//		oCol.rgb = splat3(dposdx.x) * 0.5 + splat3(0.5);
//		oCol.rgb = splat3(depth / 512.0);

	"
}
