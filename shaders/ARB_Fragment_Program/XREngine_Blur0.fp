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

PARAM c0 = program.env[0];
PARAM c1 = program.env[1];
PARAM c2 = program.env[2];
PARAM c3 = program.env[3];
PARAM c4 = program.env[4];

PARAM four = {4.0, 4.0, 4.0, 4.0};

TEMP t0;

TEMP r0;
TEMP r1;

#-----------------------------------------

TEX t0, tc0, texture[0], 2D;

ADD r0.rgb, t0, -c0;

MUL r0.rgb, r0, c1;
MUL r0.rgb, r0, four;

DP3 r1.rgb, r0, c2;
MUL r1.rgb, r1, c4;
MAD r0.rgb, r0, c3, r1;
MUL r0.rgb, r0, v0;
MOV r0.a, c2.a;

MOV oCol, r0;

END
