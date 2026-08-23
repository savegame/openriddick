#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_VBOperator_MechHood
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2008
#					
#	History:		
#
#	Note:			Combine all the layers for the 'mech hood' hud interface
#
#\*____________________________________________________________________________________________*/

*flags
{
	MaskMaps	0x0001
}

*generate
{
	*permute MaskMaps
	{
		*gen 0
	}
}

*program
{
	*header
	"!!ARBfp1.0
	OPTION ARB_precision_hint_fastest;
	"
	
	*begindeclarations
	{
		*common
		"
		#-----------------------------------
		OUTPUT oCol = result.color;
		
		# Texture coordinates
		ATTRIB tc0 = fragment.texcoord[0];	# mesh coordinates
		ATTRIB tc1 = fragment.texcoord[1];	# pixel position
		ATTRIB tc2 = fragment.texcoord[2];	# 1st offset
		ATTRIB tc3 = fragment.texcoord[3];	# 2nd offset
		ATTRIB tc4 = fragment.texcoord[4];	# scaled scrolling
		
		# Predefines
		PARAM c0 = program.env[0];
		PARAM c1 = program.env[1];
		PARAM c2 = program.env[2];
		PARAM c3 = program.env[3];
		PARAM proj0 = program.env[4];
		PARAM proj1 = program.env[5];
		PARAM proj2 = program.env[6];
		PARAM proj3 = program.env[7];
		
		# Constants
		PARAM const_val0 = { 0, 0.5, 1, 2 };	# Used (xyzw)
		
		# Registers
		TEMP t0;	# Screen texel
		TEMP t1;	# Mul2 visir_dirt layer
		TEMP t2;	# Offseted visir_static add layer
		TEMP t3;	# Offseted visir_static add layer
		TEMP t4;	# Scaled and scrolled visir_scanline add layer
		TEMP r0;
		TEMP r1;
		"
		
		*if_MaskMaps
		"
		TEMP t5;	# Mask texel
		"
	}
	
	*enddeclarations
	"
		#-----------------------------------
	"
	
	*do
	"
		# Calculate texture coordinates for screen texel
		DP4 r0.x, tc1, proj0;
		DP4 r0.y, tc1, proj1;
		DP4 r0.z, tc1, proj2;
		DP4 r0.w, tc1, proj3;
		
		RCP r1.w, r0.w;
		MUL r0.xy, r0, r1.w;
		
		# Borders
		ADD r0, r0, c0;
		
		# Load texels
		TEX t0, r0,  texture[0], 2D;	# Get screen texel
		TEX t1, tc0, texture[1], 2D;	# Get visir_dirt texel
		TEX t2, tc2, texture[2], 2D;	# Get 1st offseted visir_static texel
		TEX t3, tc3, texture[2], 2D;	# Get 2nd offseted visir_static texel
		TEX t4, tc4, texture[3], 2D;	# Get visir_scanline texel
	"
	
	*if_MaskMaps
	"
		TEX t5, tc0, texture[4], 2D;	# Get masking texel
		
		# Apply mask texel on visir_static and visir_scanline texels
		MUL t2, t2, t5;
		MUL t3, t3, t5;
		MUL t4, t4, t5;
	"
	
	*finalize
	"
		# First mul2 layer
		MUL r0, t0, t1;
		ADD r0, r0, r0;
		
		# Second mul2 layer
		MUL r1, r0, t1;
		ADD r0, r1, r1;
		
		# First and second offseted add layer
		MAD r0, t2, c1, r0;
		MAD r0, t3, c2, r0;
		
		# Scanline add layer
		MAD oCol, t4, c3, r0;
	"
	
	*endprogram
	"
		END
	"
}


