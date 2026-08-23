/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			XR Shader Deferred
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2008
					
	History:
	

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags 0 //dopreparse
}

*flags
{
	*normal				1
	*alphamap			2
	*separateoffset		4
}

*generate
{
	*permute normal + alphamap + separateoffset
	{
		*gen 0
	}
}

*param
{
}

*texture
{
	*tex2D_0		sampler_material
    *if_alphamap
    {
		*tex2D_1		sampler_alphamap
		*if_separateoffset
		{
			*tex2D_2		sampler_sepblendoffset
		}
	}
}

*attrib
{
    *texcoord0	tc_mapping
    *if_alphamap
    {
	    *texcoord1	tc_alpha
	}
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
	"
}

*main
{
	*do1
	"
		float blendoffsetrange = 5.0;
	
		float4 final;
		float4 tex_material = texture2D(sampler_material, tc_mapping.xy);
		final.a = 1.0;
	"
	*if_normal
	"
		tex_material.rgb = splat3(0.5) * ConvertNormalTexel(tex_material) + splat3(0.5);
	"
	*if_alphamap
	{
		*do
		"
			float4 tex_alphamap = texture2D(sampler_alphamap, tc_alpha.xy);
			float blendoffset = tex_material.a;
		"
		*if_separateoffset
		"
			float4 tex_blendoffset = texture2D(sampler_sepblendoffset, tc_mapping.xy);
			blendoffset = tex_blendoffset.a;
		"
		*do2
		"
			final.a = saturate(tex_alphamap.r * (1.0 + blendoffsetrange) - blendoffsetrange + blendoffsetrange * blendoffset);
		//	final.rgb = splat3(blendoffset);
		//	final.a = 1.0;
		"
	}
	
	*do2
	"
		final.rgb = tex_material.rgb;
		oCol = final;
	"
}
