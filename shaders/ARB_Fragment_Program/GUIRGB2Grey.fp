!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for picking R, G or B channel
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB iCol = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];	# R,G or B is set to 1

TEMP t0;
TEMP r0;

#-----------------------------------

# Fetch texel
TEX t0, tc0, texture[0], 2D;

# Convert RGB to greyscale using parameter and copy texel alpha
DP3 r0, t0, p0;
MOV r0.w, t0.w;

# Multiply grey scale result with input color
MUL oCol, r0, iCol;

END