!!ARBfp1.0
OPTION ARB_draw_buffers;

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

OUTPUT oCol0 = result.color[0];
OUTPUT oCol1 = result.color[1];
OUTPUT oCol2 = result.color[2];

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

TEMP NormalTexel;
#TEMP NormalTexelW;
TEMP DiffuseTexel;
TEMP SpecularTexel;

#-----------------------------------------
# Fetch Textures
@if dynmip
TEXDYN NormalTexel, MappingTexCoord, texture[0], 2D;
TEXDYN DiffuseTexel, MappingTexCoord, texture[1], 2D;	
TEXDYN SpecularTexel, MappingTexCoord, texture[2], 2D;
@else
TEX NormalTexel, MappingTexCoord, texture[0], 2D;
TEX DiffuseTexel, MappingTexCoord, texture[1], 2D;	
TEX SpecularTexel, MappingTexCoord, texture[2], 2D;
@endif

#-----------------------------------------
# Reconstruct normal from g,b components
MAD NormalTexel.rgba, NormalTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
SWZ NormalTexel.rgb, NormalTexel, 0, a, g, 0;
DP3 NormalTexel.r, NormalTexel, NormalTexel;	# r = g^2 + b^2
SUB NormalTexel.r, const_val.y, NormalTexel.r;	# r = 1 - r;
MAX NormalTexel.r, NormalTexel.r, 0.00001;
RSQ NormalTexel.r, NormalTexel.r;			# r = 1/sqrt(r)
RCP NormalTexel.r, NormalTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

#-----------------------------------------
# Rotate normal into world-space
#DP3 NormalTexelW.r, NormalTexel, TS2W_Mat_0;			# Transform normal to world space
#DP3 NormalTexelW.g, NormalTexel, TS2W_Mat_1;
#DP3 NormalTexelW.b, NormalTexel, TS2W_Mat_2;

# Write result
MAD oCol0, NormalTexel, const_val.x, const_val.x;
MOV oCol1, DiffuseTexel;
MOV oCol2, SpecularTexel;

#MOV oCol0, 1;

END
