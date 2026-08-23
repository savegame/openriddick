/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			BRDF model 3.0 - One shader to rule them all, zomg!1!

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2008

	Comments:
		- Surface blending use deferred rendering
			Map0: N.x, N.y, N.z, SurfN.x				Normals are in worldspace
			Map1: D.r, D.g, D.b, SurfN.y
			Map2. AO, Fresnel, lg2(SpecI)/16, SurfN.z		

			Anisotropic shading not supported.
			Colored specular not supported.
			Shading by quad/lightvolume possible.
			Proper decals possible.

		- Simple fresnel for dielectrics
		- One table fresnel per surface [TBD]
		- 
		
		- e_w, Eye vector points from the pixel being shaded towards the eye
		- l_w, Light vector points from the pixel being shaded towards the light


\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*anisotrophic		1
	*materialmask		2
	*environmentmap		4
	*macronormalmap		8
	*light0				16
	*light1				32
	*light2				64
	*projmap0			128
	*projmap1			256
	*projmap2			512
	*detailnormalmap	1024
	*deferredalphaonly	0x00000800
	*lightfield			8192
	*lightfieldmapping	16384
	*deferredin			32768
	*deferredout		0x00010000
	*deferredout_n		0x00020000
	*deferredout_d		0x00040000
	*deferredout_s		0x00080000
	*deferredmrt		0x00100000
	*fixedtangentspace	0x00200000
	*deferredfromdepth	0x00400000
	*deferredalphamap	0x00800000
}

*generate
{
	*gen deferredin + deferredfromdepth + light0
	*gen deferredin + deferredfromdepth + light0 + projmap0
	
	*gen macronormalmap + fixedtangentspace + deferredout + deferredmrt + deferredalphaonly
	*gen macronormalmap + fixedtangentspace + deferredout + deferredout_n + deferredalphaonly
	*gen macronormalmap + fixedtangentspace + deferredout + deferredout_d + deferredalphaonly
	*gen macronormalmap + fixedtangentspace + deferredout + deferredout_s + deferredalphaonly
	
	// 128 combinations
	*permute environmentmap + fixedtangentspace + deferredalphamap + macronormalmap + detailnormalmap /* + materialmask  */
	{
		*gen deferredout + deferredout_n
		*gen deferredout + deferredout_d
		*gen deferredout + deferredout_s
		*gen deferredout + deferredmrt
	}

	// 15 * 8 = 120 combinations
	*permute anisotrophic + detailnormalmap + environmentmap /* + materialmask */
	{
		*gen lightfield
		*gen lightfieldmapping
		*gen deferredin + lightfield
		*gen deferredin + lightfieldmapping
		*gen deferredin + light0
		*gen deferredin + light0 + projmap0

		*permute 0 // lightfield
		{
			*gen light0
			*gen light0 + projmap0		
			*gen light0 + light1
			*gen light0 + light1 + projmap0
			*gen light0 + light1 + projmap0 + projmap1
			*gen light0 + light1 + light2
			*gen light0 + light1 + light2 + projmap0
			*gen light0 + light1 + light2 + projmap0 + projmap1
			*gen light0 + light1 + light2 + projmap0 + projmap1 + projmap2
		}
					
/*		*permute projmap0
		{ 
			*gen light0
			{
				*permute projmap1
				{ 
					*gen light1
					{
						*permute projmap2
						{ 
							*gen light2
						}
					}
				}
			}
		}*/
	}
}

