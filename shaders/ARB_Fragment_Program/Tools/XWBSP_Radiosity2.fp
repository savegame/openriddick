!!ARBfp1.0
OPTION NV_fragment_program2;

#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			Program for CXR_Shader::RenderShading_FP20
#					
#	Author:			Jim Kjellin
#					
#	Copyright:		Starbreeze AB 2005
#
#	History:
#
#\*____________________________________________________________________________________________*/

OUTPUT oCol = result.color;

PARAM colscale = program.env[0];
PARAM pos_frame = program.env[1];
PARAM texdistu_frame = program.env[2];
PARAM texdistv_frame = program.env[3];
PARAM stepcountu = program.env[4];
PARAM stepcountv = program.env[5];
PARAM dot_vector = program.env[6];
PARAM vec_upperleft = program.env[7];
PARAM vec_lowerright = program.env[8];

TEMP step;
TEMP cur_pos_frame;
TEMP final_result;
TEMP r0, r1;
TEMP tbox;
TEMP box_divider;

ADD box_divider.x, stepcountu.x, -1;
ADD box_divider.y, stepcountv.x, -1;
RCP box_divider.x, box_divider.x;
RCP box_divider.y, box_divider.y;

MOV final_result, 0;

MOV step.y, 0;
REP stepcountv;

  MAD cur_pos_frame, step.y, texdistv_frame, pos_frame;
  MOV step.x, 0;
  REP stepcountu;

    # Sample texture
    TEX r0, cur_pos_frame, texture[0], 2D;
    
    # Calculate t interpolator for both X and Y axis
    MUL tbox.xy, step, box_divider;

    # Calculate vector for this pixel
    LRP r1, tbox, vec_lowerright, vec_upperleft;
    NRM r1, r1;

    # Dot with base vector to get falloff
    DP3 r1, r1, dot_vector;

    # Multiply texture with falloff and add to result
    MAD final_result, r0, r1, final_result;
    #ADD final_result, final_result, r1.w;

	ADD step.x, step.x, 1;
	ADD cur_pos_frame, cur_pos_frame, texdistu_frame;

  ENDREP;

  ADD step.y, step.y, 1;
ENDREP;

MUL oCol, final_result, colscale;


END
