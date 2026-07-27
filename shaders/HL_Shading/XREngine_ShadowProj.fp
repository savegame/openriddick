/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Shadowmap projection using depth buffer
					
	Author:			Magnus Högdahl
					
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
	*sample2x2	0x0001
	*sample3x3	0x0002
	*sample4x4	0x0004
	*sample5x5	0x0008
}

*generate
{
	*gen sample2x2
	*gen sample3x3
	*gen sample4x4
	*gen sample5x5
}

*param
{
    *envX       VPParam
    *envX       VPConst
    *envX       VPScale
    *envX		V2SMat0
    *envX		V2SMat1
    *envX		V2SMat2
    *envX		V2SMat3
    *envX		LightPosV
    *envX		ShadowAttenuation
    *envX		SMPixelUV
    *envX       ScreenTransform
}

*texture
{
	*tex2DShadow_0		sampler_Shadowmap
	*tex2D_1		sampler_Depth
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
	
*bla
	"

float4 ShadowMapStep2(float _RefZ, float4 _SampledDepth) { return step( splat4(_RefZ), _SampledDepth); }

#define NUM_POISSON_TAPS 8
                      
#ifdef target_glsl
float shadow2D_Poisson8(sampler2DShadow DepthTex, float3 vShadowCoord, float2 _vScreen,float2 _PixelUV)
#else
float shadow2D_Poisson8(sampler2D DepthTex, float3 vShadowCoord, float2 _vScreen,float2 _PixelUV)
#endif
{
    float fAngle = (frac(_vScreen.x * 715.3) + frac(_vScreen.y * 517.3)) * 3.14;

	float4 vRotScale;
	float FilterScale = 1.25;
    vRotScale.x =  cos(fAngle) * FilterScale * _PixelUV.x;
    vRotScale.y =  sin(fAngle) * FilterScale * _PixelUV.y;
    vRotScale.z = -sin(fAngle) * FilterScale * _PixelUV.x;
    vRotScale.w =  cos(fAngle) * FilterScale * _PixelUV.y;
    float3 vShadowCoord1;
    float3 vShadowCoord2;
    float3 vShadowCoord3;
    float3 vShadowCoord4;
    float3 vShadowCoord5;
    float3 vShadowCoord6;
    float3 vShadowCoord7;
    float3 vShadowCoord8;
    
    vShadowCoord1.xy = vShadowCoord.xy + (1.000000 * vRotScale.xy) + (0.000000 * vRotScale.zw);
    vShadowCoord2.xy = vShadowCoord.xy + (0.527837 * vRotScale.xy) + (-0.085868 * vRotScale.zw);
    vShadowCoord3.xy = vShadowCoord.xy + (-0.040088 * vRotScale.xy) + (0.536087 * vRotScale.zw);
    vShadowCoord4.xy = vShadowCoord.xy + (-0.419418 * vRotScale.xy) + (-0.616039 * vRotScale.zw);
    vShadowCoord5.xy = vShadowCoord.xy + (-0.419418 * vRotScale.xy) + (-0.616039 * vRotScale.zw);
    vShadowCoord6.xy = vShadowCoord.xy + ( 0.440453 * vRotScale.xy) + (-0.639399 * vRotScale.zw);
    vShadowCoord7.xy = vShadowCoord.xy + (-0.757088 * vRotScale.xy) + (0.349334 * vRotScale.zw);
    vShadowCoord8.xy = vShadowCoord.xy + ( 0.574619 * vRotScale.xy) + (0.685879 * vRotScale.zw);

#ifdef target_hlsl
    float LOD = 0.0;
    float4 SampledDepth0;
    float4 SampledDepth1;

    asm
    {
        setTexLOD LOD.x
        tfetch2D SampledDepth0.x___, vShadowCoord1.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth0._x__, vShadowCoord2.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth0.__x_, vShadowCoord3.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth0.___x, vShadowCoord4.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true

        tfetch2D SampledDepth1.x___, vShadowCoord5.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth1._x__, vShadowCoord6.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth1.__x_, vShadowCoord7.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth1.___x, vShadowCoord8.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
	};
    float4 Attenuation0 = ShadowMapStep2( vShadowCoord.z, SampledDepth0 );
    float4 Attenuation1 = ShadowMapStep2( vShadowCoord.z, SampledDepth1 );
#else
    vShadowCoord1.z = vShadowCoord.z;
    vShadowCoord2.z = vShadowCoord.z;
    vShadowCoord3.z = vShadowCoord.z;
    vShadowCoord4.z = vShadowCoord.z;
    vShadowCoord5.z = vShadowCoord.z;
    vShadowCoord6.z = vShadowCoord.z;
    vShadowCoord7.z = vShadowCoord.z;
    vShadowCoord8.z = vShadowCoord.z;

    float4 Attenuation0;
    float4 Attenuation1;

    Attenuation0.x = shadow2D(DepthTex,vShadowCoord1).x;
    Attenuation0.y = shadow2D(DepthTex,vShadowCoord2).x;
    Attenuation0.z = shadow2D(DepthTex,vShadowCoord3).x;
    Attenuation0.w = shadow2D(DepthTex,vShadowCoord4).x;
    Attenuation1.x = shadow2D(DepthTex,vShadowCoord5).x;
    Attenuation1.y = shadow2D(DepthTex,vShadowCoord6).x;
    Attenuation1.z = shadow2D(DepthTex,vShadowCoord7).x;
    Attenuation1.w = shadow2D(DepthTex,vShadowCoord8).x;
#endif

	float4 vWeights = splat4(1.0 / float(NUM_POISSON_TAPS));
    return dot( Attenuation0, vWeights ) + dot( Attenuation1, vWeights );
}

	
#ifdef NO_PCF
                float SampleShadow2D(sampler2DShadow _Tex,vec3 _TC,float2 _WHRcp)
                {
                        float2 Part = _WHRcp;
                        _TC.xy -= (Part * 0.5);

                        // Determine fraction of pixels
                        float2 MinTC = mod(_TC.xy,Part);
                        float2 Frac = MinTC / Part;

                        // Make sure to sample at the center of each pixel so shader won't be broken for
                        // Platforms where linear filtering actually exists
                        _TC.xy += (Part * 0.5) - MinTC;

                        // Sample surrounding pixels
                        float Clr0 = shadow2D(_Tex,_TC).x;
                        float Clr1 = shadow2D(_Tex,float3(_TC.x+Part.x,_TC.y,_TC.z)).x;
                        float Clr2 = shadow2D(_Tex,float3(_TC.x,_TC.y+Part.y,_TC.z)).x;
                        float Clr3 = shadow2D(_Tex,_TC+float3(Part.x,Part.y,0.0)).x;

                        // Interpolate colors manually
                        Clr0 = lerp(Clr0,Clr1,Frac.x);
                        Clr1 = lerp(Clr2,Clr3,Frac.x);
                        return lerp(Clr0,Clr1,Frac.y);
                }
#else
                float SampleShadow2D(sampler2DShadow _Tex,vec3 _TC,vec2 _WHRcp)
                {
                        return shadow2D(_Tex,_TC).x;
                }
#endif

	"
}

