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
PARAM stepcount = program.env[4];
PARAM stepcounthalf = program.env[5];
PARAM dot_vector = program.env[6];
PARAM vec_front_upperleft = program.env[7];
PARAM vec_front_lowerright = program.env[8];
PARAM vec_left_upperleft = program.env[9];
PARAM vec_left_lowerright = program.env[10];
PARAM vec_up_upperleft = program.env[11];
PARAM vec_up_lowerright = program.env[12];
PARAM vec_down_upperleft = program.env[13];
PARAM vec_down_lowerright = program.env[14];
PARAM front_offset = {0, 0, 0, 0};
PARAM up_offset = {64, 16, 0, 0};
PARAM left_offset = {112, 0, 0, 0}; #{96 + 16, 0, 0, 0};	
PARAM down_offset = {128, 0, 0, 0};

TEMP step;
TEMP cur_pos_frame;
TEMP final_result;
TEMP r0, r1;
TEMP tbox;
TEMP box_divider;

MOV final_result, 0;

#------------------------------------------------------------------------------------------------------------
# First do front
ADD box_divider.x, stepcounthalf.x, -1;
ADD box_divider.y, stepcount.x, -1;
RCP box_divider.x, box_divider.x;
RCP box_divider.y, box_divider.y;

ADD tbox, tbox, -tbox;
MOV step.y, 0;
REP stepcount;

  MAD cur_pos_frame, step.y, texdistv_frame, pos_frame;
  
  MOV step.x, 0;
  REP stepcounthalf;

    MAD r1, front_offset.x, texdistu_frame, cur_pos_frame;
    MAD r1, front_offset.y, texdistv_frame, r1;

    # Sample texture
    TEX r0, r1, texture[0], 2D;
    
    # Calculate t interpolator for both X and Y axis
    MUL tbox.xy, step, box_divider;

    # Calculate vector for this pixel
    LRP r1, tbox, vec_front_lowerright, vec_front_upperleft;
    NRM r1, r1;

    # Dot with base vector to get falloff
    DP3 r1, r1, dot_vector;

    # Multiply texture with falloff and add to result
    MAD final_result, r0, r1, final_result;

	ADD step.x, step.x, 1;
	ADD cur_pos_frame, cur_pos_frame, texdistu_frame;

  ENDREP;

  ADD step.y, step.y, 1;
ENDREP;

#------------------------------------------------------------------------------------------------------------
# Do left side

ADD box_divider.z, stepcounthalf.x, -1;
ADD box_divider.y, stepcount.x, -1;
RCP box_divider.z, box_divider.z;
RCP box_divider.y, box_divider.y;

ADD tbox, tbox, -tbox;
MOV step.y, 0;
REP stepcount;

  MAD cur_pos_frame, step.y, texdistv_frame, pos_frame;
  MOV step.z, 0;
  REP stepcounthalf;
  
    MAD r1, left_offset.x, texdistu_frame, cur_pos_frame;
    MAD r1, left_offset.y, texdistv_frame, r1;
    TEX r0, r1, texture[0], 2D;
    
    MUL tbox.yz, step, box_divider;
    
    LRP r1, tbox, vec_left_lowerright, vec_left_upperleft;
    NRM r1, r1;
    
    DP3 r1, r1, dot_vector;
    MAD final_result, r0, r1, final_result;
    
    ADD step.z, step.z, 1;
    ADD cur_pos_frame, cur_pos_frame, texdistu_frame;
  
  ENDREP;
  
  ADD step.y, step.y, 1;
ENDREP;

#------------------------------------------------------------------------------------------------------------
# Do up and down

ADD box_divider.xz, stepcounthalf.x, -1;
RCP box_divider.x, box_divider.x;
RCP box_divider.z, box_divider.z;

ADD tbox, tbox, -tbox;
MOV step.z, 0;
REP stepcounthalf;

  MAD cur_pos_frame, step.z, texdistv_frame, pos_frame;
  MOV step.x, 0;
  REP stepcounthalf;
  
    MUL tbox.xz, step, box_divider;

    MAD r1, up_offset.x, texdistu_frame, cur_pos_frame;
    MAD r1, up_offset.y, texdistv_frame, r1;
    TEX r0, r1, texture[0], 2D;

    LRP r1, tbox, vec_up_lowerright, vec_up_upperleft;
    NRM r1, r1;
    
    DP3 r1, r1, dot_vector;
    MAD final_result, r0, r1, final_result;

    MAD r1, down_offset.x, texdistu_frame, cur_pos_frame;
    MAD r1, down_offset.y, texdistv_frame, r1;
    TEX r0, r1, texture[0], 2D;

    LRP r1, tbox, vec_down_lowerright, vec_down_upperleft;
    NRM r1, r1;
    
    DP3 r1, r1, dot_vector;
    MAD final_result, r0, r1, final_result;
    
    ADD step.x, step.x, 1;
    ADD cur_pos_frame, cur_pos_frame, texdistu_frame;
  
  ENDREP;
  
  ADD step.z, step.z, 1;
ENDREP;

#------------------------------------------------------------------------------------------------------------
MUL oCol, final_result, colscale;


END
