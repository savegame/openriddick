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

OUTPUT oCol0 = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];
PARAM dUV = program.env[0];
PARAM UVOfs0 = program.env[1];
PARAM UVOfs1 = program.env[2];
PARAM UVOfs2 = program.env[3];
PARAM UVOfs3 = program.env[4];
PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_diffmul = { 2, 2, 0, 0 };

TEMP r0;
TEMP r1;
TEMP VPVel2D;
TEMP TexScreen;
TEMP TexMV;
TEMP TexMVRef;
TEMP tcmb0;
TEMP tcmb1;
TEMP tcmb2;
TEMP tcmb3;
TEMP TexOut;
TEMP TexMB0;
TEMP TexMB1;
TEMP TexMB2;
TEMP TexMB3;
TEMP TexMV0;
TEMP TexMV1;
TEMP TexMV2;
TEMP TexMV3;
TEMP dUV0;
TEMP dUV1;
TEMP dUV2;
TEMP dUV3;

#-----------------------------------------

TEX TexScreen, tc0, texture[0], 2D;
TEX TexMV, tc1, texture[2], 2D;
TEX TexMVRef, tc0, texture[1], 2D;
#LRP TexMVRef, 0.5, TexMVRef, TexMV;
MAD TexMV, TexMV, const_val.z, -const_val.y;
#SUB TexMV, TexMV, 0.5;

#MAD r1.xy, TexMVRef, const_val.z, -const_val.y;
#MOV r1.z, 0;
#DP3_SAT r1.w, r1, r1;

@if platform_pc
  MOV TexMV.y, -TexMV.y;
@endif

#MUL r0, dUV, 3;
#MUL dUV0, r0, 1;
#MUL dUV1, r0, 2;
#MUL dUV2, r0, 3;
#MUL dUV3, r0, 4;

MAD tcmb0, TexMV, UVOfs0, tc0;
MAD tcmb1, TexMV, UVOfs1, tc0;
MAD tcmb2, TexMV, UVOfs2, tc0;
MAD tcmb3, TexMV, UVOfs3, tc0;
TEX TexMV0, tcmb0, texture[1], 2D;
TEX TexMV1, tcmb1, texture[1], 2D;
TEX TexMV2, tcmb2, texture[1], 2D;
TEX TexMV3, tcmb3, texture[1], 2D;
TEX TexMB0, tcmb0, texture[0], 2D;
TEX TexMB1, tcmb1, texture[0], 2D;
TEX TexMB2, tcmb2, texture[0], 2D;
TEX TexMB3, tcmb3, texture[0], 2D;

#SUB r0.xyz, TexMV0, TexMVRef;
#MAD r1.w, r0.z, 16, -1;
#MUL r1.z, r0.z, 128;
#MOV oCol0, r1.w;
#MOV oCol0, TexMV0;


SUB r0.xyz, TexMV0, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB0, r0.w, TexScreen, TexMB0;

SUB r0.xyz, TexMV1, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB1, r0.w, TexScreen, TexMB1;

SUB r0.xyz, TexMV2, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB2, r0.w, TexScreen, TexMB2;

SUB r0.xyz, TexMV3, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB3, r0.w, TexScreen, TexMB3;

MUL TexOut, TexMB0, 0.80;
MAD TexOut, TexMB1, 0.60, TexOut;
MAD TexOut, TexMB2, 0.40, TexOut;
MAD TexOut, TexMB3, 0.20, TexOut;

MAD tcmb0, -TexMV, UVOfs0, tc0;
MAD tcmb1, -TexMV, UVOfs1, tc0;
MAD tcmb2, -TexMV, UVOfs2, tc0;
MAD tcmb3, -TexMV, UVOfs3, tc0;
TEX TexMV0, tcmb0, texture[1], 2D;
TEX TexMV1, tcmb1, texture[1], 2D;
TEX TexMV2, tcmb2, texture[1], 2D;
TEX TexMV3, tcmb3, texture[1], 2D;
TEX TexMB0, tcmb0, texture[0], 2D;
TEX TexMB1, tcmb1, texture[0], 2D;
TEX TexMB2, tcmb2, texture[0], 2D;
TEX TexMB3, tcmb3, texture[0], 2D;

SUB r0.xyz, TexMV0, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB0, r0.w, TexScreen, TexMB0;

SUB r0.xyz, TexMV1, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB1, r0.w, TexScreen, TexMB1;

SUB r0.xyz, TexMV2, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB2, r0.w, TexScreen, TexMB2;

SUB r0.xyz, TexMV3, TexMVRef;
MOV r0.z, 0;
DP3_SAT r0.w, r0, r0;
SUB_SAT r0.w, 1, r0.w;
#LRP TexMB3, r0.w, TexScreen, TexMB3;

MAD TexOut, TexMB0, 0.80, TexOut;
MAD TexOut, TexMB1, 0.60, TexOut;
MAD TexOut, TexMB2, 0.40, TexOut;
MAD TexOut, TexMB3, 0.20, TexOut;

ADD TexOut, TexOut, TexScreen;
MUL TexOut, TexOut, 0.20;
#MUL r0.xy, TexMV, TexMV;
#ADD r0.x, r0.x, r0.y;
#SUB r0.x, 1, r0.x;
#MUL r0.x, r0.x, r0.x;
#SUB r0.x, 1, r0.x;
#MUL r0.x, r0.x, 0.8;
#LRP TexOut, r0.x, TexOut, TexScreen;

#MOV oCol0, TexScreen;
MOV oCol0, TexOut;
#MOV oCol0, TexMV;

END
