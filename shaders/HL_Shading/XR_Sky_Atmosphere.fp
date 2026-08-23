/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Sky light scatter
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*lightscatter		1
}

*generate
{
	*gen 0
}

*param
{
    *env0       eye
	*env1		suntheta	// theta, cos(theta), lightscale, unused
	*env2		sunvec
    *env3		skybaselight
}

*texture
{
	*texCube_0		texture_env
    *tex2D_1		texture_screen
}

*attrib
{
    *texcoord0	pixelinfo_v
}

*output
{
    *color      oCol
}


*source
{
	*INCLUDE "XR_FPUtil.fph"
	
	*doeet
	"
		float AttenuatedLightIntegral(float x, float k, float l, float m, float n)
		{
			//	S(0->x)(e^(kx + l) * e^(mx+n) dx) = 
			//	e^(l+n) * (e^(x(k+m)) - 1) / (k + m)
			
			return exp(l + n) * (exp(x * (k + m)) - 1.0) / (k + m);
		}
		
		float4 VolumeLight(float3 _v, float3 _p, float _depth, float _LightAttn)
		{
			float3 vnrm = normalize(_v);
			float Light = AttenuatedLightIntegral(_depth, -_LightAttn, 0.0, vnrm.z * _LightAttn, -_LightAttn * (384.0 - _p.z));
			float MieScatter = (2.0 + vnrm.z) * (1.0 / 3.0);
			Light = Light * _LightAttn * MieScatter;

			return splat4(Light) * float4(0.8, 1.0, 0.9, 1);
		}
	"
}

*main
{
	*doeeet
	"
		float3 eyevec = normalize(pixelinfo_v.xyz);
		float3 wpos = eye.xyz + pixelinfo_v.xyz;
		float costheta = dot(eyevec, sunvec.xyz);
		float rayleigh = (1.0 + costheta) * 1.5;
		float3 inscatter = skybaselight.rgb * float3(0.01, 0.01, 0.01) * splat3(rayleigh);
		float4 final = float4(inscatter.x, inscatter.y, inscatter.z, 0.97);
		oCol = final;
	"
}
