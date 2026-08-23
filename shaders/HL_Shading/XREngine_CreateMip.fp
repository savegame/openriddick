/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Create mipmap
					
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
	*color aColor
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
//		float4 tex0 = texture2DLod(sampler_tex0, tc_mapping.xy, dUV_Mip.z - 1.0);
		if (1 == 1)
		{
			float2 dxy = float2(dUV_Mip.x*2.00, dUV_Mip.y*2.00);
			float4 tex0 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(-dxy.x, -dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex1 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(-dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex2 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(-dxy.x, dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex3 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, -dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex4 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex5 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex6 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(dxy.x, -dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex7 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex8 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(dxy.x, dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			oCol = (tex0 + tex1 + tex2 + tex3 + tex4 + tex5 + tex6 + tex7 + tex8) * splat4(1.0 / 9.0);
			oCol *= aColor;
		}
		else
		if (1 == 1)
		{
			float2 dxy = float2(dUV_Mip.x*1.75, dUV_Mip.y*1.75);
			float4 tex0 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex1 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(-dxy.x, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex2 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex3 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, -dxy.y), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			float4 tex4 = texture2DLod(sampler_tex0, clamp(tc_mapping.xy + float2(0.0, 0.0), UVMinMax.xy, UVMinMax.zw), dUV_Mip.z - 1.0);
			oCol = (tex4*1.0 + tex0 + tex1 + tex2 + tex3) * splat4(1.0 / 5.0);
		}
		else if (1 == 1)
		{
	//		float2 dxy = float2(dUV_Mip.x*1.0, dUV_Mip.y*1.0);
			float2 dxy = float2(dUV_Mip.x*0.8, dUV_Mip.y*0.8);
			float4 tex0 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(dxy.x, dxy.y), dUV_Mip.z - 1.0);
			float4 tex1 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(dxy.x, -dxy.y), dUV_Mip.z - 1.0);
			float4 tex2 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(-dxy.x, -dxy.y), dUV_Mip.z - 1.0);
			float4 tex3 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(-dxy.x, dxy.y), dUV_Mip.z - 1.0);
			oCol = (tex0 + tex1 + tex2 + tex3) * splat4(0.25);
	/*		tex0.rgb = tex0.rgb * tex0.rgb;
			tex1.rgb = tex1.rgb * tex1.rgb;
			tex2.rgb = tex2.rgb * tex2.rgb;
			tex3.rgb = tex3.rgb * tex3.rgb;
			oCol = (tex0 + tex1 + tex2 + tex3) * splat4(0.25);
			oCol.r = sqrt(oCol.r);
			oCol.g = sqrt(oCol.g);
			oCol.b = sqrt(oCol.b);*/
		}
		else
		{
			float4 tex0 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(dUV_Mip.x, 0.0), dUV_Mip.z - 1.0);
			float4 tex1 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(-dUV_Mip.x, 0.0), dUV_Mip.z - 1.0);
			float4 tex2 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(0.0, dUV_Mip.y), dUV_Mip.z - 1.0);
			float4 tex3 = texture2DLod(sampler_tex0, tc_mapping.xy + float2(0.0, -dUV_Mip.y), dUV_Mip.z - 1.0);
			oCol = (tex0 + tex1 + tex2 + tex3) * splat4(0.25);
		}

//		oCol = tex0;
	"
}
