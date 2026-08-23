/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			SSAO filter
					
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
	*tex2D_0		sampler_tex0
}

*attrib
{
	*texcoord0	tc_mapping
}

*output
{
	*color      oCol
}


*source
{
	*doeet
	"
	"
}

*main
{
	*dosomething
	"
		float2 dxy = float2(dUV_Mip.x*1.75, dUV_Mip.y*1.75);
		float4 tex0 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
		float4 tex1 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(-dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
		float4 tex2 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
		float4 tex3 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, -dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
		float4 tex4 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
		tex0 = min(tex0, tex1);
		tex0 = min(tex0, tex2);
		tex0 = min(tex0, tex3);
		tex0 = min(tex0, tex4);
		oCol = tex0;
	"
}
