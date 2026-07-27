#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2005
#
#	History:
#
#\*____________________________________________________________________________________________*/

*flags
{
	*motionmap	0x0001
	*dofmap		0x0002
	*dofalpha	0x0004
}

*generate
{
	*permute motionmap+dofmap+dofalpha
	{
		*gen 0
	}
}

*program
{
	*header
	"!!ARBfp1.0
		
		#-----------------------------------------
		# Texture0 = Normal map			// Default = 1, 0.5, 0.5
		# Texture1 = Diffuse Map		// Default = 1,1,1,1
		
		#-----------------------------------------
		# TexCoord0 = Mapping tex coord
		
		#-----------------------------------------
	"
	
	*begindeclarations
	"
		OUTPUT oCol0 = result.color;
		
		ATTRIB VPPos0 = fragment.texcoord[0];
		ATTRIB VPPos1 = fragment.texcoord[1];
		PARAM DOFParam0 = program.env[0];		# 1/(focus-near), 1/(far-focus), focus, farclamp
		PARAM DOFParam1 = program.env[1];		# nearclamp, _, _, _
		TEMP r0, r1, r2, r3;
		TEMP VPVel2D;
		
		PARAM const_val0 = { 0.5, 1, 0, 0 };
	"
	
	*enddeclarations
	"
		#-----------------------------------------
	"
	
	*if_motionmap
	"
		RCP r0.w, VPPos0.z;
		RCP r1.w, VPPos1.z;
		MUL r0.xy, VPPos0, r0.w;
		MUL r1.xy, VPPos1, r1.w;
		SUB r0.xy, r1, r0;
		#MUL r0.xyz, r0, 1;
		MOV r0.z, 0;
		MOV VPVel2D.rgb, r0;
		
		#MOV r0.z, 0.00001;
		#DP3 r0.w, r0, r0;
		#RSQ r1.w, r0.w;		# 1/Len
		#MUL r0.w, r1.w, r0.w;	# Len
		#MIN r0.w, 0.49999, r0.w;	# Min(1, Len)
		#MUL r0.xy, r0, r1.w;
		#MOV VPVel2D.zw, 0.0;
		#MUL VPVel2D.xy, r0, r0.w;
		
		ADD VPVel2D.xy, VPVel2D, 0.5;
		#MUL VPVel2D.z, VPPos0.z, 0.001953125;
	"
	
	*ifnot_dofmap
	"
		MOV r3.w, const_val0.x;
	"
	
	*if_dofmap
	"
		SUB r0.w, VPPos0.z, DOFParam0.z;	# Depth - Focus
		MUL r0.xy, r0.w, DOFParam0;			# r0.w*(1/(Focus-Near)), r0.w*(1/(Far-Focus))
		MAX r0.x, r0.x, DOFParam1.x;		# Clamp [Val, NearClamp] - NearClamp(0 -> -1)
		MIN r0.y, r0.y, DOFParam0.w;		# Clamp [Val, FarClamp] - FarClamp(0 -> 1)
		CMP r3.w, r0.w, r0.x, r0.y;			# Compare, (*sigh* Is there a better way?)
	"
	
	*if_dofalpha
	"
		# Scale and bias into range [0, 1] and store in alpha channel
		MAD_SAT VPVel2D.w, r3.w, const_val0.x, const_val0.x;
	"
	*ifnot_dofalpha
	"
		# Scale and bias into range [0, 1] and store in blue channel :(
		# Would prefer using alpha channel, but all platforms don't support that.
		MAD_SAT VPVel2D.z, r3.w, const_val0.x, const_val0.x;
	"
	
	*end
	"
		MOV oCol0, VPVel2D;
		END
	"
}