*param
{
	*ifnot_deferredout
	{
		*envX		EyePos_w		// { x, y, z, 0 }
		
	}

	*ifnot_deferredin
	{
		*envX		DiffuseColorR	// { r0, r1, r2, 0 } or { r, g, b, 0 }
		*if_materialmask
		{
			*envX		DiffuseColorG	// { g0, g1, g2, 0 }
			*envX		DiffuseColorB	// { b0, b1, b2, 0 }
		}
		*envX		SpecularColorE	// { e0, e1, e2, 0 } or { r, g, b, e }
		*if_materialmask
		{
			*envX		SpecularColorR	// { r0, r1, r2, 0 }
			*envX		SpecularColorG	// { g0, g1, g2, 0 }
			*envX		SpecularColorB	// { b0, b1, b2, 0 }
		}
		
		*envX		FresnelK	// { k0, k1, k2, 0 } or { r, g, b, k }
		*if_materialmask
		{
			*envX		FresnelR	// { r0, r1, r2, 0 }
			*envX		FresnelG	// { g0, g1, g2, 0 }
			*envX		FresnelB	// { b0, b1, b2, 0 }
		}

		*if_aniso
		{
			*envX		Anisotrophy		// { a0, a1, a2, 0 }
		}
		*if_detailnormalmap
		{
			*envX	NormalDetailScale0
	/*		*if_materialmask
			{
				*envX	NormalDetailScale1
				*envX	NormalDetailScale2
			} */
		}
		*if_environmentmap
		{
			*envX	EnvColor
		}
	}
	*if_deferredfromdepth
	{
	    *envX		VPParam
		*envX		VPConst
		*envX		VPScale
		*envX           DepthScale
		*envX		V2WMat_0
		*envX		V2WMat_1
		*envX		V2WMat_2
	}
	*if_light0
	{
		*envX		LightPos0_w		// { x, y, z, 1.0 / Range^2 }
		*envX		LightIntensity0	// { r, g, b, 0 }
	}
	*if_light1
	{
		*envX		LightPos1_w		// { x, y, z, 1.0 / Range^2 }
		*envX		LightIntensity1	// { r, g, b, 0 }
	}
	*if_light2
	{
		*envX		LightPos2_w		// { x, y, z, 1.0 / Range^2 }
		*envX		LightIntensity2	// { r, g, b, 0 }
	}
	*if_lightfield
	{
		*envX		LF_Axis0
		*envX		LF_Axis1
		*envX		LF_Axis2
		*envX		LF_Axis3
		*envX		LF_Axis4
		*envX		LF_Axis5
	}
	*if_lightfieldmapping
	{
		*envX		LFM_Scale
	}
	*if_fixedtangentspace
	{
		*envX		e_TS2W_Mat_0
		*envX		e_TS2W_Mat_1
		*envX		e_TS2W_Mat_2
		*if_macronormalmap
		{
			*envX		e_MacroTS2TS_Mat_0
			*envX		e_MacroTS2TS_Mat_1
		}
	}
	*if_deferredalphamap
	{
		*envX		AlphaMapParams
	}
}

*texture
{
	*tex2D_0	sampler_Diffuse0
	*tex2D_1	sampler_MaterialSpecular0
	*tex2D_2	sampler_Normal0
	*tex2D_3	sampler_AnisotropicVec
	
	*if_detailnormalmap
	{
		*tex2D_4	sampler_NormalDetail0
		*if_materialmask
		{
			*tex2D_5	sampler_NormalDetail1
			*tex2D_6	sampler_NormalDetail2
		}
	}
	
	*if_macronormalmap
	{
		*tex2D_7	sampler_NormalMacro
	}

	*texCube_8	sampler_Env0
	*texCube_9	sampler_Env1

	*if_deferredfromdepth
	{
		*tex2D_10	sampler_Depth;
	}
    *if_deferredalphamap
    {
		*tex2D_11	sampler_AlphaMap
	}
		
	*if_projmap0 { *texCube_12	sampler_ProjMap0 }
	*if_projmap1 { *texCube_13	sampler_ProjMap1 }
	*if_projmap2 { *texCube_14	sampler_ProjMap2 }
	
	*if_lightfieldmapping
	{
		*tex2D_12	sampler_LFM0;
		*tex2D_13	sampler_LFM1;
		*tex2D_14	sampler_LFM2;
		*tex2D_15	sampler_LFM3;
	}
	
/*	*tex2D_12	sampler_ShadowMask0;
	*tex2D_13	sampler_ShadowMask1;
	*tex2D_14	sampler_ShadowMask2; */
}

*attrib
{
    *texcoord0	a_TCMapping

    *if_fixedtangentspace
    {
	    *texcoord1	aa_PixelPosition_ws
	}
    *ifnot_fixedtangentspace
    {
		*texcoord1	aa_TS2W_Mat_0
		*texcoord2	aa_TS2W_Mat_1
		*texcoord3	aa_TS2W_Mat_2
	}
	*if_deferredalphamap
	{
		*texcoord6	a_TCAlphaMap
	}
	*if_macronormalmap
	{
		*texcoord7	a_TCMappingMacro
	}
    *if_lightfieldmapping
    {
		*texcoord5	a_TCLFM
		*texcoord6	a_LMIntensityScale
    }
    *ifnot_lightfieldmapping
    {
		*if_projmap0 { *texcoord5	a_TCProjMap0 }
		*if_projmap1 { *texcoord6	a_TCProjMap1 }
		*if_projmap2 { *texcoord7	a_TCProjMap2 }
	}
}

*output
{
    *color      oCol0
    *if_deferredmrt
    {
	    *color1      oCol1
		*color2      oCol2
	}
}


