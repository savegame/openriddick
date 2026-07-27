!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			CMWnd_ModTexture_PaintVideo_YUV2RGB
#					
#	Author:			Jim Kjellin
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#-----------------------------------

OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB iCol = fragment.color;

# Y component in texture0's rgb
# U component in texture1's rgb
# V component in texture1's a

TEMP YComponent;
TEMP UVComponents;

ALIAS YUV = YComponent;
ALIAS RGB = UVComponents;

PARAM YScale = { 0.5, 1.164, -0.073035294117647058823529411764044, 0 };
PARAM UScale = { 0, -0.391, 2.018, 0 };
PARAM VScale = { 1.596, -0.813, 0, 0 };

#-----------------------------------

TEX YComponent, tc0, texture[0], 2D;
TEX UVComponents, tc0, texture[1], 2D;

SUB YUV.ga, UVComponents, YScale.x;
MAD RGB.rgb, YUV.r, YScale.y, YScale.z;
MAD RGB.rg, VScale.rgba, YUV.a, RGB;
MAD RGB.gb, UScale.rgba, YUV.g, RGB;

MUL_SAT oCol.rgb, RGB, iCol;
#MOV_SAT oCol.rgb, RGB;

END
