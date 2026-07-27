/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Converts RGB to grayscale
					
	Author:			
					
	Copyright:		Starbreeze AB 2008
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type hls
    *flags nodebug
}

*flags
{
	*texture1	0x0001
	*rgb2alpha	0x0002
}

*generate
{
	*gen 0
	*gen texture1
	*gen rgb2alpha
	*gen texture1+rgb2alpha
}

*attrib
{
	*color		vCol
	*texcoord0	TCMapping0
	*if_texture1
	{
		*texcoord1	TCMapping1
	}
}

*param
{
}

*texture
{
	*tex2D_0	DiffuseTexture
	*if_texture1
	{
		*tex2D_1	MaskTexture
	}
}

*output
{
	*color	oCol
}

*main
{
	*do_0
	"
		// Sample diffuse and combine with vertex color
		vec4 Diffuse = tex2D(DiffuseTexture, TCMapping0.xy);
		Diffuse *= vCol;
	"
	
	*if_texture1
	"
		// Apply mask
		vec4 Mask = tex2D(MaskTexture, TCMapping1.xy);
		Diffuse *= Mask;
	"
	
	*do_1
	"
		// Calculate grayscale value and set output color
		float Gray = dot(Diffuse.xyz, vec3(0.212671, 0.715160, 0.072169));
		vec4 Result = vec4(Gray, Gray, Gray, Diffuse.a);
	"

	*if_rgb2alpha
	"
		Gray *= Diffuse.a;
		Result = vec4(1.0, 1.0, 1.0, Gray);
	"
	
	*do_2
	"
		oCol = Result;
	"
}
