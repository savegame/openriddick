/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Grow DoF
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
	*type  hls
    *flags nodebug //dopreparse
    *name XREngine_CreateMip
}

*flags
{
	*blur 1
}

*generate
{
	*gen 0
}

*param
{
	*envX		dUV_Mip
	*envX		UVMinMax
}

*texture
{
	*tex2D_0	sampler_tex0
}

*attrib
{
	*texcoord0	tc0
}

*output
{
	*color      oCol
}


*source
{
	*doeet
	"
		float minh(float4 _x)
		{
			return min(min(_x.x, _x.y), min(_x.z, _x.w));
		}
		float maxh(float4 _x)
		{
			return max(max(_x.x, _x.y), max(_x.z, _x.w));
		}
	"
}

*main
{
	*dosomething
	"
	//	float4 UVMinMax = float4(0.0, 0.0, 1.0, 1.0);
		float4 tex00 = texture2D(sampler_tex0, clamp(tc0.xy + float2(-dUV_Mip.x, -dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));
		float4 tex10 = texture2D(sampler_tex0, clamp(tc0.xy + float2(0.0, -dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));
		float4 tex20 = texture2D(sampler_tex0, clamp(tc0.xy + float2(dUV_Mip.x, -dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));
		float4 tex01 = texture2D(sampler_tex0, clamp(tc0.xy + float2(-dUV_Mip.x, 0.0), UVMinMax.xy, UVMinMax.zw));
		float4 tex11 = texture2D(sampler_tex0, clamp(tc0.xy + float2(0.0, 0.0), UVMinMax.xy, UVMinMax.zw));
		float4 tex21 = texture2D(sampler_tex0, clamp(tc0.xy + float2(dUV_Mip.x, 0.0), UVMinMax.xy, UVMinMax.zw));
		float4 tex02 = texture2D(sampler_tex0, clamp(tc0.xy + float2(-dUV_Mip.x, dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));
		float4 tex12 = texture2D(sampler_tex0, clamp(tc0.xy + float2(0.0, dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));
		float4 tex22 = texture2D(sampler_tex0, clamp(tc0.xy + float2(dUV_Mip.x, dUV_Mip.y), UVMinMax.xy, UVMinMax.zw));

		float4 b03 = float4(tex00.b, tex10.b, tex20.b, tex02.b);
		float4 b47 = float4(tex01.b, tex11.b, tex21.b, tex12.b);
		float bmin = min(tex22.b, minh(min(b03, b47)));
		
		float4 a03 = float4(tex00.a, tex10.a, tex20.a, tex02.a);
		float4 a47 = float4(tex01.a, tex11.a, tex21.a, tex12.a);
		float amax = max(tex22.a, maxh(max(a03, a47)));
		
		oCol = float4(tex11.r, tex11.g, bmin, amax);
	//	oCol = float4(0.3, 0.5, 1.0, 0.5);
		
	"
}
