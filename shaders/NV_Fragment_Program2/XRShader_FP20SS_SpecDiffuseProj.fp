!!ARBfp1.0
OPTION NV_fragment_program2;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20SS
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
#Texture5 = Shadow mask

#TexCoord0 = Diffuse/Normal/Specular tex coord
#TexCoord1 = Animated model space pixel position
#TexCoord3 = Interpolated tangent space light vector (IPTSLV)
#TexCoord4 = Interpolated tangent space eye vector (IPTSEV)
#TexCoord5 = Shadow mask tex coord
#TexCoord7 = ProjMap tex coord

#-----------------------------------------


OUTPUT oCol = result.color;

ATTRIB vCol = fragment.color;

ATTRIB DiffuseTexCoord = fragment.texcoord[0];
ATTRIB NormalMapTexCoord  = fragment.texcoord[0];
ATTRIB SpecularTexCoord  = fragment.texcoord[0];
ATTRIB ShadowMaskTexCoord  = fragment.texcoord[5];
ATTRIB ProjMapTexCoord = fragment.texcoord[7];

ATTRIB PixelPosition = fragment.texcoord[1];
ATTRIB IPTSLV = fragment.texcoord[3];
ATTRIB IPTSEV = fragment.texcoord[4];

PARAM LightPosition = program.env[0];	# { X, Y, Z, 0 }
PARAM LightRange = program.env[1];	# { 1.0 / Range, Range, 1.0 / Range^2, Range^2 }
PARAM LightColor = program.env[2];	# { R, G, B, 0 } (0-2 range)
PARAM SpecColor1 = program.env[3];	# { R, G, B, SpecPower } (0-2 range)
PARAM PixelUV = program.env[4];

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = { 0, 0.25, 4, 0.125 };
PARAM const_val3 = { 256, 0.02000, 0.125, 0.00001 };


PARAM SMOfs0 = { 0.5, 1.5, 0, 0 };
PARAM SMOfs1 = { 1.5, -0.5, 0, 0 };
PARAM SMOfs2 = { -0.5, -1.5, 0, 0 };
PARAM SMOfs3 = { -1.5, 0.5, 0, 0 };
PARAM SMOfs4 = { 2.5, 1.5, 0, 0 };
PARAM SMOfs5 = { 1.5, -2.5, 0, 0 };
PARAM SMOfs6 = { -2.5, -1.5, 0, 0 };
PARAM SMOfs7 = { -1.5, 2.5, 0, 0 };

PARAM SMOfs8 = { 3.5, 0.5, 0, 0 };
PARAM SMOfs9 = { 0.5, -3.5, 0, 0 };
PARAM SMOfs10 = { -3.5, -0.5, 0, 0 };
PARAM SMOfs11 = { -0.5, 3.5, 0, 0 };
PARAM SMOfs12 = { 3.5, 2.5, 0, 0 };
PARAM SMOfs13 = { 2.5, -3.5, 0, 0 };
PARAM SMOfs14 = { -3.5, -2.5, 0, 0 };
PARAM SMOfs15 = { -2.5, 3.5, 0, 0 };


TEMP LV;
SHORT TEMP Attn;
SHORT TEMP ProjMapTexel;
SHORT TEMP DiffuseTexel;
SHORT TEMP NormalMapTexel;
SHORT TEMP ShadowMaskTexel;
SHORT TEMP DepthStencilTexel;
SHORT TEMP TSLV;		# Tangent space light vector
SHORT TEMP TSEV;		# Tangent space eye vector
SHORT TEMP Reflection;
SHORT TEMP r0;
SHORT TEMP r1;
TEMP smtc0;				# Shadow mask tex coord
TEMP smtc1;
TEMP depth;
TEMP depthref;
TEMP dstexel;
TEMP depthtolerance;
SHORT TEMP sm;
SHORT TEMP smpacked0;
SHORT TEMP smpacked1;
SHORT TEMP smpacked2;
SHORT TEMP smpacked3;

#-----------------------------------------
# Fetch Textures
TEX DiffuseTexel, DiffuseTexCoord, texture[0], 2D;	# Sample diffusemap
TEX NormalMapTexel, NormalMapTexCoord, texture[2], 2D;	# Sample normalmap

#-----------------------------------------
# Calc Shadow mask tex coord
RCP smtc0.w, ShadowMaskTexCoord.w;
MUL smtc0.xy, ShadowMaskTexCoord, smtc0.w;
ADD smtc0.xy, smtc0, 0.000001;

MUL depthtolerance.a, const_val3.y, ShadowMaskTexCoord.w;

TEX ShadowMaskTexel, smtc0, texture[5], 2D;
#ADD smtc1.xy, smtc0, const_val3.w;
TEX dstexel, smtc0, texture[6], 2D;		# Sample depthstencil map
MOV depthref.rgb, dstexel;
MAD depthref.a, dstexel.r, const_val3.x, dstexel.g;

