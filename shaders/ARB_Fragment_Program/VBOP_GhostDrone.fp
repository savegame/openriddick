!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_VBOperator_GhostDrone
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2008
#					
#	History:		
#
#	Note:			Combine all the layers for the ghost drone hud interface
#
#\*____________________________________________________________________________________________*/

#-----------------------------------
OUTPUT oCol = result.color;

# Texture coordinates
ATTRIB tc0 = fragment.texcoord[0];	# screen coordinates
ATTRIB tc1 = fragment.texcoord[1];	# 0-1 texture
ATTRIB tc2 = fragment.texcoord[2];	# 1st offset
ATTRIB tc3 = fragment.texcoord[3];	# 2nd offset
ATTRIB tc4 = fragment.texcoord[4];	# 3rd offset
ATTRIB tc5 = fragment.texcoord[5];	# scaled scrolling
ATTRIB tc6 = fragment.texcoord[6];	# scrolling

# Predefines
PARAM c0 = program.env[0];
PARAM c1 = program.env[1];
PARAM c2 = program.env[2];
PARAM c3 = program.env[3];
PARAM c4 = program.env[4];
PARAM c5 = program.env[5];

# Constants
PARAM const_val0 = { 0, 0.5, 1, 2 };	# Used (xyzw)

# Registers
TEMP t0;	# screen
TEMP t1;	# dim_mul
TEMP t2;	# vascular_add
TEMP t3;	# 1st offseted vascular_add_effect
TEMP t4;	# 2nd offseted vascular_add_effect
TEMP t5;	# 3rd offseted vascular_add_effect
TEMP t6;	# radialzoommask
TEMP t7;	# scaled scrolling radialscanline
TEMP t8;	# scrolling radialscanline
TEMP r0;
TEMP r1;
#-----------------------------------

TEX t0, tc0, texture[0], 2D;	# Get screen texel
TEX t1, tc1, texture[1], 2D;	# Get dim_mul texel								(d * t)		mul
TEX t2, tc1, texture[2], 2D;	# Get vascular_add texel						(d + t)		add
TEX t3, tc2, texture[3], 2D;	# Get 1st offseted vascular_add_effect texel	(d + t)		add
TEX t4, tc3, texture[3], 2D;	# Get 2nd offseted vascular_add_effect texel	(d * 1-t)	lightmapblend
TEX t5, tc4, texture[3], 2D;	# Get 3rd offseted vascular_add_effect texel	(d + t)		add
TEX t6, tc1, texture[4], 2D;	# Get radialzoommask texel						   group layer
TEX t7, tc5, texture[5], 2D;	# Get scalaled scrolling radialscanline texel	(d + t)		add (grouped with t6)
TEX t8, tc6, texture[5], 2D;	# Get scrolling radialscanline texel			(d + t)		add	(grouped with t6)

# Invert texel 4
SUB t4, const_val0.zzzz, t4;

MAD r0, t0, t1, t2;
ADD r0, r0, t3;
MAD r0, r0, t4, t5;
MUL r1, t6, t7;
ADD r0, r0, r1;
MUL r1, t6, t8;
ADD oCol, r0, r1;

END
