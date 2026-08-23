!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Blend between texture and color
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2008
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];
PARAM p1 = program.env[1];

TEMP t0;
TEMP r0;

#-----------------------------------

TEX t0, tc0, texture[0], 2D;

LRP r0, p1, p0, t0;

MOV oCol, r0;

END