# -----------------------------------------------
MAD smtc1.xy, SMOfs0, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.r, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked0.r, sm.a;

MAD smtc1.xy, SMOfs1, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.g, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked0.g, sm.a;

MAD smtc1.xy, SMOfs2, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.b, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked0.b, sm.a;

MAD smtc1.xy, SMOfs3, PixelUV, smtc0;
TEX smpacked0.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.a, dstexel.r, const_val3.x, dstexel.g;
#MOV sm.a, sm.a;

SUB depth, depth, depthref.a;
MUL_SAT depth, |depth|, depthtolerance.a;
LRP smpacked0, depth, ShadowMaskTexel.a, smpacked0;

# -----------------------------------------------
MAD smtc1.xy, SMOfs4, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.r, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked1.r, sm.a;

MAD smtc1.xy, SMOfs5, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.g, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked1.g, sm.a;

MAD smtc1.xy, SMOfs6, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.b, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked1.b, sm.a;

MAD smtc1.xy, SMOfs7, PixelUV, smtc0;
TEX smpacked1.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.a, dstexel.r, const_val3.x, dstexel.g;
#MOV sm.a, sm.a;

SUB depth, depth, depthref.a;
MUL_SAT depth, |depth|, depthtolerance.a;
LRP smpacked1, depth, ShadowMaskTexel.a, smpacked1;

# -----------------------------------------------
MAD smtc1.xy, SMOfs8, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.r, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked2.r, sm.a;

MAD smtc1.xy, SMOfs9, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.g, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked2.g, sm.a;

MAD smtc1.xy, SMOfs10, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.b, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked2.b, sm.a;

MAD smtc1.xy, SMOfs11, PixelUV, smtc0;
TEX smpacked2.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.a, dstexel.r, const_val3.x, dstexel.g;
#MOV sm.a, sm.a;

SUB depth, depth, depthref.a;
MUL_SAT depth, |depth|, depthtolerance.a;
LRP smpacked2, depth, ShadowMaskTexel.a, smpacked2;

# -----------------------------------------------
MAD smtc1.xy, SMOfs12, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.r, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked3.r, sm.a;

MAD smtc1.xy, SMOfs13, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.g, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked3.g, sm.a;

MAD smtc1.xy, SMOfs14, PixelUV, smtc0;
TEX sm.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.b, dstexel.r, const_val3.x, dstexel.g;
MOV smpacked3.b, sm.a;

MAD smtc1.xy, SMOfs15, PixelUV, smtc0;
TEX smpacked3.a, smtc1, texture[5], 2D;
TEX dstexel, smtc1, texture[6], 2D;
MAD depth.a, dstexel.r, const_val3.x, dstexel.g;
#MOV sm.a, sm.a;

SUB depth, depth, depthref.a;
MUL_SAT depth, |depth|, depthtolerance.a;
LRP smpacked3, depth, ShadowMaskTexel.a, smpacked3;

#-----------------------------------------
ADD smpacked0, smpacked0, smpacked1;
ADD smpacked2, smpacked2, smpacked3;
ADD smpacked0, smpacked0, smpacked2;
DP4 r0.a, smpacked0, 0.125;

ADD_SAT r0.a, r0, ShadowMaskTexel;
SUB ShadowMaskTexel.a, const_val.y, r0.a;

#SUB ShadowMaskTexel.a, const_val.y, ShadowMaskTexel.a;

#-----------------------------------------
# Attenuation
SUB LV, LightPosition, PixelPosition;
DP3 LV.w, LV, LV;
MUL_SAT LV.w, LV.w, LightRange.z;
ADD Attn.w, const_val.g, -LV.w;
MUL Attn.w, Attn.w, Attn.w;

#-----------------------------------------
# Projectionmap
TEX ProjMapTexel, ProjMapTexCoord, texture[1], CUBE;
MUL Attn.w, Attn.w, ProjMapTexel.a;

#-----------------------------------------
# Normalize
MAD NormalMapTexel.rgb, NormalMapTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
NRMH NormalMapTexel.rgb, NormalMapTexel;
NRMH TSLV.rgb, IPTSLV;
NRMH TSEV.rgb, IPTSEV;

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
MUL r1.rgb, r1.x, DiffuseTexel.a;	# Mul specular with specular map
MAD r0.rgb, SpecColor1, r1, r0;			# Mul specular with specular color, add to final fragment

MUL r0.rgb, r0, Attn.w;				# Multiply final fragment by attenuation+projmap

MUL r0.rgb, r0, ShadowMaskTexel.a;
MOV r0.a, const_val2.x;

# Write result
MOV oCol, r0;

END
