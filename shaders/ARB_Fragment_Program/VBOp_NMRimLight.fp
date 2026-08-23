#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_VBOperator_NMRimLight
#					
#	Author:			Patrik Willbo
#					
#	Copyright:		Starbreeze AB 2007
#					
#	History:		
#		07-11-30	Created file
#
#\*____________________________________________________________________________________________*/

*flags
{
	*MaskMap			0x0001
	*FlipV				0x0002
	*AlphaToCoverage	0x0004
	*ProjCoord			0x0008
}

*generate
{
	*permute MaskMap+FlipV+AlphaToCoverage+ProjCoord
	{
		*gen 0
	}
}

*program
{
	*header
	"!!ARBfp1.0
	
		#-----------------------------------------
		#Texture0 = Normal map
		#Texture1 = Mask texture
		#Texture2 = Diffuse map
		
		#TexCoord0 = Diffuse/Normal/Specular tex coord
		#TexCoord1 = Animated model space pixel position
		#TexCoord2 = Interpolated tangent space eye vector (IPTSEV)
		#TexCoord3 = Mask texture coordinates
		
		#-----------------------------------------
	"
	
	*begindeclarations
	"
		OUTPUT oCol = result.color;
	
		ATTRIB vCol = fragment.color;
		
		ATTRIB TexCoord = fragment.texcoord[0];
		ATTRIB PixelPosition = fragment.texcoord[1];
		ATTRIB IPTSEV = fragment.texcoord[2];
		
		PARAM Color = program.env[0];
		PARAM Scroll = program.env[1];
		PARAM Const_Proj0 = program.env[2];
		PARAM Const_Proj1 = program.env[3];
		PARAM Const_Proj2 = program.env[4];
		PARAM Const_Proj3 = program.env[5];
		
		PARAM const_val = { 0.0, 0.75, 1.0, 2.0 };
		
		TEMP NormalMapTexel;
		TEMP MaskMapTexel;
		TEMP TSEV;				# Tangent space eye vector
		TEMP tcproj;
		TEMP r0;
	"
	
	*enddeclarations
	"
		#-----------------------------------------
	"
	
	*if_MaskMap
	{
		*if_ProjCoord
		{
			*do
			"
				DP4 tcproj.x, PixelPosition, Const_Proj0;
				DP4 tcproj.y, PixelPosition, Const_Proj1;
				DP4 tcproj.z, PixelPosition, Const_Proj2;
				DP4 tcproj.w, PixelPosition, Const_Proj3;
				
				RCP r0.w, tcproj.w;
				MUL r0.xy, tcproj, r0.w;
			"

			*if_FlipV
			"
				SUB r0.y, const_val.z, r0.y;
			"
		}
		
		*ifnot_ProjCoord
		"
			MOV r0, TexCoord;
		"
		
		*doscroll
		"
			MAD r0.xy, r0, Scroll.zwzw, Scroll;
		"
	}

	*fetchtextures
	{
		*Normalmap
		"
			#-----------------------------------------
			# Fetch Textures
			@if dynmip
			TEXDYN NormalMapTexel, TexCoord, texture[0], 2D;	# Sample normalmap
			@else
			TEX NormalMapTexel, TexCoord, texture[0], 2D;		# Sample normalmap
			@endif
		"
		
		*if_MaskMap
		"
			@if dynmip
			TEXDYN MaskMapTexel, r0, texture[1], 2D;	# Sample maskmap
			@else
			TEX MaskMapTexel, r0, texture[1], 2D;		# Sample maskmap
			@endif
		"
	}

	*donormalmaptsev
	"
		#-----------------------------------------
		# Reconstruct normal from g,b components
		MAD NormalMapTexel.rgba, NormalMapTexel, const_val.a, -const_val.b;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
		SWZ NormalMapTexel.rgb, NormalMapTexel, 0, a, g, 0;
		DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;	# r = g^2 + b^2
		SUB_SAT NormalMapTexel.r, const_val.z, NormalMapTexel.r;	# r = 1 - r;
		RSQ NormalMapTexel.r, NormalMapTexel.r;			# r = 1/sqrt(r)
		RCP NormalMapTexel.r, NormalMapTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

		#-----------------------------------------
		# Normalize TSEV
		@if support_normalize
		NRM TSEV.xyz, IPTSEV.xyz;
		@else
		DP3 TSEV.a, IPTSEV, IPTSEV;
		RSQ TSEV.a, TSEV.a;
		MUL TSEV.xyz, IPTSEV, TSEV.a;
		@endif               
	"
	
	*dorimlight
	{
		*do
		"
			#-----------------------------------------
			# "Rim" light and color multiplications
			DP3 r0, NormalMapTexel, TSEV;
			SUB_SAT r0, const_val.z, r0;
			MUL r0, r0, r0;
		"
		
		*if_MaskMap
		{
			*do
			"
				MUL r0, r0, Color;
				MUL oCol, r0, MaskMapTexel;
			"
			
			*ifnot_AlphaToCoverage
			"
				MOV oCol.a, MaskMapTexel.a;
			"
		}
		
		*ifnot_MaskMap
		"
			MUL oCol, r0, Color;
		"
	}
	
	*if_AlphaToCoverage
	"
		#-----------------------------------------
		# Fetch diffuse texture for alphatocoverage
		@if dynmip
		TEXDYN oCol.a, TexCoord, texture[2], 2D;	# Sample diffusemap
		@else
		TEX oCol.a, TexCoord, texture[2], 2D;		# Sample diffusemap
		@endif
	"
	
	*endprogram
	"
		END
	"
}
