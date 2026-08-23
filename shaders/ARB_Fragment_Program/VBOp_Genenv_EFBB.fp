!!ARBfp1.0

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Bump mapped environment mapping (GenEnv surface operator)
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#ps.1.1
#
#tex t0
#texm3x3pad t1, t0_bx2
#texm3x3pad t2, t0_bx2
#texm3x3vspec t3, t0_bx2
#
#mul r0.rgb, t3, v0
#mov r0.a, v0.a

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];
ATTRIB tc2 = fragment.texcoord[2];
ATTRIB tc3 = fragment.texcoord[3];

PARAM one = {1.0, 1.0, 1.0, 1.0};
PARAM two = {2.0, 2.0, 2.0, 2.0};

TEMP tc4;

TEMP t0;
TEMP t3;
TEMP u;
TEMP e;

TEMP r0;

#-----------------------------------

TEX t0, tc0, texture[0], 2D;

MAD t0, t0, two, -one;

DP3 u.x, tc1, t0;
DP3 u.y, tc2, t0;
DP3 u.z, tc3, t0;
MOV e.x, tc1.w;
MOV e.y, tc2.w;
MOV e.z, tc3.w;
DP3 r0.a, e, u;
DP3 r0.x, u, u;
RCP r0.x, r0.x;
MUL r0.x, r0.x, r0.a;
MUL r0.x, r0.x, two.x;
MAD tc4, u, r0.x, -e;

TEX t3, tc4, texture[3], CUBE;

MUL r0.rgb, t3, v0;
MOV r0.a, v0.a;
MOV oCol, r0;


END

