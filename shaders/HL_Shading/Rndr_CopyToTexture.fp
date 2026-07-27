/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			CopyToTexture
					
	Author:			Jim Kjellin
					
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
	*msaa2x	1
	*msaa4x	2
}

*generate
{
	*gen 0
	*gen msaa2x
	*gen msaa4x
}

*param
{
}

*texture
{
	*tex2D_0	tex
}

*attrib
{
	*position vPos
}

*output
{
	*color	oCol
}


*main
{
	*ifnot_msaa2x
	{
		*ifnot_msaa4x
		"
			float2 texscale = float2(1.0, 1.0);
		"
	}
	*if_msaa2x
	"
		float2 texscale = float2(2.0, 1.0);
	"
	*if_msaa4x
	"
		float2 texscale = float2(2.0, 2.0);
	"
	*dooeeet
	"
		oCol = tex2D(tex, vPos.xy * texscale);
	"
}
