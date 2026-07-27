!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Fresnel
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
#tex t1
#
#dp3 r0, t0_bx2, t1_bias
#mad_x2 r0.a, r0.b, c0.b, c1.b
#mov r0.a, 1-r0.a

#-----------------------------------

OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];

PARAM c0 = program.env[0];
PARAM c1 = program.env[1];

PARAM Const0 = {0.0, 0.5, 1.0, 2.0};

TEMP NormalMapTexel;
TEMP t1;

TEMP r0;

#-----------------------------------

TEX NormalMapTexel, tc0, texture[0], 2D;
#TEX t1, tc1, texture[1], CUBE;

MOV t1, tc1;
DP3 t1.a, t1, t1;
RSQ t1.a, t1.a;
MUL t1.rgb, t1, t1.a;

MAD NormalMapTexel.rgba, NormalMapTexel, Const0.w, -Const0.z;	# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
SWZ NormalMapTexel.rgb, NormalMapTexel, 0, a, g, 0;
DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;	# r = g^2 + b^2
SUB NormalMapTexel.r, Const0.z, NormalMapTexel.r;		# r = 1 - r;
RSQ NormalMapTexel.r, NormalMapTexel.r;			# r = 1/sqrt(r)
RCP NormalMapTexel.r, NormalMapTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

#MAD t1, t1, Const0.w, -Const0.z;
#SUB t1, t1, Const0.y;

MOV r0.rgb, Const0.z;
DP3 r0.a, NormalMapTexel, t1;
#MAD r0.a, r0.b, c0.b, c1.b;
#MUL r0.a, r0.a, Const0.w;
#SUB r0.a, Const0.z, r0.a;
#MAX r0.a, r0.a, Const0.x;

#MOV r0.a, c1.b;
#MOV oCol.rgb,

MAD r0.rgb, NormalMapTexel, 0.5, 0.5;
MOV oCol, r0;

#SUB oCol.a, Const0.z, r0.a;

#MOV oCol.rgb, r0.a;
#MOV oCol.a, 1;

#MAD oCol.rgb, t1, 0.5, 0.5;
#MOV oCol, r0;

#MOV oCol, Const0.z;

END

