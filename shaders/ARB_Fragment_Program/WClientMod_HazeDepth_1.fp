/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for "Depth-Haze"
					
	Author:			Patrik Willbo
					
	Copyright:		Starbreeze AB 2007
					
	Comment:		Distort original screen texture using "scrolling" normalmap textures
					and mask texture to determine area of effect.
					
			Texture0 = Mask texture
			Texture1 = Original screen texture
			Texture2 = Distortion normal 1
			Texture3 = Distortion normal 2
\*____________________________________________________________________________________________*/
*flags
{
	*blendnormalmaps	0x0001
}

*generate
{
	*gen 0
	*gen blendnormalmaps
}

*program
{
	*doInit
	"!!ARBfp1.0
	 OPTION ARB_precision_hint_fastest;
	 
		#------------------------------------------------
		
		OUTPUT oCol = result.color;
		
		# Texture coordinates
		ATTRIB tc0 = fragment.texcoord[0];		# Mask
		ATTRIB tc1 = fragment.texcoord[1];		# Original screen
		ATTRIB tc2 = fragment.texcoord[2];		# Distortion map 1
	"
	
	*if_blendnormalmaps
	"
		ATTRIB tc3 = fragment.texcoord[3];		# Distortion map 2
	"
	
	*doInitEnd
	"	
		# Program parameters
		PARAM p0 = program.env[0];				# { DistortionStrength, Mix0, Mix1, _ }
		
		# Constants
		PARAM cv0 = { 0, 1, 2, 0 };				# Used (xyz_)	
		
		# Registers
		TEMP t0;
		TEMP t1;
		TEMP t2;
		TEMP t3;
		TEMP r0;
		
		#------------------------------------------------
	"
	
	*doProgram
	"	
		# Sample textures
		TEX t0, tc0, texture[0], 2D;		# Mask texel
		TEX t2, tc2, texture[2], 2D;		# Distort texel 1
	"
	
	*if_blendnormalmaps
	"
		TEX t3, tc3, texture[3], 2D;		# Distort texel 2
	"
	
	*doProgramNormalize
	"
		# Reconstruct both normals from b,g component
		MAD t2.rgba, t2, cv0.z, -cv0.y;
		SWZ t2.rgb, t2, 0, a, r, 0;
		DP3 t2.r, t2, t2;
		SUB t2.r, cv0.y, t2.r;
		RSQ t2.r, t2.r;
		RCP t2.r, t2.r;
		
		# Apply mix? (Probably not, use distortion instead!)
	"
	
	*if_blendnormalmaps
	"
		MAD t3.rgba, t3, cv0.z, -cv0.y;
		SWZ t3.rgb, t3, 0, a, r, 0;
		DP3 t3.r, t3, t3;
		SUB t3.r, cv0.y, t3.r;
		RSQ t3.r, t3.r;
		RCP t3.r, t3.r;
		
		# Blend the two normals togheter, using specified strength
		MUL r0, t2, p0.y;
		MAD t2, t3, p0.z, t2;
		
		# Normalize t2? (I'm not really sure this is needed or wanted actually)
		# DP3 t2.w, t2, t2;
		# RSQ t2.w, t2.w;
		# MUL t2.xyz, t2, t2.w;
	"
		
	*doProgramFinal
	"
		# Apply distort mask and strength before fetching screen texel
		MUL r0, t2, t0.xxxx;
		MAD r0, r0, p0.x, tc1;
		TEX t1, r0, texture[1], 2D;			# Screen texel

		# MOV t1, t2;			# Debug, shows the normal map on screen
		# MOV t1, t0.xxxx;		# Debug, shows the mask map on screen

		# Set result
		MOV oCol, t1;
		
		END
		
		#------------------------------------------------
	"
}
