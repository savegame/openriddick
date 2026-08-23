/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Ocean surface / medium lighting
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:
	
					S(0->x)(e^(kx + l) * e^(mx+n) dx) = 
					e^(l+n) * (e^(x(k+m)) - 1) / ((k + m) * LN(e))

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
    *using isfrontfacing
}

*flags
{
	*lightscatter		1
	*frontside			2
	*backside			4
	*fog				8
	*fogcube			16
	*fogsky				32
}

*generate
{
	*gen frontside
	*gen backside
	*gen frontside+backside
	*gen lightscatter

	*permute fogcube+fogsky
	{
		*gen fog+frontside
		*gen fog+frontside+backside
		*gen fog+backside
	}
}

*param
{
    *env       eye
    *env       VPParam
    *env       VPConst
    *env       VPScale
    *env       DepthScale
    *env		V2L_Mat0
    *env		V2L_Mat1
    *env		V2L_Mat2
    *env		V2L_Mat3
    *env		vtessbase
    *env		vtessdiru
    *env		vtessdirv
    *env		vtess
    *env		vtesspersp
    *env		suntheta
    *env		sunvec
    *env		skybaselight
    *env		waterparams
    *env		watercolor
    *env		L2V_Mat2
    *env		atmrange
    *env		FoamParams
    *env		EnvScale

	*if_fog
	{
		*env		FogInterval
		*env		FogColor
		*env		FogCubeColor
		*env		FogDitherUVScale
	}
}

*texture
{
	*texCube_0		texture_env
    *tex2D_1		texture_screen
    *tex2D_2		texture_depth
    *tex2D_3		texture_foam01
	*tex3D_4		texture_atm
	*tex2D_13		texture_normal
	*if_fog
	{
		*tex2D_14		Sampler_FogSky
		*texCube_15		Sampler_FogCube
	}
}

*attrib
{
    *texcoord2	tc0
    *texcoord3  tc_screen
    *texcoord4  pixelinfo_v_view
//    *texcoord5  pixelinfo_n
}

*output
{
    *color      oCol
}

/*
	float EvalScatter(float3 _v, float3 _p, float _eyedist, float _density, float _zmax)
	{
		float dz = max(0, _zmax - _p.z);
		float attn = exp(-_density * (dz + _eyedist));
		float cosphi = _v.z;
		float miescatter = (2.0 + cosphi) * (1.0 / 3.0);
		return attn * miescatter;
	}
	
	float EvalRayScatter(float3 _v, float3 _p, float _depth)
	{
		float zmax = 192.0;
		float density = waterparams.x;
		float3 vnrm = normalize(_v);
		_depth = min(1024, _depth);
		float dx = _depth / 4.0;
		float Scatter = density * dx * (
			EvalScatter(vnrm, _p + vnrm*float3(dx*0.5), dx*0.5, density, zmax) +
			EvalScatter(vnrm, _p + vnrm*float3(dx*0.5), dx*1.5, density, zmax) +
			EvalScatter(vnrm, _p + vnrm*float3(dx*0.5), dx*2.5, density, zmax) +
			EvalScatter(vnrm, _p + vnrm*float3(dx*0.5), dx*3.5, density, zmax));
		return Scatter;
	}
*/