*source
{
	*INCLUDE "XR_FPUtil.fph"
	*INCLUDE "XR_FPDepth.fph"
	
	*doeet
	"
		float Attenuation(float3 _pos, float3 _light, float _range)
		{
			float3 lv = _light - _pos;
			float lensqr = dot(lv, lv);
			float attn = sqr(1.0 - saturate(lensqr * _range));
			return attn;
		}

		float SelfShadow(float _cosi, float _k1, float _k2)
		{
			return saturate((_k1 + _cosi) * _k2);
		}
	"
	
	*dofres
	"
		float3 Fresnel(float _cosi, float4 _fresparams)	// Should be ~13 instructions
		{
			float fres = FresnelBaseDielectric(_cosi);
			float3 fresrgb = lerp(_fresparams.rgb, splat3(1.0), splat3(fres));
			fres = lerp(fres, 1.0, _fresparams.w);
			return fresrgb * splat3(fres);
		}
	"

	*misc
	"
		float4 a_TS2W_Mat_0;
		float4 a_TS2W_Mat_1;
		float4 a_TS2W_Mat_2;
		float3 a_PixelPosition_ws;
		
		float DeferredspecToSpec(float _s)
		{
			return exp2(_s * 16.0);
		}
		
		float SpecToDeferredspec(float _s)
		{
			return log2(max(_s, 0.00001)) * (1.0 / 16.0);
		}
		
		float3 TS_to_W(float3 _v)
		{
			return float3(dot(_v, a_TS2W_Mat_0.xyz), dot(_v, a_TS2W_Mat_1.xyz), dot(_v, a_TS2W_Mat_2.xyz));
		}

		float3 TS_to_W(float4 _v)
		{
			return float3(dot(_v, a_TS2W_Mat_0), dot(_v, a_TS2W_Mat_1), dot(_v, a_TS2W_Mat_2));
		}
		
		float AnisoWeight(float _cosi, float _anisotrophy)
		{
			return saturate(pow(_cosi, 0.125) * _anisotrophy);
		}

		float3 AnisoNormal(float3 _n, float3 _anisodir, float3 _h, float _weight)
		{
			float3 an = normalize(cross(_anisodir, _h));
			float3 nproj = _n - an * dot(an, _n);
			float3 nnew = normalize(lerp(nproj, _n, splat3(_weight)));
			return nnew;
		}
	"
	
	*lighting
	"
		float3 BRDF(float3 _n, float3 _nsurf, float3 _e, float3 _l, float4 _kdiff, float4 _kspec, float4 _fresparams, float3 _envmap)
		{
			// kdiff = r,g,b,junk
			// kspec = r,g,b,e
			
			_kspec.w = max(1.0, _kspec.w * _kspec.r);
		
			float3 h = normalize(_e + _l);
		
	//		float incidenceoffset = 0.05;
	//		float cosi = saturate((dot(_n, _l) - incidenceoffset) * (1.0 / (1.0 - incidenceoffset)));
			float incidenceoffset = 0.0;
			float cosi = saturate(dot(_n, _l));
			float3 fres = Fresnel(cosi, _fresparams) * saturate(_kspec.w * 0.125);
			float3 frescolor = lerp(_fresparams.rgb, splat3(1.0), fres);
			
	//		float specweight = max(max(fres.r, fres.g), fres.b) * _kspec.r;
			float specweight1 = (1.0 - rsqrt(_kspec.w));
			float specweight2 = max(max(fres.r, fres.g), fres.b);
			
			float specexp = _kspec.w;
			
		//	float speclobe = exp2(-1.717 * specexp * (1.0 - dot(_n, h)));
			float speclobe = pow(saturate(dot(_n, h)), specexp);
		//	float ispecular = (specexp * 0.76923 + 2.655) * speclobe * (0.5 / sqr(3.1415));
			float ispecular = (specexp + 3.4515) * speclobe * 0.038969686;
			float idiffuse = M_2PI_RCP;
			float3 diffuse = splat3(idiffuse) * _kdiff.rgb;
			float specular = ispecular / max(0.000001, dot(_e, h));// * _kspec.r;
			float cosisurf = dot(_nsurf, _l);

			float selfshadow = SelfShadow(cosisurf, -incidenceoffset, 8.0);
			return (_envmap*splat3(specweight1) + lerp(diffuse, frescolor * splat3(specular), splat3(specweight1*specweight2))) * splat3(cosi * selfshadow);
		}
		
	"
	*if_aniso
	"
		float3 BRDF(float3 _matweights, float3 _n, float3 _nsurf, float3 _e, float3 _l, float4 _kdiff, float4 _kspec, float4 _fresparams, float3 _anisodir, float3 _envmap)
		{
			float3 h = normalize(_e + _l);
			float3 n = AnisoNormal(_n, _anisodir, h, AnisoWeight(saturate(_n, _l), dot(Anisotrophy.xyz, _matweights)));
			return BRDF(n, _nsurf, _e, _l, _kdiff, _kspec, _fresparams, _envmap);
		}
		
		float4 GetAnsiotropyParam() { return Anisotropy; }
	"
	*ifnot_aniso
	"
		float3 BRDF(float3 _matweights, float3 _n, float3 _nsurf, float3 _e, float3 _l, float4 _kdiff, float4 _kspec, float4 _fresparams, float3 _anisodir, float3 _envmap)
		{
			return BRDF(_n, _nsurf, _e, _l, _kdiff, _kspec, _fresparams, _envmap);
		}
		
		float4 GetAnsiotropyParam() { return splat4(0.0); }
	"
}

