!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for COR:EFBB post processing effects
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];
ATTRIB tc2 = fragment.texcoord[2];
ATTRIB tc3 = fragment.texcoord[3];

PARAM c0 = program.env[0];
PARAM c1 = program.env[1];
PARAM c2 = program.env[2];
PARAM c3 = program.env[3];
PARAM c4 = program.env[4];
PARAM c5 = program.env[5];
PARAM c6 = program.env[6];
PARAM c7 = program.env[7];

PARAM two = {2.0, 2.0, 2.0, 2.0};

TEMP t0;
TEMP t1;
TEMP t2;
TEMP t3;

TEMP r0;
TEMP r1;

#-----------------------------------

TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[1], 2D;
TEX t2, tc2, texture[1], 2D;
TEX t3, tc3, texture[1], 2D;

# colorize base image
DP3 r0.rgb, t0, c0;
MUL r0.rgb, r0, c2;
MAD r0.rgb, r0, two, t0;
LRP r0.rgb, c3, r0, t0;
MUL r0.rgb, r0, c5;

# add blurred texture
ADD r1.rgb, t1, t2;
ADD r1.rgb, r1, t3;
MAD r0, r1, c4, r0;

MOV oCol, r0;

END

