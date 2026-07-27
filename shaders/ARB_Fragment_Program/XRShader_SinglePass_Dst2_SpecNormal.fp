!!ARBfp1.0

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/


#-----------------------------------------
#Texture0 = Diffuse Map
#Texture1 = Projection Map
#Texture2 = Normal+Specular map
#Texture3 = Normalization cube Map

#TexCoord0 = Diffuse/Normal/Specular tex coord
#TexCoord1 = Animated model space pixel position
#TexCoord3 = Interpolated tangent space light vector (IPTSLV)
#TexCoord4 = Interpolated tangent space eye vector (IPTSEV)
#TexCoord7 = ProjMap tex coord

#-----------------------------------------


OUTPUT oCol = result.color;

ATTRIB vCol = fragment.color;

ATTRIB DiffuseTexCoord = fragment.texcoord[0];
ATTRIB NormalMapTexCoord  = fragment.texcoord[0];
ATTRIB SpecularTexCoord  = fragment.texcoord[0];

ATTRIB PixelPosition = fragment.texcoord[1];
ATTRIB IPTSLV = fragment.texcoord[3];
ATTRIB IPTSEV = fragment.texcoord[4];
#ATTRIB ProjMapTexCoord = fragment.texcoord[7];

PARAM LightPosition = program.env[0];	# { X, Y, Z, 0 }
PARAM LightRange = program.env[1];	# { 1.0 / Range, Range, 1.0 / Range^2, Range^2 }
PARAM LightColor = program.env[2];	# { R, G, B, 0 } (0-2 range)
PARAM SpecColor1 = program.env[3];	# { R, G, B, SpecPower } (0-2 range)
#PARAM EyePosition = program.env[4];	# { X, Y, Z, 0 }

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = { 0, 0.25, 4, 0 };

TEMP DiffuseTexel;
TEMP NormalMapTexel;
TEMP TSLV;		# Tangent space light vector
TEMP TSEV;		# Tangent space eye vector
TEMP Reflection;
TEMP r0;
TEMP r1;

#-----------------------------------------
# Fetch Textures
TEX DiffuseTexel, DiffuseTexCoord, texture[0], 2D;	# Sample diffusemap
TEX NormalMapTexel, NormalMapTexCoord, texture[2], 2D;	# Sample normalmap

#TEX TSLV, IPTSLV, texture[3], CUBE;			# Normalize TSLV
#TEX TSEV, IPTSEV, texture[3], CUBE;			# Normalize TSLV
#MAD TSLV.rgb, TSLV, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
#MAD TSEV.rgb, TSEV, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)

#MAD NormalMapTexel.rgb, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
#TEX NormalMapTexel.rgb, NormalMapTexel, texture[3], CUBE;			# Normalize normal
#MAD NormalMapTexel.rgb, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)

#-----------------------------------------
# Attenuation
SUB r1, LightPosition, PixelPosition;
DP3 r1.w, r1, r1;
#DP3 r1.w, IPTSLV, IPTSLV;
MUL_SAT r1.w, r1.w, LightRange.z;
ADD r1.w, const_val.g, -r1.w;
MUL r1.w, r1.w, r1.w;

#-----------------------------------------
# Normalize normal
MAD NormalMapTexel.rgb, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
DP3 r0.a, NormalMapTexel, NormalMapTexel;
RSQ r0.a, r0.a;
MUL NormalMapTexel.rgb, NormalMapTexel, r0.a;

#-----------------------------------------
# Normalize TSLV
DP3 TSLV.a, IPTSLV, IPTSLV;
RSQ TSLV.a, TSLV.a;
MUL TSLV.xyz, IPTSLV, TSLV.a;

#-----------------------------------------
# Normalize TSEV
DP3 TSEV.a, IPTSEV, IPTSEV;
RSQ TSEV.a, TSEV.a;
MUL TSEV.xyz, IPTSEV, TSEV.a;

#-----------------------------------------
# Calc reflection vector
DP3 r0.a, NormalMapTexel, TSEV;
ADD r0.a, r0.a, r0.a;
MAD Reflection.xyz, NormalMapTexel, r0.a, -TSEV;

#-----------------------------------------
# Self shadowing
SUB r0.a, const_val2.y, -TSLV.x;
MUL_SAT r0.a, r0.a, const_val2.z;
MUL r1.w, r1.w, r0.a;

#-----------------------------------------
# Diffuse
MUL r1.rgb, LightColor, DiffuseTexel;		# Multiply diffusemap with light color
MUL r1.rgb, r1, const_val.b;			# Scale by 2 for correct brightness
DP3_SAT r0.rgb, NormalMapTexel, TSLV;
MUL r0.rgb, r0, r1;				# Diffuse color * Diffuse dotprod

#-----------------------------------------
# Specular
DP3_SAT r1.x, TSLV, Reflection;
POW r1.x, r1.x, SpecColor1.a;
MUL r1.rgb, r1.x, NormalMapTexel.a;	# Mul specular with specular map
MAD r0.rgb, SpecColor1, r1, r0;			# Mul specular with specular color, add to final fragment

MUL r0.rgb, r0, r1.w;				# Multiply final fragment by attenuation+projmap

# Write result
MOV oCol, r0;

END