*main
{
	*ifnot_deferredfromdepth
	{
		*ifnot_fixedtangentspace
		"
			a_PixelPosition_ws = float3(aa_TS2W_Mat_0.w, aa_TS2W_Mat_1.w, aa_TS2W_Mat_2.w);
		"
		*if_fixedtangentspace
		"
			a_PixelPosition_ws = aa_PixelPosition_ws.xyz;
		"
	}
	*if_deferredin
	{
		*do
		"
			float2 TCMapping = a_TCMapping.xy * splat2(1.0 / a_TCMapping.w);
		"
		*if_deferredfromdepth
		"
			float4 tex_depth = texture2D(sampler_Depth, TCMapping);
			float depth = ConvertDepth(tex_depth, VPConst);
			float4 vpos = ConvertDepthUVToPos(depth, TCMapping, DepthScale);
			a_PixelPosition_ws = float3(dot(vpos, V2WMat_0), dot(vpos, V2WMat_1), dot(vpos, V2WMat_2));
		"
	}
    *if_fixedtangentspace
    {
		*do
		"
			a_TS2W_Mat_0 = e_TS2W_Mat_0;
			a_TS2W_Mat_1 = e_TS2W_Mat_1;
			a_TS2W_Mat_2 = e_TS2W_Mat_2;
		"
    }
    
    *ifnot_fixedtangentspace
    "
		a_TS2W_Mat_0 = aa_TS2W_Mat_0;
		a_TS2W_Mat_1 = aa_TS2W_Mat_1;
		a_TS2W_Mat_2 = aa_TS2W_Mat_2;
    "

    // ------------------------------------- MATERIAL INPUT -------------------------------------
    
	*dovar
	"
		float4 FresParams = float4(1.0, 1.0, 1.0, 0.05);
		
	"
	
	*ifnot_deferredout
	"
		float3 e_w = normalize(EyePos_w.xyz - a_PixelPosition_ws.xyz);
	"

	*if_deferredin
	{
		*do
		"
			float4 tex_deferred_specular = texture2D(sampler_MaterialSpecular0, TCMapping.xy);
			float4 tex_deferred_diffuse = texture2D(sampler_Diffuse0, TCMapping.xy);
			float4 tex_deferred_normal = texture2D(sampler_Normal0, TCMapping.xy);

			FresParams.w = tex_deferred_specular.g;
			float4 SpecularColor;
			float4 DiffuseColor = splat4(1.0);
			SpecularColor.w = DeferredspecToSpec(tex_deferred_specular.b);
			SpecularColor.rgb = splat3(1.0);
			DiffuseColor.rgb = tex_deferred_diffuse.rgb;
			
			float3 n_w = tex_deferred_normal.rgb * splat3(2.0) - splat3(1.0);
			float3 nsurf_w = float3(tex_deferred_normal.w, tex_deferred_diffuse.w, tex_deferred_specular.w) * splat3(2.0) - splat3(1.0);
	//		nsurf_w = n_w;
			
			float3 matweights = splat3(0.0);
		"

		*if_lightfieldmapping
		"
			float3 n_ts = normalize(splat3(n_w.x) * a_TS2W_Mat_0.xyz +
				splat3(n_w.y) * a_TS2W_Mat_1.xyz +
				splat3(n_w.z) * a_TS2W_Mat_2.xyz);
		"
	}
	*ifnot_deferredin
	{
		*do
		"
			float3 nsurf_w = (float3(a_TS2W_Mat_0.x, a_TS2W_Mat_1.x, a_TS2W_Mat_2.x));
		"
		*if_materialmask
		{
			*do
			"
				float4 tex_materialspecular = texture2D(sampler_MaterialSpecular0, a_TCMapping.xy);
				float3 matweights = normalize(tex_materialspecular.xyz);
				
				float4 SpecularColor = float4(
					dot(SpecularColorR.xyz, matweights),
					dot(SpecularColorG.xyz, matweights),
					dot(SpecularColorB.xyz, matweights),
					dot(SpecularColorE.xyz, matweights));
				float4 DiffuseColor = float4(
					dot(DiffuseColorR.xyz, matweights),
					dot(DiffuseColorG.xyz, matweights),
					dot(DiffuseColorB.xyz, matweights),
					dot(GetAnsiotropyParam().xyz, matweights));
				SpecularColor.rgb = saturate(SpecularColor.rgb * splat3(tex_materialspecular.a));
				
				FresParams.w = dot(matweights, FresnelK.rgb);
				FresParams.rgb = float3(dot(matweights, FresnelR.rgb), dot(matweights, FresnelG.rgb), dot(matweights, FresnelB.rgb));
			"
		}

		*ifnot_materialmask
		{
			*do
			"
				float4 tex_materialspecular = texture2D(sampler_MaterialSpecular0, a_TCMapping.xy);
				float3 matweights = float3(1.0, 0.0, 0.0);
				
				float4 SpecularColor = SpecularColorE;
				float4 DiffuseColor = DiffuseColorR;
				SpecularColor.w = SpecularColor.w * tex_materialspecular.w;
				float stemp = dot(SpecularColor.rgb * tex_materialspecular.rgb, float3(0.25, 0.5, 0.25));
				SpecularColor.w *= stemp;
				SpecularColor.rgb = splat3(1.0);
				
				FresParams = FresnelK;
			"
		}

		*readnormal
		"
			float4 tex_normal = texture2D(sampler_Normal0, a_TCMapping.xy) - splat4(0.5);
		"	
		*if_macronormalmap
		{
			*if_fixedtangentspace
			"
				float3 normalmacro_ts2 = texture2D(sampler_NormalMacro, a_TCMappingMacro.xy).xyz * splat3(2.0) - splat3(1.0);
				float4 normalmacro_ts = float4(0.0, normalmacro_ts2.b, 0.0, normalmacro_ts2.g);
				tex_normal = tex_normal + normalmacro_ts * splat4(0.5);
				nsurf_w = normalize(TS_to_W(normalmacro_ts2));
			"
			*ifnot_fixedtangentspace
			"
				tex_normal = tex_normal + texture2D(sampler_NormalMacro, a_TCMappingMacro.xy) - splat4(0.5);
			"
		}

		*if_detailnormalmap
		{
			*if_materialmask
			"
				float4 tex_normaldetail0 = texture2D(sampler_NormalDetail0, a_TCMapping.xy * NormalDetailScale0.xy);
				float4 tex_normaldetail1 = texture2D(sampler_NormalDetail1, a_TCMapping.xy * NormalDetailScale0.xy);
				float4 tex_normaldetail2 = texture2D(sampler_NormalDetail2, a_TCMapping.xy * NormalDetailScale0.xy);
				tex_normaldetail0 = 
					tex_normaldetail0 * splat4(matweights.x) +
					tex_normaldetail1 * splat4(matweights.y) +
					tex_normaldetail2 * splat4(matweights.z);
				tex_normal = tex_normal + tex_normaldetail0 - splat4(0.5);
			"
			*ifnot_materialmask
			"
				tex_normal = tex_normal + splat4(0.5) * (texture2D(sampler_NormalDetail0, a_TCMapping.xy * NormalDetailScale0.xy) - splat4(0.5));
		//		float lenrcp = rsqrt(min(sqr(tex_normal.g) + sqr(tex_normal.a)), 0.25);
			//	tex_normal = tex_normal * splat4(lenrcp);
			"
			
		}
		
		*do1
		"
			float3 n_ts = ConvertNormalTexel_NoRemap(tex_normal * splat4(2.0));
			float4 tex_diffuse = texture2D(sampler_Diffuse0, a_TCMapping.xy);
			DiffuseColor = saturate(DiffuseColor * tex_diffuse);
			float3 n_w = normalize(TS_to_W(n_ts));
		"
		
	}
    // ------------------------------------- MATERIAL OUPUT -------------------------------------
	*if_deferredout
	{
		*do
		"
			float3 deferredalphaout = nsurf_w * splat3(0.5) + splat3(0.5);
			float3 n_out = saturate(n_w * splat3(0.5) + splat3(0.5));
			float spec_out = SpecToDeferredspec(SpecularColor.w);
			float ao_out = 0.0;
		"
		
		*if_deferredalphaonly
		{
			*if_deferredmrt
			"
				oCol0 = float4(0.0, 0.0, 0.0, deferredalphaout.x);
				oCol1 = float4(0.0, 0.0, 0.0, deferredalphaout.y);
				oCol2 = float4(0.0, 0.0, 0.0, deferredalphaout.z);
			"
			*ifnot_deferredmrt
			{
				*if_deferredout_n
				"
					oCol0 = float4(0.0, 0.0, 0.0, deferredalphaout.x);
				"
				*if_deferredout_d
				"
					oCol0 = float4(0.0, 0.0, 0.0, deferredalphaout.y);
				"
				*if_deferredout_s
				"
					oCol0 = float4(0.0, 0.0, 0.0, deferredalphaout.z);
				"
			}
		}
		*ifnot_deferredalphaonly
		{
			*if_deferredalphamap
			"
				float blendoffset = tex_diffuse.a;
				float blendoffsetrange = AlphaMapParams.x;
				float blend = texture2D(sampler_AlphaMap, a_TCAlphaMap.xy).r;
				blend = saturate(blend * (1.0 + blendoffsetrange) - blendoffsetrange + blendoffsetrange * blendoffset);
				deferredalphaout = splat3(blend);
			"

			*if_deferredmrt
			"
				oCol0 = float4(n_out.x, n_out.y, n_out.z, deferredalphaout.x);
				oCol1 = float4(DiffuseColor.r, DiffuseColor.g, DiffuseColor.b, deferredalphaout.y);
				oCol2 = float4(ao_out, FresParams.w, spec_out, deferredalphaout.z);
			"
			*ifnot_deferredmrt
			{
				*if_deferredout_n
				"
					oCol0 = float4(n_out.x, n_out.y, n_out.z, deferredalphaout.x);
				"
				*if_deferredout_d
				"
					oCol0 = float4(DiffuseColor.r, DiffuseColor.g, DiffuseColor.b, deferredalphaout.y);
				"
				*if_deferredout_s
				"
					oCol0 = float4(ao_out, FresParams.w, spec_out, deferredalphaout.z);
				"
			}
		}
	}
	
    // ------------------------------------- LIGHTING -------------------------------------
	*ifnot_deferredout
	{
		*if_light0
		"
			float3 l0_w = normalize(LightPos0_w.xyz - a_PixelPosition_ws.xyz);
			float3 attn0 = splat3(Attenuation(a_PixelPosition_ws.xyz, LightPos0_w.xyz, LightPos0_w.w)) * LightIntensity0.rgb;;
		"
		*if_light1
		"
			float3 l1_w = normalize(LightPos1_w.xyz - a_PixelPosition_ws.xyz);
			float3 attn1 = splat3(Attenuation(a_PixelPosition_ws.xyz, LightPos1_w.xyz, LightPos1_w.w)) * LightIntensity1.rgb;
		"
		*if_light2
		"
			float3 l2_w = normalize(LightPos2_w.xyz - a_PixelPosition_ws.xyz);
			float3 attn2 = splat3(Attenuation(a_PixelPosition_ws.xyz, LightPos2_w.xyz, LightPos2_w.w)) * LightIntensity2.rgb;
		"

		*doenvinit
		"
			float4 tex_envmap = splat4(0.0);
		"
		*ifnot_deferredin
		{
			*if_environmentmap
			{
				*do
				"
					{
						float3 r_w = reflect(-e_w, n_w);
						float cosi = saturate(dot(n_w, r_w));
						float specexp = max(1.0, SpecularColor.w * SpecularColor.r);
						float envlod = max(0.0, EnvColor.w - 1.0*log2(specexp) + cosi);
						tex_envmap = textureCubeLod(sampler_Env0, r_w.xyz, envlod);
						float3 fres = Fresnel(cosi, FresParams) * saturate(specexp * 0.125);
				//		tex_envmap.rgb *= fres;
						tex_envmap.rgb *= fres * EnvColor.rgb * splat3(0.5 * SelfShadow(dot(r_w, nsurf_w), 0.0, 8.0));
					}
				"
				*ifnot_lightmapping
				{
					*ifnot_lightfieldmapping
					"
				//		tex_envmap = splat4(0.0);
					"
				}
				*if_light0
				"
//					attn0 *= saturate(splat3(0.0) + tex_envmap.rgb * 2.0);
				"
				
				*do2
				"
//					tex_envmap.rgb = splat3(0.0);
				"

			}
		}
		
		*if_aniso
		{
			*do
			"
				float4 tex_anisodir = texture2D(sampler_AnisotropicVec, a_TCMapping.xy);
				tex_anisodir.xyz = normalize(tex_anisodir.xyz * splat3(2.0) - splat3(1.0));
				float3 anisodir_w = TS_to_W(tex_anisodir.xyz);
			"
		}
		*ifnot_aniso
		{
			*do
			"
				float3 anisodir_w = float3(1.0, 0.0, 0.0);
			"
		}

		*if_projmap0
		"
			float4 tex_proj0 = textureCube(sampler_ProjMap0, a_TCProjMap0.xyz);
			attn0 = attn0 * tex_proj0.rgb;
		"
		*if_projmap1
		"
			float4 tex_proj1 = textureCube(sampler_ProjMap1, a_TCProjMap1.xyz);
			attn1 = attn1 * tex_proj1.rgb;
		"
		*if_projmap2
		"
			float4 tex_proj2 = textureCube(sampler_ProjMap2, a_TCProjMap2.xyz);
			attn2 = attn2 * tex_proj2.rgb;
		"

		*dolight
		{
			*res
			"
				float4 result;
				result.rgb = splat3(0.0);
				result.a = DiffuseColor.a;
			"

			// -------------------------------------------------------------------
			*if_light0
			"
	//		float3 BRDF(float3 _matweights, float3 _n, float3 _e, float3 _l, float4 _kdiff, float4 _kspec, float4 _fresparams, float3 _tex_diff, float3 _tex_spec)
				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l0_w, DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * attn0;
			"
			*if_light1
			"
				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l1_w, DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * attn1;
			"
			*if_light2
			"
				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l2_w, DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * attn2;
			"

			// -------------------------------------------------------------------
			*if_lightfield
			"
				{
					float3 rgbweights = float3(1.0,2.0,1.0);
					float3 n_surf_w = normalize(float3(a_TS2W_Mat_0.x, a_TS2W_Mat_1.x, a_TS2W_Mat_2.x));
					
					float3 r_surf_w = reflect(-e_w, n_surf_w);
//					float3 ls_w = normalize(r_surf_w * splat3(1.0) + n_w);
//					float3 ls_w = r_surf_w;
					float3 ls_w = n_surf_w;
					float3 n_sat0 = saturate(-ls_w * splat3(0.5) + splat3(0.5));
					float3 n_sat1 = saturate(ls_w * splat3(0.5) + splat3(0.5));
					
	//				float3 n_sat0 = saturate(-n_surf_w * splat3(0.5) + splat3(0.5));
	//				float3 n_sat1 = saturate(n_surf_w * splat3(0.5) + splat3(0.5));
	//				float3 n_sat0 = saturate(-n_surf_w);
	//				float3 n_sat1 = saturate(n_surf_w);
	//				float3 r_sat0 = saturate(-r_surf_w) * splat3(0.5);
	//				float3 r_sat1 = saturate(r_surf_w) * splat3(0.5);
	/*				float3 attn_lf = 
						LF_Axis0.xyz * splat3(n_sat0.r) + 
						LF_Axis2.xyz * splat3(n_sat0.g) + 
						LF_Axis4.xyz * splat3(n_sat0.b) + 
						LF_Axis1.xyz * splat3(n_sat1.r) + 
						LF_Axis3.xyz * splat3(n_sat1.g) + 
						LF_Axis5.xyz * splat3(n_sat1.b);
					attn_lf *= 4.0;*/
					float3 l_w = normalize(
						n_sat1 * float3(dot(LF_Axis1.xyz, rgbweights), dot(LF_Axis3.xyz, rgbweights), dot(LF_Axis5.xyz, rgbweights)) -
						n_sat0 * float3(dot(LF_Axis0.xyz, rgbweights), dot(LF_Axis2.xyz, rgbweights), dot(LF_Axis4.xyz, rgbweights)));

/*					float3 l_w = normalize(
						n_sat1 * float3(dot(LF_Axis1.xyz, rgbweights), dot(LF_Axis3.xyz, rgbweights), dot(LF_Axis5.xyz, rgbweights)) -
						n_sat0 * float3(dot(LF_Axis0.xyz, rgbweights), dot(LF_Axis2.xyz, rgbweights), dot(LF_Axis4.xyz, rgbweights)) +
						r_sat1 * float3(dot(LF_Axis1.xyz, rgbweights), dot(LF_Axis3.xyz, rgbweights), dot(LF_Axis5.xyz, rgbweights)) -
						r_sat0 * float3(dot(LF_Axis0.xyz, rgbweights), dot(LF_Axis2.xyz, rgbweights), dot(LF_Axis4.xyz, rgbweights))
						);
*/						

//					n_sat0 = saturate(-l_w);
//					n_sat1 = saturate(l_w);
					n_sat0 = saturate(-l_w* splat3(0.75) + splat3(0.25));
					n_sat1 = saturate(l_w* splat3(0.75) + splat3(0.25));
					float3 attn_lf = 
						LF_Axis0.xyz * splat3(n_sat0.r) + 
						LF_Axis2.xyz * splat3(n_sat0.g) + 
						LF_Axis4.xyz * splat3(n_sat0.b) + 
						LF_Axis1.xyz * splat3(n_sat1.r) + 
						LF_Axis3.xyz * splat3(n_sat1.g) + 
						LF_Axis5.xyz * splat3(n_sat1.b);
						
			/*		n_sat0 = saturate(-r_surf_w);
					n_sat1 = saturate(r_surf_w);
					float3 attn_lf_r = 
						LF_Axis0.xyz * splat3(n_sat0.r) + 
						LF_Axis2.xyz * splat3(n_sat0.g) + 
						LF_Axis4.xyz * splat3(n_sat0.b) + 
						LF_Axis1.xyz * splat3(n_sat1.r) + 
						LF_Axis3.xyz * splat3(n_sat1.g) + 
						LF_Axis5.xyz * splat3(n_sat1.b);
					
					attn_lf += attn_lf_r;	*/
					attn_lf *= 4.0;
											
					
/*					float3 l_w = normalize(float3(
						dot(LF_Axis1.xyz, rgbweights) - dot(LF_Axis0.xyz, rgbweights),
						dot(LF_Axis3.xyz, rgbweights) - dot(LF_Axis2.xyz, rgbweights),
						dot(LF_Axis5.xyz, rgbweights) - dot(LF_Axis4.xyz, rgbweights)));
*/											
					n_sat0 = saturate(l_w);
					n_sat1 = saturate(-l_w);
					float3 lost = 
						LF_Axis0.xyz * splat3(n_sat0.r) + 
						LF_Axis2.xyz * splat3(n_sat0.g) + 
						LF_Axis4.xyz * splat3(n_sat0.b) + 
						LF_Axis1.xyz * splat3(n_sat1.r) + 
						LF_Axis3.xyz * splat3(n_sat1.g) + 
						LF_Axis5.xyz * splat3(n_sat1.b);

	//				n_sat0 = sqrt(n_sat0);
	//				n_sat1 = sqrt(n_sat1);
	/*				float3 l_w = normalize(float3(
						dot(LF_Axis1.xyz * splat3(n_sat1.r), rgbweights) - dot(LF_Axis0.xyz * splat3(n_sat0.r), rgbweights),
						dot(LF_Axis3.xyz * splat3(n_sat1.g), rgbweights) - dot(LF_Axis2.xyz * splat3(n_sat0.g), rgbweights),
						dot(LF_Axis5.xyz * splat3(n_sat1.b), rgbweights) - dot(LF_Axis4.xyz * splat3(n_sat0.b), rgbweights)));
	*/				
	//				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l_w, DiffuseColor, SpecularColor, FresParams, anisodir_w) * attn_lf;
	//				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, -l_w, DiffuseColor, SpecularColor, FresParams, anisodir_w) * lost;

	//				result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l_w, splat4(0.0), SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * splat3(1.0);
	//				tex_envmap.rgb = splat3(0.0);
					
					float3 lf = 
						LF_Axis0.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(-1.0, 0.0, 0.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) + 
						LF_Axis1.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(1.0, 0.0, 0.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) + 
						LF_Axis2.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(0.0, -1.0, 0.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) + 
						LF_Axis3.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(0.0, 1.0, 0.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) + 
						LF_Axis4.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(0.0, 0.0, -1.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) + 
						LF_Axis5.xyz * BRDF(matweights, n_w, nsurf_w, e_w, float3(0.0, 0.0, 1.0), DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb);
	
					lf *= splat3(4.0);
					result.rgb += lf;
	
				}
			"
			
			// -------------------------------------------------------------------
			*if_lightfieldmapping
			"
				{
					float4 tex_LFM0 = texture2D(sampler_LFM0, a_TCLFM.xy);
					float4 tex_LFM1 = texture2D(sampler_LFM1, a_TCLFM.xy);
					float4 tex_LFM2 = texture2D(sampler_LFM2, a_TCLFM.xy);
					float4 tex_LFM3 = texture2D(sampler_LFM3, a_TCLFM.xy);
					float4 tex_LFM4 = float4(tex_LFM1.a, tex_LFM2.a, tex_LFM3.a, 0.0);
					
					float3 n_sat0 = saturate(n_ts);
					float3 n_sat1 = saturate(-n_ts);
					float3 attn_lfm = 
						tex_LFM0.xyz * splat3(n_sat0.r) +
						tex_LFM1.xyz * splat3(n_sat1.b) + 
						tex_LFM2.xyz * splat3(n_sat1.g) +
						tex_LFM3.xyz * splat3(n_sat0.b) + 
						tex_LFM4.xyz * splat3(n_sat0.g);
						
					attn_lfm *= splat3(4.0 * a_LMIntensityScale.x);
					attn_lfm *= LFM_Scale.rgb;

					float3 rgbweights = float3(1.0,2.0,1.0);
					float3 l_ts = float3(dot(tex_LFM0.xyz, rgbweights),
						dot(tex_LFM4.xyz, rgbweights) - dot(tex_LFM2.xyz, rgbweights),
						dot(tex_LFM3.xyz, rgbweights) - dot(tex_LFM1.xyz, rgbweights));
					float3 l_w = SafeNormalize(TS_to_W(l_ts),0.01);
					
					result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l_w, DiffuseColor, SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * attn_lfm;
		//			result.rgb = result.rgb + BRDF(matweights, n_w, nsurf_w, e_w, l_w, splat4(0.0), SpecularColor, FresParams, anisodir_w, tex_envmap.rgb) * splat3(1.0);
		//			result.rgb = n_w*splat3(0.5) + splat3(0.5);
				}
			"

			// -------------------------------------------------------------------		
			*end
			"
				result.rgb *= splat3(4.0);
		//		result.rgb = (n_w + splat3(1.0)) * splat3(0.5);
		//		result.rgb = attn0.rgb *splat3(0.25);
				oCol0 = result;
			"
		}
	}
}