*source
{
        *INCLUDE "XR_FPDepth.fph"
	*INCLUDE "XR_FPUtil.fph"
	
	*if_fog
	{
		*if_fogcube
		{
			*if_fogsky
			"
				float4 Fog_GetColor(float3 _pixelvec_w, float2 _tcscreen)
				{
					float dist = length(_pixelvec_w);
					float4 cube = textureCube(Sampler_FogCube, _pixelvec_w) * FogCubeColor;
					float4 sky = texture2D(Sampler_FogSky, _tcscreen);
					float skyblend = saturate(dist * FogInterval.w + FogInterval.z);
					float4 fogcolor;
					fogcolor.rgb = lerp(cube.rgb, sky.rgb, splat3(skyblend));
					fogcolor.a = saturate(dist * FogInterval.y + FogInterval.x);
					return fogcolor;
				}
			"
			*ifnot_fogsky
			"
				float4 Fog_GetColor(float3 _pixelvec_w, float2 _tcscreen)
				{
					float dist = length(_pixelvec_w);
					float4 cube = textureCube(Sampler_FogCube, _pixelvec_w) * FogCubeColor;
					float4 fogcolor;
					fogcolor.rgb = cube.rgb;
			//		fogcolor.rgb = float3(0.0, 1.0, 0.0);
					fogcolor.a = saturate(dist * FogInterval.y + FogInterval.x);
					return fogcolor;
				}
			"
		}
		*ifnot_fogcube
		{
			*if_fogsky
			"
				float4 Fog_GetColor(float3 _pixelvec_w, float2 _tcscreen)
				{
					float dist = length(_pixelvec_w);
					float4 sky = texture2D(Sampler_FogSky, _tcscreen);
					float skyblend = saturate(dist * FogInterval.w + FogInterval.z);
					float4 fogcolor;
					fogcolor.rgb = lerp(FogColor.rgb, sky.rgb, splat3(skyblend));
					fogcolor.a = saturate(dist * FogInterval.y + FogInterval.x);
					return fogcolor;
				}
			"
			*ifnot_fogsky
			"
				float4 Fog_GetColor(float3 _pixelvec_w, float2 _tcscreen)
				{
					float dist = length(_pixelvec_w);
					float4 fogcolor;
					fogcolor.rgb = FogColor.rgb;
					fogcolor.a = saturate(dist * FogInterval.y + FogInterval.x);
					return fogcolor;
				}
			"
		}

		*do
		"
			float4 FogLowLevel(float3 _pixelvec_w, float2 _tcscreen)
			{
				return Fog_GetColor(_pixelvec_w, _tcscreen);
			}

		/*	float4 Fog()
			{
				return Fog_GetColor(FogAttr0.xyz, FogAttr1.xy * splat2(1.0 / FogAttr1.w));
			}

			float3 FogOpaque(float3 _color)
			{
				float4 fogresult = Fog();
				return lerp(_color, fogresult.rgb, splat3(fogresult.a));
			}
		*/
		"
	}

	*ifnot_fog
	{
		*do
		"
			float4 FogLowLevel(float3 _pixelvec_w, float2 _tcscreen)
			{
				return float4(1.0, 1.0, 1.0, 0.0);
			}
			
			float4 Fog()
			{
				return float4(1.0, 1.0, 1.0, 0.0);
			}
			
			float3 FogOpaque(float3 _color)
			{
				return _color;
			}
		"
	}
	
	*doeet
	"
		float3 CalcWaveOriginalVertexPos(float2 _uv)
		{
//			float vpersp = (1.0 / lerp(_uv.y, 1.0, 8.0) - 0.125) * 1.142857142;	// Perspective
			float vpersp = (1.0 / lerp(vtesspersp.x, vtesspersp.y, _uv.y)) * vtesspersp.z + vtesspersp.w;
			float extx = (_uv.x * 2.0 - 1.0) * lerp(vtess.x, vtess.y, vpersp);
			float exty = vpersp * vtess.z;
			
			float3 pos = (vtessdiru.xyz * splat3(extx)) + (vtessdirv.xyz * splat3(exty)) + vtessbase.xyz;
			return pos;
		}
	    
		float2 RefractTexcoord(float2 _uv, float _amount, float3 _n)
		{
			float3 nproj = float3(_n.x, _n.y, 0.0);
			float2 tc_refr = _uv + splat2(_amount) * float2(dot(nproj, vtessdiru.xyz), dot(nproj, -vtessdirv.xyz));
			return tc_refr;
		}

		float4 ApplyFoam(float4 _LightIn, float _Depth, float4 _FoamTexel, float3 _n)
		{
			float k = saturate(_Depth * 0.1) * saturate(-_Depth * FoamParams.x + FoamParams.y);
			k = saturate(k + saturate(-_n.z * FoamParams.z + FoamParams.w));
			k = k * _FoamTexel.a;
			float4 LightOut = lerp(_LightIn, skybaselight, splat4(k));
			return LightOut;
		}

		float AttenuatedLightIntegral(float x, float k, float l, float m, float n)
		{
			//	S(0->x)(e^(kx + l) * e^(mx+n) dx) = 
			//	e^(l+n) * (e^(x(k+m)) - 1) / (k + m)
			float kplusm = k + m;
		//	return (abs(kplusm) > 0.001) ? (exp(l + n) * (exp(x * kplusm) - 1.0) / kplusm) : 0.0;
			return exp(l + n) * (exp(x * kplusm) - 1.0) / kplusm;
		}
		
		float4 VolumeLight(float3 _v, float3 _p, float _depth)
		{
	//	return splat4(0.0);
	//		float3 vnrm = SafeNormalize(_v, 0.00001);
			float3 vnrm = normalize(_v);
			float LightAttn = waterparams.x;
			float Light = AttenuatedLightIntegral(_depth, -LightAttn, 0.0, vnrm.z * LightAttn, -LightAttn * (384.0 - _p.z));
			float MieScatter = (2.0 + vnrm.z) * (1.0 / 3.0);
			Light = Light * LightAttn * MieScatter;

			return splat4(Light) * skybaselight * watercolor; // float4(0.8, 1.0, 0.9, 1);
	//		return splat4(Light) * skybaselight * float4(1.0, 0.0, 0.0, 1);
		}
	"
	
	*if_backside
	{
		*if_frontside
		"
	//		float WaterIsFrontFacing() { return float(IsFrontFacing); }
			bool WaterIsFrontFacing() { return IsFrontFacing; }
		"
		*ifnot_frontside
		"
			bool WaterIsFrontFacing() { return false; }
		"
	}
	*ifnot_backside
	{
		*if_frontside
		"
			bool WaterIsFrontFacing() { return true; }
		"
	}
}

