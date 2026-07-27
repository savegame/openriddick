/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Blending used in gui
					
	Author:			
					
	Copyright:		Starbreeze AB 2008
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
	*type hls
	*flags nodebug
}

*param
{
	*env0 ColorMultiply
}

*attrib
{
	*texcoord0 TexCoordScreen
}

*texture
{
	*tex2D_0	TextureScreen
}

*output
{
	*color	outColor
}

*main
{
	*do
	"
		float4 ColorScreen = tex2D(TextureScreen, TexCoordScreen.xy);
		ColorScreen.a = 1.0;
		
		outColor = ColorScreen * ColorMultiply;
	"
}
