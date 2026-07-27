!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for Tentacle GUI screen fadeing
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

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];
PARAM p1 = program.env[1];

PARAM c0 = { 2.28, 0, 0, 0 };

TEMP t0;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;

#-----------------------------------

# Calculate coordinates around sampling pixel
ADD r0, tc0, p0.xzzz;
SUB r1, tc0, p0.xzzz;
ADD r2, tc0, p0.zyzz;
SUB r3, tc0, p0.zyzz;

# Sample pixels
TEX t0, tc0, texture[0], 2D;
TEX r0, r0, texture[0], 2D;
TEX r1, r1, texture[0], 2D;
TEX r2, r2, texture[0], 2D;
TEX r3, r3, texture[0], 2D;

# Add togheter everything
ADD r0, r0, r1;
ADD r1, r2, r3;
ADD r0, r0, r1;

# Scale and interpolate using fade timer
MAD r0, r0, p1.y, t0;
LRP oCol, p1.z, c0.xxxx, r0;

END

