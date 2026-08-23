!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for XREngine guassian blur
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
# 		Cycles/64 pixel vector: ALU 82.7, vertex 0, texture 136, sequencer 32, interpolator 4
# 		18 GPRs, 10 threads, Performance (if enough threads): ~136 cycles per vector
#\*____________________________________________________________________________________________*/


#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB texcoord = fragment.texcoord[0];

#-----------------------------------------
PARAM ColorBias = program.env[0];	# r,g,b,ZoneMin
PARAM ColorScale = program.env[1];	# r,g,b,ZoneMax
PARAM ColorGamma = program.env[2];	# r,g,b

PARAM UVMin = program.env[3];		# u,v,u,v
PARAM UVMax = program.env[4];		# u,v,u,v

PARAM Weight0 = program.env[5];		# UCenter, VCenter, 0, w0
PARAM Weight1 = program.env[6];		# w1,w2,w3,w4
PARAM Weight5 = program.env[7];		# w5,w6,w7,w8

PARAM dUV12 = program.env[8];
PARAM dUV34 = program.env[9];
PARAM dUV56 = program.env[10];
PARAM dUV78 = program.env[11];

PARAM c0 = { 0.5, 1, 0, 0 };	# Used (x,y,z,_)

#-----------------------------------------
TEMP tcbase;
TEMP tc0;
TEMP tc1;
TEMP tc2;
TEMP tc3;
TEMP tc4;

TEMP t0;
TEMP t1;
TEMP t2;
TEMP t3;
TEMP t4;

TEMP r0;

TEMP t0org;
TEMP Result;

#-----------------------------------------

# Get coordinate in 0->1 range
SUB r0, UVMax, UVMin;
RCP r0.z, r0.x;
RCP r0.w, r0.y;
SUB r0.xy, texcoord, UVMin;
MUL r0, r0.xyxy, r0.zwzw;

# Calculate length
SUB r0, c0.xxzz, r0;
MOV r0.z, c0.z;
DP3 r0.w, r0, r0;
RSQ r0.z, r0.w;
MUL r0.y, r0.z, r0.w;

# Calculate fade scale zone and clamp within 0-1 range
LRP r0.x, r0.y, ColorScale.w, ColorBias.w;
MIN r0.x, r0.x, c0.y;
MAX r0.x, r0.x, c0.z;


TEX t0, texcoord, texture[0], 2D;
MOV t0org, t0;
#MOV Result, t0;
MUL Result, t0, Weight0.w;
SWZ tcbase, texcoord, x, y, x, y;

ADD tc0, tcbase, dUV12;
MIN tc0, tc0, UVMax;
MAX tc0, tc0, UVMin;
SWZ tc1, tc0, z,w,0,0;
ADD tc2, tcbase, dUV34;
MIN tc2, tc2, UVMax;
MAX tc2, tc2, UVMin;
SWZ tc3, tc2, z,w,0,0;
TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[0], 2D;
TEX t2, tc2, texture[0], 2D;
TEX t3, tc3, texture[0], 2D;
MAD Result.rgb, t0, Weight1.x, Result;
MAD Result.rgb, t1, Weight1.y, Result;
MAD Result.rgb, t2, Weight1.z, Result;
MAD Result.rgb, t3, Weight1.w, Result;

ADD tc0, tcbase, dUV56;
MIN tc0, tc0, UVMax;
MAX tc0, tc0, UVMin;
SWZ tc1, tc0, z,w,0,0;
ADD tc2, tcbase, dUV78;
MIN tc2, tc2, UVMax;
MAX tc2, tc2, UVMin;
SWZ tc3, tc2, z,w,0,0;
TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[0], 2D;
TEX t2, tc2, texture[0], 2D;
TEX t3, tc3, texture[0], 2D;
MAD Result.rgb, t0, Weight5.x, Result;
MAD Result.rgb, t1, Weight5.y, Result;
MAD Result.rgb, t2, Weight5.z, Result;
MAD Result.rgb, t3, Weight5.w, Result;

SUB tc0, tcbase, dUV12;
MIN tc0, tc0, UVMax;
MAX tc0, tc0, UVMin;
SWZ tc1, tc0, z,w,0,0;
SUB tc2, tcbase, dUV34;
MIN tc2, tc2, UVMax;
MAX tc2, tc2, UVMin;
SWZ tc3, tc2, z,w,0,0;
TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[0], 2D;
TEX t2, tc2, texture[0], 2D;
TEX t3, tc3, texture[0], 2D;
MAD Result.rgb, t0, Weight1.x, Result;
MAD Result.rgb, t1, Weight1.y, Result;
MAD Result.rgb, t2, Weight1.z, Result;
MAD Result.rgb, t3, Weight1.w, Result;

SUB tc0, tcbase, dUV56;
MIN tc0, tc0, UVMax;
MAX tc0, tc0, UVMin;
SWZ tc1, tc0, z,w,0,0;
SUB tc2, tcbase, dUV78;
MIN tc2, tc2, UVMax;
MAX tc2, tc2, UVMin;
SWZ tc3, tc2, z,w,0,0;
TEX t0, tc0, texture[0], 2D;
TEX t1, tc1, texture[0], 2D;
TEX t2, tc2, texture[0], 2D;
TEX t3, tc3, texture[0], 2D;
MAD Result.rgb, t0, Weight5.x, Result;
MAD Result.rgb, t1, Weight5.y, Result;
MAD Result.rgb, t2, Weight5.z, Result;
MAD Result.rgb, t3, Weight5.w, Result;

MUL Result.rgb, Result, ColorScale;
ADD_SAT Result.rgb, Result, ColorBias;
POW Result.r, Result.r, ColorGamma.r;
POW Result.g, Result.g, ColorGamma.g;
POW Result.b, Result.b, ColorGamma.b;

#TEX Result, texcoord, texture[0], 2D;
LRP Result, r0.x, Result, t0org;

MOV oCol, Result;

END
