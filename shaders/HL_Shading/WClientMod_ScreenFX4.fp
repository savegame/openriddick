/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR Nightvision effect
					
	Author:			Anders Ekermo (original by Magnus Högdahl)
					
	Copyright:		Starbreeze AB 2008
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}


*param
{
    *env   ScaleOffs
    *env   ScaleParam
}

*texture
{
    *tex2D_0    DiffuseTex
}

*attrib
{
    *texcoord0  TexCoord
    *texcoord1  ProjX
    *texcoord2  ProjY
}

*output
{
    *color      oCol
}

*main
"
    vec2 TCloc = (TexCoord.xy - ScaleOffs.zw) * ScaleOffs.xy;
    float Ln = length(TCloc);
    vec2 TCnrm = TCloc / Ln;

    Ln = max((Ln * 0.7071067) - 0.45,0.0) * 1.818181;
    Ln *= Ln;

    vec3 TCoffs;
    TCoffs.xy = TCnrm * Ln * ScaleParam.x;
    TCoffs.z = 1.0;
    TCloc.x = dot(ProjX.xyz,TCoffs);
    TCloc.y = dot(ProjY.xyz,TCoffs);
    
    oCol = texture2D(DiffuseTex,TCloc);
"