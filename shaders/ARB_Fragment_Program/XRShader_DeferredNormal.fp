!!ARBfp1.0

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


#-----------------------------------------
# Texture0 = Normal map			// Default = 1, 0.5, 0.5
# Texture1 = Diffuse Map		// Default = 1,1,1,1

#-----------------------------------------
# TexCoord0 = Mapping tex coord

#-----------------------------------------

OUTPUT oCol = result.color;

#ATTRIB vCol = fragment.color;

ATTRIB MappingTexCoord = fragment.texcoord[0];
#ATTRIB LFMTexCoord = fragment.texcoord[1];
#ATTRIB PixelPosition = fragment.texcoord[2];
#ATTRIB IPTSEV = fragment.texcoord[3];
#ATTRIB ProjMapTexCoord = fragment.texcoord[4];
#ATTRIB TS2W_Mat_0 = fragment.texcoord[5];
#ATTRIB TS2W_Mat_1 = fragment.texcoord[6];
#ATTRIB TS2W_Mat_2 = fragment.texcoord[7];

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = { 0, 0.25, 4, 0 };

TEMP NormalMapTexel;
TEMP NormalMapTexelW;

#-----------------------------------------
# Fetch Textures
@if dynmip
TEXDYN NormalMapTexel, MappingTexCoord, texture[0], 2D;	# Sample normalmap
@else
TEX NormalMapTexel, MappingTexCoord, texture[0], 2D;	# Sample normalmap
@endif

#-----------------------------------------
# Reconstruct normal from g,b components
MAD NormalMapTexel.rgba, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
SWZ NormalMapTexel.rgb, NormalMapTexel, 0, a, g, 0;
DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;	# r = g^2 + b^2
SUB NormalMapTexel.r, const_val.y, NormalMapTexel.r;	# r = 1 - r;
MAX NormalMapTexel.r, NormalMapTexel.r, 0.00001;
RSQ NormalMapTexel.r, NormalMapTexel.r;			# r = 1/sqrt(r)
RCP NormalMapTexel.r, NormalMapTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

#-----------------------------------------
# Rotate normal into world-space
#DP3 NormalMapTexelW.r, NormalMapTexel, TS2W_Mat_0;			# Transform reflection vector to world space
#DP3 NormalMapTexelW.g, NormalMapTexel, TS2W_Mat_1;
#DP3 NormalMapTexelW.b, NormalMapTexel, TS2W_Mat_2;

# Write result
MAD oCol, NormalMapTexel, const_val.x, const_val.x;
#MOV oCol, 0;


END
