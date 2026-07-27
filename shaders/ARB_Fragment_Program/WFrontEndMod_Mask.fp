/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			WFrontEndMod_Mask

	Author:			
					
	Copyright:		Starbreeze AB 2008

	Description:	Used for blending togheter none post-processed gui overlay rendering with
					a mask texture previously created before post-processing was applied.
					We do this so none processed rendering doesn't end up ontop of gui.
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
	*type hls
	// *flags nodebug
}

*attrib
{
	*texcoord0 TexCoord_01
}

*texture
{
	*tex2D_0	Texture_0
	*tex2D_1	Texture_1
}

*output
{
	*color	outColor
}

*main
{
	*do
	"
		float4 Color0 = tex2D(Texture_0, TexCoord_01.xy);
		float4 Color1 = tex2D(Texture_1, TexCoord_01.xy);

		outColor = Color0 * Color1;
	"
}