*main
{
	*ifnot_lightscatter
	"
		float4 tmp = pixelinfo_v_view;
		tmp.w = 1.0;
		float3 pixelinfo_v = float3(dot(tmp, V2L_Mat0), dot(tmp, V2L_Mat1), dot(tmp, V2L_Mat2));
		float3 v_eye = eye.xyz - pixelinfo_v.xyz;
		float3 v_eye_n = normalize(v_eye);

                float4 TexNrm = SampleFPTexLinear2D(texture_normal,tc0.xy,waterparams.yz);
		float3 n = normalize(TexNrm.xyz - splat3(0.5));
//		n = float3(0.0, 0.0, 1.0);
		float3 rindex;

		float3 FresnelConst0;
		float3 FresnelConst1;

		if (WaterIsFrontFacing())
		{
			FresnelConst0 = float3(0.746487, 0.233151, 0.020363); // FresnelApprox_AirWater_0;
			FresnelConst1 = float3(7.0, 2.8, 1.0); // FresnelApprox_AirWater_1;
			rindex = float3(1.000293, 1.33333, 0.562832487);
		}
		else
		{
//			FresnelConst0 = FresnelApprox_WaterAir_0;
//			FresnelConst1 = FresnelApprox_WaterAir_1;
			FresnelConst0 = float3(0.792980, 0.185204, 0.020363);
			FresnelConst1 = float3(33.4, 4.2, 2.951257);
			rindex = float3(1.33333, 1.000293, 1.7767275);
			n = -n;
		}

		float2 tc_screen_p = tc_screen.xy * splat2(1.0 / tc_screen.w);
		float2 tc_refract = RefractTexcoord(tc_screen_p, -0.04, n);
		
		float3 orgpos = CalcWaveOriginalVertexPos(tc0.xy);

		float4 tex_foam01 = texture2D(texture_foam01, orgpos.xy * splat2(0.002));
		float4 tex_depth = texture2D(texture_depth, tc_screen_p);
		float4 tex_depthrefr = texture2D(texture_depth, tc_refract);
		float depth = ConvertDepth(tex_depth, VPConst);
		float depthrefr = ConvertDepth(tex_depthrefr, VPConst);
//depthrefr = depth;
		float4 vpos = ConvertDepthUVToPos(depth, tc_screen_p, DepthScale);
		float4 vposrefr = ConvertDepthUVToPos(depthrefr, tc_screen_p, DepthScale);
		float3 mpos = float3(dot(vpos, V2L_Mat0), dot(vpos, V2L_Mat1), dot(vpos, V2L_Mat2));
		float3 mposrefr = float3(dot(vposrefr, V2L_Mat0), dot(vposrefr, V2L_Mat1), dot(vposrefr, V2L_Mat2));
//		float3 mpos = mposrefr;
		float4 tex_refract;

		// This removes foreground refraction but the compiler seems to fuck it up
		if (lengthsqr(mposrefr - eye.xyz) < lengthsqr(eye.xyz - pixelinfo_v.xyz))
		{
			mposrefr = mpos; //pixelinfo_v.xyz;
			vposrefr = vpos;
			tc_refract = tc_screen_p;
		}
		
		tex_refract = texture2D(texture_screen, tc_refract);
		
		float3 fogvec;
		float3 fogstart;
		if (WaterIsFrontFacing())
		{
			fogvec = mposrefr - pixelinfo_v.xyz;
			fogstart = pixelinfo_v.xyz;
		}
		else
		{
			fogvec = pixelinfo_v.xyz - eye.xyz;
			fogstart = eye.xyz;
		}
		float fogveclen = length(fogvec);


		float4 final = tex_refract;
		
		float fog = saturate(exp(-fogveclen*waterparams.x));
		if (WaterIsFrontFacing())
		{
			final = lerp(splat4(0.0), final, splat4(fog));
			float4 Tmp = VolumeLight(fogvec, fogstart, fogveclen);
                //        if( Tmp.g == Tmp.g )
			final = final + Tmp;
			float foambgdistance = max(0.6, abs(v_eye_n.z))*length(mposrefr - pixelinfo_v.xyz);
			final = ApplyFoam(final, foambgdistance, tex_foam01, n);
		}
	//final = splat4(fog);

		float cosi = saturate(dot(v_eye_n, n));
		float3 r = splat3(cosi * 2.0) * n - v_eye_n;
		
		float fres = 1.0;
		float4 tex_env;
		
		if (WaterIsFrontFacing())
		{
	//		r.z = abs(r.z);			
			tex_env = textureCube(texture_env, r);
	/*		float envdepth = (tex_env.a * 256.0);			
			float3 renv = (envdepth + dot(v_eye, r))*r - v_eye;
			tex_env = textureCube(texture_env, renv);
			envdepth = (tex_env.a * 256.0);			
			renv = (envdepth + dot(v_eye, r))*r - v_eye;
			tex_env = textureCube(texture_env, renv);*/
			tex_env *= EnvScale;
		}
		else
			tex_env = VolumeLight(r, pixelinfo_v.xyz, 512.0);
		
/*		float cos_t2 = SnellsLaw(cosi, rindex.z);
		if (cos_t2 > 0.0)
		{
			float cos_t = sqrt(cos_t2);
			fres = saturate(Fresnel(cosi, cos_t, rindex.x, rindex.y));
		}
*/

		fres = FresnelApprox(cosi, FresnelConst0, FresnelConst1);

//		final.rgb = splat3(fres);
		final.rgb = lerp(final.rgb, tex_env.rgb, splat3(fres));

		if (!WaterIsFrontFacing())
		{
			final = lerp(splat4(0.0), final, splat4(fog));
			final = final + VolumeLight(fogvec, fogstart, fogveclen);
		}

//		final.xyz = splat3(0.0) + (mposrefr - pixelinfo_v.xyz) * splat3(0.01);
//		final.xyz = splat3(fogveclen * 0.01);
//		final.xyz = splat3(0.5) + (mposrefr.xyz) * splat3(0.001);
		
//		float pixeldepth = dot(pixelinfo_v.xyz - eye.xyz, L2V_Mat2.xyz);
//		float4 atm = texture3D(texture_atm, float3(tc_screen_p.x, tc_screen_p.y, pixeldepth * atmrange.y));		

//		final.rgb = saturate(pixelinfo_v_view.xyz * splat3(0.001) + float3(0.5, 0.5, 0.0));
//		final.xyz = float3(dot(pixelinfo_v_view.xyz, V2L_Mat0.xyz), dot(pixelinfo_v_view.xyz, V2L_Mat1.xyz), dot(pixelinfo_v_view.xyz, V2L_Mat2.xyz)) * splat3(0.001);
/*		float4 bla = pixelinfo_v_view;
		bla.w = 1.0;
		final.xyz = float3(dot(bla, V2L_Mat0), dot(bla, V2L_Mat1), dot(bla, V2L_Mat2)) * splat3(0.01);
		final.xy = splat2(0.0);
		final.xyz = r.xyz * splat3(0.5) + splat3(0.5);
*/
//		final.xyz = splat3(max(1.0, abs(v_eye_n.z))*length(mpos - pixelinfo_v.xyz) * splat3(0.01));

//		if (final.rgb != final.rgb)
		if (final.r != final.r)
			final.rgb = splat3(0.0);

		if (WaterIsFrontFacing())
		{
			float3 dv = vpos.xyz - pixelinfo_v_view.xyz;
			float distsqr = sqrt(dot(dv, dv));
			float blend = saturate(distsqr * 0.0625);
			final.rgb = lerp(tex_refract.rgb, final.rgb, splat3(blend));
			
			float4 fogresult = FogLowLevel(pixelinfo_v.xyz - eye.xyz, tc_screen_p);
			final.rgb = lerp(final.rgb, fogresult.rgb, splat3(fogresult.a));
		}
		oCol = final;
	"

	*if_lightscatter
	"
		float2 tc_screen_p = tc_screen.xy * splat2(1.0 / tc_screen.w);
		float4 tex_depth = texture2D(texture_depth, tc_screen_p);
		float depth = ConvertDepth(tex_depth, VPConst);
		float4 vpos = ConvertDepthUVToPos(depth, tc_screen_p, VPScale);
		float3 mpos = float3(dot(vpos, V2L_Mat0), dot(vpos, V2L_Mat1), dot(vpos, V2L_Mat2));
		float4 tex_bg = texture2D(texture_screen, tc_screen_p);
		
		float3 fogvec = mpos - eye.xyz;
		float3 fogstart = eye.xyz;
		float fogveclen = length(fogvec);
		float fog = saturate(pow(2.717, -fogveclen*waterparams.x));
		
		float4 final = tex_bg;
		final = lerp(splat4(0.0), final, splat4(fog));
		final = final + VolumeLight(fogvec, fogstart, fogveclen);

		oCol = final;
	"
}
