!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CTextureContainer_Blur
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];
ATTRIB tc2 = fragment.texcoord[2];
ATTRIB tc3 = fragment.texcoord[3];

PARAM c0 = program.env[0];

TEMP t0;
TEMP t1;
TEMP t2;
TEMP t3;

TEMP r0;

#-----------------------------------------

TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[1], 2D;
TEX t2, tc2, texture[2], 2D;
TEX t3, tc3, texture[3], 2D;

MUL r0.rgb, t0, c0;
MAD r0.rgb, t1, c0, r0;
MAD r0.rgb, t2, c0, r0;
MAD r0.rgb, t3, c0, r0;
MUL r0.rgb, r0, v0;
MOV r0.a, c0.a;

MOV oCol, r0;

END
