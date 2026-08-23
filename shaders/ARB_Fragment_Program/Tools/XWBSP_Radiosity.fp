!!ARBfp1.0
OPTION NV_fragment_program2;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20
#					
#	Author:			Henrik Meijer
#					
#	Copyright:		Starbreeze AB 2005
#
#	History:
#
#\*____________________________________________________________________________________________*/

OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM colscale = program.env[0];
PARAM texdistu = program.env[1];
PARAM texdistv = program.env[2];


TEMP t0;
TEMP t1;
TEMP r0;
TEMP r1;
#-------------------------------------------
# calc avg of a 8x8 texel block
#-------------------------------------------
MOV t0, tc0;

MAD t0, texdistv, -3, t0;

ADD r1, r1, -r1;
# --- row 1 --------------------------------
REP 8;
#  MOV t1, t0;
  MAD t1, texdistu, -4, t0;
  REP 8;
    TEX r0, t1, texture[0], 2D;
    ADD r1, r0, r1;
    ADD t1, t1, texdistu;
  ENDREP;
  ADD t0, t0, texdistv;
ENDREP;


MUL oCol, r1, colscale;

END
