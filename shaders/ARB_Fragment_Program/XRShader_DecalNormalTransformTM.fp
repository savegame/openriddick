!!ARBfp1.0

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::
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
ATTRIB pixelpos = fragment.texcoord[1];
ATTRIB NormalTransformU = fragment.texcoord[2];
ATTRIB NormalTransformV = fragment.texcoord[3];

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = { 0, 0.25, 4, 0 };
PARAM pln = program.env[0];

TEMP NormalMapTexel;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;

#-----------------------------------------
# Fetch Textures

#reconstruct from 2-component
@if dynmip
TEXDYN NormalMapTexel, MappingTexCoord, texture[0], 2D;	# Sample normalmap
@else
TEX NormalMapTexel, MappingTexCoord, texture[0], 2D;	# Sample normalmap
@endif
MOV r0, NormalMapTexel.b;                               # store alpha
MAD NormalMapTexel, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
SWZ NormalMapTexel, NormalMapTexel, 0, a, g, b;
DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;	# r = g^2 + b^2
SUB NormalMapTexel.r, const_val.y, NormalMapTexel.r;	# r = 1 - r;
MAX NormalMapTexel.r, NormalMapTexel.r, 0.00001;
RSQ NormalMapTexel.r, NormalMapTexel.r;			# r = 1/sqrt(r)
RCP NormalMapTexel.r, NormalMapTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

#TEX NormalMapTexel, MappingTexCoord, texture[0], 2D;	# Sample normalmap
#MAD NormalMapTexel.rgb, NormalMapTexel, const_val.z, -const_val.y;

MOV NormalMapTexel.a, r0;
MOV r0.r, NormalMapTexel.r;
SWZ r1, NormalMapTexel, g, b, 0, 0;
SWZ r2, NormalTransformU, g, r, 0, 0;
SWZ r3, NormalTransformV, g, r, 0, 0;
MOV r2.g, -r2.g;
MOV r3.g, -r3.g;
#DP3 r0.g, r1, NormalTransformU;
#DP3 r0.b, r1, NormalTransformV;
DP3 r0.g, r1, r2;
#MOV r0.g, 0;
DP3 r0.b, r1, r3;
#DP3 r0.g, r1, { 1, 0, 0, 0};
#DP3 r0.b, r1, { 0, 1, 0, 0};
#MOV r0, NormalTransformU;
MAD_SAT r0, r0, const_val.x, const_val.x;


# create alpha by distance
DP3 r1, pln, pixelpos;
SUB r1, r1, pln.w;
ABS r1, r1;
SUB_SAT r1, 1.0, r1;
MUL r0.a, r1, NormalMapTexel.a;


#MOV r0.r, 0;

# Write result
MOV oCol, r0;

END
