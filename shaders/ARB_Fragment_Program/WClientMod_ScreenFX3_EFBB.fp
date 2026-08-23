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

PARAM c0 = program.env[0];
PARAM c1 = program.env[1];

TEMP t0;
TEMP t1;

TEMP r0;
TEMP r1;

#-----------------------------------

TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[1], 2D;

MUL r0.rgb, t0, c0;
MAD r0.rgb, t1, c1, r0;
MOV r0.a, c0.a;

MOV oCol, r0;

END

