!!ARBfp1.0
OPTION NV_fragment_program2;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20
#					
#	Author:			Magnus Högdahl
#					
#	Copyright:		Starbreeze AB 2004
#					
#	Comments:
#				SpecNormal = Specular intensity stored in normal map alpha
#\*____________________________________________________________________________________________*/


#-----------------------------------------
#Texture0 = Diffuse Map
#Texture1 = Projection Map
#Texture2 = Normal+Specular map
#Texture3 = Normalization cube Map, Not used in this program

#TexCoord0 = Diffuse/Normal/Specular tex coord
#TexCoord1 = Animated model space pixel position
#TexCoord3 = Interpolated tangent space light vector (IPTSLV)
#TexCoord4 = Interpolated tangent space eye vector (IPTSEV)
#TexCoord7 = ProjMap tex coord

#-----------------------------------------
SHORT OUTPUT oCol = result.color;

ATTRIB vCol = fragment.color;

ATTRIB DiffuseTexCoord = fragment.texcoord[0];
ATTRIB NormalMapTexCoord  = fragment.texcoord[0];
ATTRIB SpecularTexCoord  = fragment.texcoord[0];

ATTRIB PixelPosition = fragment.texcoord[1];
ATTRIB IPTSLV = fragment.texcoord[3];
ATTRIB IPTSEV = fragment.texcoord[4];
ATTRIB ProjMapTexCoord = fragment.texcoord[7];

PARAM LightPosition = program.env[0];	# { X, Y, Z, 0 }
PARAM LightRange = program.env[1];	# { 1.0 / Range, Range, 1.0 / Range^2, Range^2 }
PARAM LightColor = program.env[2];	# { R, G, B, 0 } (0-2 range)
PARAM SpecColor1 = program.env[3];	# { R, G, B, SpecPower } (0-2 range)
PARAM EyePosition = program.env[4];	# { X, Y, Z, 0 }

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = { 0, 0.25, 4, 0 };

TEMP LV;
SHORT TEMP DiffuseTexel;
SHORT TEMP NormalMapTexel;
SHORT TEMP SpecPowerTexel;
SHORT TEMP TSLV;		# Tangent space light vector
SHORT TEMP TSEV;		# Tangent space eye vector
SHORT TEMP Reflection;
SHORT TEMP Attn;

SHORT TEMP r0;
SHORT TEMP r1;

#-----------------------------------------
# Fetch Textures
TEX DiffuseTexel, DiffuseTexCoord, texture[0], 2D;	# Sample diffusemap
TEX NormalMapTexel, NormalMapTexCoord, texture[2], 2D;	# Sample normalmap
MAD NormalMapTexel.rgb, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
NRMH NormalMapTexel.rgb, NormalMapTexel;
NRMH TSLV.rgb, IPTSLV;
NRMH TSEV.rgb, IPTSEV;

#-----------------------------------------
# Attenuation
SUB LV, LightPosition, PixelPosition;
DP3 LV.w, LV, LV;
MUL_SAT LV.w, LV.w, LightRange.z;
ADD Attn.w, const_val.g, -LV.w;
MUL Attn.w, Attn.w, Attn.w;

#-----------------------------------------
# Self shadowing
SUB r0.a, const_val2.y, -TSLV.x;
MUL_SAT r0.a, r0.a, const_val2.z;
MUL Attn.w, Attn.w, r0.a;

#-----------------------------------------
# Calc reflection vector
DP3 r0.a, NormalMapTexel, TSEV;
ADD r0.a, r0.a, r0.a;
MAD Reflection.xyz, NormalMapTexel, r0.a, -TSEV;

#-----------------------------------------
# Diffuse
DP3_SAT r0.rgb, NormalMapTexel, TSLV;
MULH r1.rgb, LightColor, DiffuseTexel;		# Multiply diffusemap with light color
MUL r1.rgb, r1, const_val.b;			# Scale by 2 for correct brightness
MUL r0.rgb, r0, r1;				# Diffuse color * Diffuse dotprod

#-----------------------------------------
# Specular
DP3_SAT r1.x, TSLV, Reflection;
POW SpecPowerTexel, r1.x, SpecColor1.a;
MUL r1.rgb, SpecPowerTexel, DiffuseTexel.a;	# Mul specular with specular map
MADH r0.rgb, SpecColor1, r1, r0;			# Mul specular with specular color, add to final fragment

MUL r0.rgb, r0, Attn.w;				# Multiply final fragment by attenuation+projmap

# Write result
MOV oCol, r0;

END
