!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for simple 5 points average blur pass
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
# 		
# 		
#\*____________________________________________________________________________________________*/


#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB texcoord = fragment.texcoord[0];

#-----------------------------------------

PARAM PixelSize = program.env[0];	# u,v,-u,-v

PARAM UVMin = program.env[1];		# u,v,u,v
PARAM UVMax = program.env[2];		# u,v,u,v

PARAM c0 = { 0.5, 0.25, 0, 0 };		# x,y,_,_

#-----------------------------------------
TEMP tcbase;
TEMP tc0;
TEMP tc1;

TEMP t0;
TEMP t1;
TEMP t2;
TEMP t3;
TEMP t4;

TEMP r0;

#-----------------------------------------

SWZ tcbase, texcoord, x, y, x, y;
MOV tc0, tcbase;
MOV tc1, tcbase;

ADD tc0.yw, tcbase, PixelSize;
ADD tc1.xz, tcbase, PixelSize;
MIN tcbase, tcbase, UVMax;
MAX tcbase, tcbase, UVMin;
MIN tc0, tc0, UVMax;
MIN tc1, tc1, UVMax;
MAX tc0, tc0, UVMin;
MAX tc1, tc1, UVMin;

TEX t0, tcbase, texture[0], 2D;
TEX t1, tc0, texture[0], 2D;
TEX t2, tc0.zwzw, texture[0], 2D;
TEX t3, tc1, texture[0], 2D;
TEX t4, tc1.zwzw, texture[0], 2D;

MUL r0, t0, c0.x;
MAD r0, t1, c0.y, r0;
MAD r0, t2, c0.y, r0;
MUL r0, r0, c0.x;
MAD r0, t3, c0.y, r0;
MAD oCol, t4, c0.y, r0;

END
