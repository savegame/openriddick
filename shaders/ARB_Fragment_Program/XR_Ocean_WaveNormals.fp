/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:

\*____________________________________________________________________________________________*/

*flags
{
}

*generate
{
	*gen 0
}

*program
{
	*do0
	"!!ARBfp1.0
OPTION ARB_draw_buffers;

		
		#-----------------------------------------
		# TexCoord0 = Mapping tex coord
		
		#-----------------------------------------
		
		OUTPUT oCol0 = result.color[0];
#		OUTPUT oCol1 = result.color[1];
#		OUTPUT oCol2 = result.color[2];
		
		ATTRIB tc0 = fragment.texcoord[0];
		PARAM dUV = program.env[0];
		PARAM v2w_r0 = program.env[1];
		PARAM v2w_r1 = program.env[2];
		PARAM v2w_r2 = program.env[3];
		
		TEMP tc;
		TEMP r0;
		TEMP v0;
		TEMP v1;
		TEMP v2;
		TEMP N;

		MOV tc, tc0;
		SUB tc.y, 1.0, tc0.y;
		TEX v0, tc, texture[0], 2D;
		ADD tc.x, tc0.x, dUV.x;
		TEX v1, tc, texture[0], 2D;
		MOV tc.x, tc0.x;
		ADD tc.y, tc0.y, dUV.y;
		SUB tc.y, 1.0, tc.y;
		TEX v2, tc, texture[0], 2D;
		
		SUB v1.xyz, v1, v0;
		SUB v2.xyz, v2, v0;
		XPD N.xyz, v2, v1;
		DP3 v0.w, N, N;
		MAX v0.w, v0.w, 0.001;
		RSQ v0.w, v0.w;
		MUL N.xyz, N, v0.w;
		MOV N.w, 0.0;
		DP4 r0.x, N, v2w_r0;
		DP4 r0.y, N, v2w_r1;
		DP4 r0.z, N, v2w_r2;
		MAD N.xyz, r0, 0.5, 0.5;
		MOV N.w, 1;
		MOV oCol0, N;
		
		END
	"
}