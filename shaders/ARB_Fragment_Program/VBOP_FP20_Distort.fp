#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_VBOperator_FP20_Distort
#					
#	Author:			
#					
#	Copyright:		Starbreeze AB 2007
#					
#	History:		
#
#\*____________________________________________________________________________________________*/

*flags
{
	*MeshScreenCoords	0x0001
}

*generate
{
	*permute MeshScreenCoords
	{
		*gen 0
	}
}

*program
{
	*header
	"!!ARBfp1.0
	OPTION ARB_precision_hint_fastest;

	#-----------------------------------
	"
	
	*begindeclarations
	{
		*shared
		"
		OUTPUT oCol = result.color;
		
		# Texture coordinates
		ATTRIB tc0 = fragment.texcoord[0];
		ATTRIB tc1 = fragment.texcoord[1];
		ATTRIB PixelPosition = fragment.texcoord[2];
		
		# Predefines
		PARAM c0 = program.env[0];
		PARAM c1 = program.env[1];
		
		# Program parameters
		PARAM p0 = program.env[1];				# { c1, SpeedX, SpeedY, Frequency }
		PARAM p1 = program.env[2];				# { UComp, VComp, WComp, 0 }
		
		# Constants
		PARAM const_val0 = { 0, 0.5, 1, 2 };	# Used (xyzw)
		
		# Registers
		TEMP t0;
		TEMP t1;
		TEMP t2;
		
		TEMP r0;
		TEMP r1;
		"
		
		*if_MeshScreenCoords
		"
		PARAM Const_Proj0 = program.env[3];
		PARAM Const_Proj1 = program.env[4];
		PARAM Const_Proj2 = program.env[5];
		PARAM Const_Proj3 = program.env[6];
		TEMP tcproj;
		"
		
		*enddeclarations
		"
		#------------------------------------------------
		"
	}
	
	*do
	"
		# Scroll coords and load normal map texel and mask texel
		MAD r1, p0.yzyz, c1.x, tc0;
		TEX t1, r1, texture[2], 2D;
		TEX t2, tc0, texture[1], 2D;
		
		#SWZ t1, t1, 0, g, 1, a;
		MAD r1.rgba, t1, const_val0.w, -const_val0.z;
		SWZ r1.rgb, r1, 0, a, r, 0;
		DP3 r1.r, r1, r1;
		#SUB r1.r, const_val0.z, r1.r;
		#SUB_SAT r1.r, const_val0.z, r1.r;
		RSQ r1.r, r1.r;
		RCP r1.r, r1.r;

		# Determine components to use
		MUL r1, r1, p1;
		
		# Multiply with mask
		MUL r1, r1, t2.xxxx;
	"
	
	*if_MeshScreenCoords
	"	
			# Calculate screen coordinate.
			DP4 tcproj.x, PixelPosition, Const_Proj0;
			DP4 tcproj.y, PixelPosition, Const_Proj1;
			DP4 tcproj.z, PixelPosition, Const_Proj2;
			DP4 tcproj.w, PixelPosition, Const_Proj3;
			
			RCP r0.w, tcproj.w;
			MUL r0.xy, tcproj, r0.w;

			# Offset
			MAD_SAT r0, r1, p0.w, r0;
	"
	
	*ifnot_MeshScreenCoords
	"
		# Offset
		MAD_SAT r0, r1, p0.w, tc1;
	"
	
	*finialize
	"	
		# Clamp inside framebuffer coordinates
		LRP r1.xy, r0.xyxy, c0.zwxy, c0.xyzw;
		
		# Load texel
		TEX t0, r1, texture[0], 2D;
		
		# End, write result
		MOV oCol, t0;
		END
		
		#------------------------------------------------
	"
}