*main
{
	*doeeet
	"
		float2 tc_screen_p = tc_screen.xy * splat2(1.0 / tc_screen.w);
		float4 tex_depth = texture2D(sampler_Depth, tc_screen_p.xy);
		float depth = ConvertDepth(tex_depth, VPConst);

                /*
                tc_screen_p += ScreenTransform.xy;
                tc_screen_p *= ScreenTransform.zw;
                */

                // tc_screen_p.y -= (1024.0 - 717.0) / 1024.0;
		float4 vpos = ConvertDepthUVToPos(depth, tc_screen_p, ScreenTransform);
		float4 vpossm = float4(dot(vpos, V2SMat0), dot(vpos, V2SMat1), dot(vpos, V2SMat2), dot(vpos, V2SMat3));
		vpossm = SMCoordinateTransform(vpossm);
		clip(vpossm.w);
		vpossm.z = max(vpossm.z, 0.0000001);
		float3 dUV = float3(SMPixelUV.x, SMPixelUV.y, 0.0);
	"

	// -------------------------------------------------------------------
	*if_sample2x2
	"
		// A single shadow map sample is a 2x2 pcf
		float Occlusion = shadow2D(sampler_Shadowmap, vpossm.xyz).x;
	"

	// -------------------------------------------------------------------
	*if_sample3x3
	"

	#if 0 // !defined(xenon)
		float Occlusion = 
			dot(splat4(0.25),
			float4(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, 0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, 0.5, 0.0)).x));
	#else

//		float Occlusion = shadow2D_3x3(sampler_Shadowmap, vpossm.xyz);
//		float Occlusion = shadow2D_4x4(sampler_Shadowmap, vpossm.xyz);
		float Occlusion = shadow2D_Poisson8(sampler_Shadowmap, vpossm.xyz, tc_screen_p.xy,SMPixelUV.xy);
	#endif
	"
	
	// -------------------------------------------------------------------
	*if_sample4x4
	"
	#if 1 //defined(target_glsl) || defined(xenon)
//		float Occlusion = shadow2D_4x4(sampler_Shadowmap, vpossm.xyz);
		float Occlusion = shadow2D_Poisson8(sampler_Shadowmap, vpossm.xyz, tc_screen_p.xy,SMPixelUV.xy);

	#else
		float Occlusion =
			dot(splat3(0.11111111),
			float3(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.0, 1.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.0, 0.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.0, -1.0, 0.0)).x) +
			float3(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.0, 1.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.0, 0.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.0, -1.0, 0.0)).x) +
			float3(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.0, 1.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.0, 0.0, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.0, -1.0, 0.0)).x));
	#endif

	"

	// -------------------------------------------------------------------
	*if_sample5x5
	"

	#if 0 //!defined(xenon)
		float Occlusion =
			dot(splat4(0.0625),
			float4(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.5, 1.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.5, 0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(1.5, -1.5, 0.0)).x) +
			float4(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, 1.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, 0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(0.5, -1.5, 0.0)).x) +
			float4(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, 1.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, 0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-0.5, -1.5, 0.0)).x) +
			float4(shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.5, 1.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.5, 0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.5, -0.5, 0.0)).x,
				shadow2D(sampler_Shadowmap, vpossm.xyz + dUV*float3(-1.5, -1.5, 0.0)).x)
			);
	#else

		// 5x5 is not implemented so we use the 4x4 sampler
	//	float Occlusion = shadow2D_4x4(sampler_Shadowmap, vpossm.xyz);
		float Occlusion = 
			(shadow2D_Poisson8(sampler_Shadowmap, vpossm.xyz, tc_screen_p.xy,SMPixelUV.xy) +
			 shadow2D_Poisson8(sampler_Shadowmap, vpossm.xyz, tc_screen_p.xy * 1.17 + splat2(0.03),SMPixelUV.xy)) * 0.5;
	
	#endif
	"

	// -------------------------------------------------------------------
	*doend
	"
	#if !defined(xenon)
		Occlusion = 1.0 - Occlusion;
	#endif
		Occlusion *= VPParam.w;
		
		float Attn = saturate((length((vpos - LightPosV).xyz) - ShadowAttenuation.x) * ShadowAttenuation.y);
		Occlusion  *= (1.0 - Attn);
		
		float4 final = float4(Occlusion, 0.0, 0.0, 1.0 - Occlusion);
		oCol = saturate(final);
	"
}
